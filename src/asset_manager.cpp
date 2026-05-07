#include "asset_manager.hpp"

#include "gpu_context.hpp"
#include "types.hpp"
#include "utils/gltf.hpp"
#include "utils/upload.hpp"
#include <fmt/core.h>
#include <fastgltf/core.hpp>

// Explicit initalization not needed for now
AssetManager asset_manager = {};

void AssetManager::cleanup()
{
    for (usize i = 0; i < models.items_count; i++)
    {
        Model & model = models.items[i];
        if (!model.material_buffer.is_empty())
        {
            gpu.device.destroy_buffer(model.material_buffer);
        }
        for (auto & mesh : model.meshes)
        {
            gpu.device.destroy_buffer(mesh.vertex_buffer);
            gpu.device.destroy_buffer(mesh.index_buffer);
        };
        for (auto & image : model.images)
        {
            gpu.device.destroy_image(image);
        };
    }
    model_cache.clear();
}

std::expected<Handle, LoadModelError> AssetManager::load_model(std::string_view path)
{
    fmt::println("Loading {}", path);

    std::filesystem::path file_path = path;

    if (model_cache.contains(file_path.string()))
    {
        return model_cache.at(file_path.string());
    }

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
    ;
    fastgltf::Expected<fastgltf::Asset> asset = parser.loadGltf(gltf.get(), file_path.parent_path(), options);
    if (!asset)
    {
        fmt::println("Failed to load gltf: {}", fastgltf::getErrorMessage(asset.error()));
        return std::unexpected(LoadModelError::Failed_To_Load);
    }

    Model model = {};

    utils::gltf::BuildImagesResult image_result = utils::gltf::build_images(asset.get());
    model.images.reserve(image_result.images.size());
    for (auto & img : image_result.images)
    {
        model.images.emplace_back(create_and_upload_image(img.data.data(), img.data.size(), img.mip_infos, img.info));
    }

    std::vector<MaterialData> materials = utils::gltf::build_materials(asset.get(), image_result.image_cache);
    model.materials.reserve(materials.size());
    for (auto const & mat : materials)
    {
        model.materials.emplace_back(Material{
            .base_color = mat.base_color,
            .metallic = mat.metallic,
            .roughness = mat.roughness,
            .base_color_texture = model.images[mat.base_color_texture],
            .metallic_roughness_texture = model.images[mat.metallic_roughness_texture],
            .normal_texture = model.images[mat.normal_texture],
        });
    }
    {
        std::vector<GPUMaterial> gpu_materials;
        gpu_materials.reserve(model.materials.size());

        for (auto const & mat : model.materials)
        {
            gpu_materials.emplace_back(GPUMaterial{
                .base_color = std::bit_cast<daxa_f32vec3>(mat.base_color),
                .metallic = mat.metallic,
                .roughness = mat.roughness,
                .base_color_texture = mat.base_color_texture.default_view(),
                .metallic_roughness_texture = mat.metallic_roughness_texture.default_view(),
                .normal_texture = mat.normal_texture.default_view(),
            });
        }
        model.material_buffer =
            create_and_upload_buffer(gpu_materials.data(), {
                                                               .size = gpu_materials.size() * sizeof(GPUMaterial),
                                                               .name = "material buffer",
                                                           });
    }

    std::vector<MeshData> meshes = utils::gltf::build_meshes(asset.get());
    model.meshes.reserve(meshes.size());
    for (auto & mesh : meshes)
    {
        model.meshes.push_back({
            .vertex_buffer = create_and_upload_buffer(mesh.vertices.data(),
                                                      {
                                                          .size = mesh.vertices.size() * sizeof(Vertex),
                                                          .name = "vertex buffer",
                                                      }),
            .index_buffer = create_and_upload_buffer(mesh.indices.data(),
                                                     {
                                                         .size = mesh.indices.size() * sizeof(u32),
                                                         .name = "index buffer",
                                                     }),
            .sub_meshes = std::move(mesh.sub_meshes),
        });
    }

    model.nodes = utils::gltf::build_nodes(asset.get());

    Handle out = models.insert(model);
    model_cache.insert({file_path.string(), out});

    return out;
}
