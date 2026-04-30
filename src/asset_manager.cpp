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
    for (usize i = 0; i < asset_manager.models.items_count; i++)
    {
        for (auto & mesh : asset_manager.models.items[i].meshes)
        {
            gpu.device.destroy_buffer(mesh.vertex_buffer);
            gpu.device.destroy_buffer(mesh.index_buffer);
        }
        // Do something
    }
    asset_manager.model_cache.clear();
}

AssetManager::LoadModelResult AssetManager::load_model(std::string_view const & path, Handle & out)
{
    fmt::println("Loading {}", path);

    std::filesystem::path file_path = path;

    if (model_cache.contains(path.data()))
    {
        out = model_cache.at(path.data());
        return LoadModelResult::Success;
    }

    if (!std::filesystem::exists(file_path))
    {
        fmt::println("Failed to load: {}. File not found", path);
        return LoadModelResult::File_Not_Found;
    }

    constexpr auto options = fastgltf::Options::DontRequireValidAssetMember | fastgltf::Options::AllowDouble |
                             fastgltf::Options::LoadExternalBuffers | fastgltf::Options::GenerateMeshIndices;
    ;
    auto gltf = fastgltf::MappedGltfFile::FromPath(path);
    if (!bool(gltf))
    {
        fmt::println("Failed to open: {}. {}", path, fastgltf::getErrorMessage(gltf.error()));
        return LoadModelResult::Failed_To_Load;
    }

    fastgltf::Parser parser{};
    auto asset = parser.loadGltf(gltf.get(), file_path.parent_path(), options);
    if (!asset)
    {
        fmt::println("Failed to load gltf: {}", fastgltf::to_underlying(asset.error()));
        return LoadModelResult::Failed_To_Load;
    }
    out = models.insert({});
    Model * model = models.get(out);

    build_gltf_model(model, asset.get());

    return LoadModelResult::Success;
}
