#include "asset_manager.hpp"

#include "gpu_context.hpp"
#include "types.hpp"
#include "utils/gltf.hpp"
#include "utils/ktx.hpp"
#include "utils/upload.hpp"
#include <bit>
#include <fmt/core.h>
#include <fastgltf/core.hpp>

// Explicit initalization not needed for now
AssetManager asset_manager = {};

namespace
{
    usize hash_path_impl(std::string_view path) { return std::hash<std::string_view>{}(path); }

    void destroy_model_gpu_resources_impl(Model & model)
    {
        gpu.device.destroy_buffer(model.material_buffer);
        for (auto & mesh : model.meshes)
        {
            gpu.device.destroy_buffer(mesh.vertex_buffer);
            gpu.device.destroy_buffer(mesh.index_buffer);
        }
        for (auto & image : model.images)
        {
            gpu.device.destroy_image(image);
        }
    }
} // namespace

void AssetManager::cleanup()
{
    for (auto & [hash, model] : models)
    {
        destroy_model_gpu_resources_impl(model);
    }
    for (auto & [hash, image] : textures)
    {
        gpu.device.destroy_image(image);
    }
    models.clear();
    textures.clear();
}

void AssetManager::unload_model(std::string_view path)
{
    auto it = models.find(hash_path_impl(path));
    if (it == models.end())
    {
        return;
    }

    destroy_model_gpu_resources_impl(it->second);
    models.erase(it);
}

void AssetManager::unload_texture(std::string_view path)
{
    auto it = textures.find(hash_path_impl(path));
    if (it == textures.end())
    {
        return;
    }

    gpu.device.destroy_image(it->second);
    textures.erase(it);
}

std::expected<Model *, LoadModelError> AssetManager::load_model(std::string_view path)
{
    fmt::println("Loading {}", path);

    usize const hash = hash_path_impl(path);
    if (auto it = models.find(hash); it != models.end())
    {
        return &it->second;
    }

    std::filesystem::path file_path = path;
    if (!std::filesystem::exists(file_path))
    {
        fmt::println("Failed to load: {}. File not found", path);
        return std::unexpected(LoadModelError::File_Not_Found);
    }

    fastgltf::Expected<fastgltf::MappedGltfFile> gltf = fastgltf::MappedGltfFile::FromPath(path);
    if (!bool(gltf))
    {
        fmt::println("Failed to open: {}. {}", path, fastgltf::getErrorMessage(gltf.error()));
        return std::unexpected(LoadModelError::Failed_To_Load);
    }

    constexpr auto extensions = fastgltf::Extensions::KHR_texture_basisu;
    fastgltf::Parser parser{extensions};

    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                             fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices;
    fastgltf::Expected<fastgltf::Asset> asset = parser.loadGltf(gltf.get(), file_path.parent_path(), options);
    if (!asset)
    {
        fmt::println("Failed to load gltf: {}", fastgltf::getErrorMessage(asset.error()));
        return std::unexpected(LoadModelError::Failed_To_Load);
    }

    Model model = {};

    // All GPU uploads for this model are batched into one session: one submit,
    // one wait, one collect_garbage
    UploadSession session = begin_upload_session();

    // Images
    utils::gltf::BuildImagesResult image_result = utils::gltf::build_images(asset.get());
    model.images.reserve(image_result.images.size());
    for (auto & img_data : image_result.images)
    {
        model.images.emplace_back(
            session.create_image(img_data.data.data(), img_data.data.size(), img_data.mip_infos, img_data.info));
    }

    // Materials (CPU-side first, then upload the GPU buffer)
    std::vector<MaterialData> materials = utils::gltf::build_materials(asset.get(), image_result.image_cache);
    model.materials.reserve(materials.size());
    for (auto const & mat_data : materials)
    {
        Material cpu_mat = {};
        cpu_mat.base_color = mat_data.base_color;
        cpu_mat.metallic = mat_data.metallic;
        cpu_mat.roughness = mat_data.roughness;
        if (mat_data.base_color_texture)
        {
            cpu_mat.base_color_texture = model.images[mat_data.base_color_texture.value()];
        }
        if (mat_data.normal_texture)
        {
            cpu_mat.normal_texture = model.images[mat_data.normal_texture.value()];
        }
        if (mat_data.metallic_roughness_texture)
        {
            cpu_mat.metallic_roughness_texture = model.images[mat_data.metallic_roughness_texture.value()];
        }
        model.materials.push_back(cpu_mat);
    }
    {
        std::vector<GPUMaterial> gpu_materials;
        gpu_materials.reserve(model.materials.size());
        for (auto const & mat : model.materials)
        {
            GPUMaterial gpu_mat = {};
            gpu_mat.base_color = std::bit_cast<daxa_f32vec3>(mat.base_color);
            gpu_mat.metallic = mat.metallic;
            gpu_mat.roughness = mat.roughness;
            if (!mat.base_color_texture.is_empty())
            {
                gpu_mat.base_color_texture = mat.base_color_texture.default_view();
            }
            if (!mat.normal_texture.is_empty())
            {
                gpu_mat.normal_texture = mat.normal_texture.default_view();
            }
            if (!mat.metallic_roughness_texture.is_empty())
            {
                gpu_mat.metallic_roughness_texture = mat.metallic_roughness_texture.default_view();
            }
            gpu_materials.push_back(gpu_mat);
        }
        model.material_buffer =
            session.create_buffer(gpu_materials.data(), {
                                                            .size = gpu_materials.size() * sizeof(GPUMaterial),
                                                            .name = "material buffer",
                                                        });
    }

    // Meshes
    std::vector<MeshData> meshes = utils::gltf::build_meshes(asset.get());
    model.meshes.reserve(meshes.size());
    for (auto & mesh_data : meshes)
    {
        model.meshes.push_back({
            .vertex_buffer = session.create_buffer(mesh_data.vertices.data(),
                                                   {
                                                       .size = mesh_data.vertices.size() * sizeof(Vertex),
                                                       .name = "vertex buffer",
                                                   }),
            .index_buffer = session.create_buffer(mesh_data.indices.data(),
                                                  {
                                                      .size = mesh_data.indices.size() * sizeof(u32),
                                                      .name = "index buffer",
                                                  }),
            .sub_meshes = std::move(mesh_data.sub_meshes),
        });
    }

    model.nodes = utils::gltf::build_nodes(asset.get());

    // One submit+wait for the entire model — regardless of mesh/texture count.
    session.flush();

    models.emplace(hash, std::move(model));
    return &models.at(hash);
}

std::expected<daxa::ImageId, LoadTextureError> AssetManager::load_texture(std::string_view path)
{
    fmt::println("Loading {}", path);

    usize const hash = hash_path_impl(path);

    if (auto it = textures.find(hash); it != textures.end())
    {
        return it->second;
    }

    std::filesystem::path file_path = path;

    if (!std::filesystem::exists(file_path))
    {
        fmt::println("Failed to load: {}. File not found", path);
        return std::unexpected(LoadTextureError::File_Not_Found);
    }

    std::expected<ImageData, std::string> img_data = utils::ktx::create_from_file(file_path.string().c_str());
    if (!img_data)
    {
        fmt::println("{}: {}", path, img_data.error());
        return std::unexpected(LoadTextureError::Failed_To_Load);
    }

    UploadSession session = begin_upload_session();
    daxa::ImageId image =
        session.create_image(img_data->data.data(), img_data->data.size(), img_data->mip_infos, img_data->info);
    session.flush();

    textures.emplace(hash, image);
    return textures.at(hash);
}
