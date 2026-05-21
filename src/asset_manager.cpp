#include "asset_manager.hpp"

#include "gpu_context.hpp"
#include "types.hpp"
#include "utils/gltf.hpp"
#include "utils/ktx.hpp"
#include "utils/upload.hpp"
#include <fmt/core.h>
#include <fastgltf/core.hpp>

// Explicit initalization not needed for now
AssetManager asset_manager = {};

namespace
{
    usize hash_path_impl(std::string_view path)
    {
        return std::hash<std::string>{}(std::filesystem::weakly_canonical(path).string());
    }

    void destroy_model_gpu_resources_impl(Model & model)
    {
        gpu.device.destroy_buffer(model.material_buffer);
        gpu.device.destroy_buffer(model.vertex_buffer);
        gpu.device.destroy_buffer(model.index_buffer);
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
    fmt::println("[Assets] Loading {}", path);

    usize const hash = hash_path_impl(path);
    if (auto it = models.find(hash); it != models.end())
    {
        return &it->second;
    }

    std::filesystem::path file_path = path;
    if (!std::filesystem::exists(file_path))
    {
        return std::unexpected(LoadModelError{
            .code = LoadModelError::Code::File_Not_Found,
            .message = fmt::format("'{}': file not found", path),
        });
    }

    fastgltf::Expected<fastgltf::MappedGltfFile> gltf = fastgltf::MappedGltfFile::FromPath(path);
    if (!bool(gltf))
    {
        return std::unexpected(LoadModelError{
            .code = LoadModelError::Code::Failed_To_Load,
            .message = fmt::format("'{}': {}", path, fastgltf::getErrorMessage(gltf.error())),
        });
    }

    constexpr auto   extensions = fastgltf::Extensions::KHR_texture_basisu;
    fastgltf::Parser parser{extensions};

    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                             fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices;
    fastgltf::Expected<fastgltf::Asset> asset = parser.loadGltf(gltf.get(), file_path.parent_path(), options);
    if (!asset)
    {
        return std::unexpected(LoadModelError{
            .code = LoadModelError::Code::Failed_To_Load,
            .message = fmt::format("'{}': {}", path, fastgltf::getErrorMessage(asset.error())),
        });
    }

    Model model = {};

    // All GPU uploads for this model are batched into one session: one submit,
    // one wait, one collect_garbage
    UploadSession session = begin_upload_session();

    // Images
    utils::gltf::BuildImagesResult image_result = utils::gltf::build_images(asset.get(), file_path);
    model.images.resize(image_result.images.size());
    for (usize i = 0; i < image_result.images.size(); ++i)
    {
        auto & img_data = image_result.images[i];
        if (!img_data.data.empty())
        {
            model.images[i] =
                session.create_image(img_data.data.data(), img_data.data.size(), img_data.mip_infos, img_data.info);
        }
    }

    // Materials: one pass straight to GPUMaterial, then upload
    std::vector<GPUMaterial> gpu_materials =
        utils::gltf::build_materials(asset.get(), image_result.image_cache, model.images, model.material_transparent);
    model.material_buffer =
        session.create_buffer(gpu_materials.data(), {
                                                        .size = gpu_materials.size() * sizeof(GPUMaterial),
                                                        .name = "material buffer",
                                                    });

    // Meshes
    utils::gltf::MeshBuildResult mesh_result = utils::gltf::build_meshes(asset.get());
    model.vertex_buffer = session.create_buffer(mesh_result.mesh.vertices.data(),
                                                {
                                                    .size = mesh_result.mesh.vertices.size() * sizeof(Vertex),
                                                    .name = "vertex buffer",
                                                });
    model.index_buffer = session.create_buffer(mesh_result.mesh.indices.data(),
                                               {
                                                   .size = mesh_result.mesh.indices.size() * sizeof(u32),
                                                   .name = "index buffer",
                                               });
    model.sub_meshes = std::move(mesh_result.mesh.sub_meshes);

    model.nodes = utils::gltf::build_nodes(asset.get(), mesh_result.sub_meshes_offsets);

    session.flush();

    models.emplace(hash, std::move(model));
    return &models.at(hash);
}

std::expected<daxa::ImageId, LoadTextureError> AssetManager::load_texture(std::string_view path)
{
    fmt::println("[Assets] Loading {}", path);

    usize const hash = hash_path_impl(path);

    if (auto it = textures.find(hash); it != textures.end())
    {
        return it->second;
    }

    std::filesystem::path file_path = path;

    if (!std::filesystem::exists(file_path))
    {
        return std::unexpected(LoadTextureError{
            .code = LoadTextureError::Code::File_Not_Found,
            .message = fmt::format("'{}': file not found", path),
        });
    }

    std::expected<ImageData, std::string> img_data = utils::ktx::create_from_file(file_path.string().c_str());
    if (!img_data)
    {
        return std::unexpected(LoadTextureError{
            .code = LoadTextureError::Code::Failed_To_Load,
            .message = fmt::format("'{}': {}", path, img_data.error()),
        });
    }

    UploadSession session = begin_upload_session();
    daxa::ImageId image =
        session.create_image(img_data->data.data(), img_data->data.size(), img_data->mip_infos, img_data->info);
    session.flush();

    textures.emplace(hash, image);
    return textures.at(hash);
}
