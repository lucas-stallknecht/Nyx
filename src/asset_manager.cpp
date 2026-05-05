#include "asset_manager.hpp"

#include "gpu_context.hpp"
#include "types.hpp"
#include "utils/gltf.hpp"
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

AssetManager::LoadModelResult AssetManager::load_model(std::string_view path, Handle & out)
{
    fmt::println("Loading {}", path);

    std::filesystem::path file_path = path;

    if (model_cache.contains(file_path.string()))
    {
        out = model_cache.at(file_path.string());
        return LoadModelResult::Success;
    }

    if (!std::filesystem::exists(file_path))
    {
        fmt::println("Failed to load: {}. File not found", path);
        return LoadModelResult::File_Not_Found;
    }

    fastgltf::Expected<fastgltf::MappedGltfFile> gltf = fastgltf::MappedGltfFile::FromPath(path);
    if (!bool(gltf))
    {
        fmt::println("Failed to open: {}. {}", path, fastgltf::getErrorMessage(gltf.error()));
        return LoadModelResult::Failed_To_Load;
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
        return LoadModelResult::Failed_To_Load;
    }
    out = models.insert({});
    Model * model = models.get(out);

    build_gltf_model(model, asset.get());
    model_cache.insert({file_path.string(), out});

    return LoadModelResult::Success;
}
