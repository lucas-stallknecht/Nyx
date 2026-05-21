#pragma once

#include "model.hpp"
#include <fastgltf/core.hpp>

namespace utils::gltf
{

    using LocalImageCache = std::unordered_map<fastgltf::Image const *, usize>;
    struct BuildImagesResult
    {
        std::vector<ImageData> images = {};
        LocalImageCache        image_cache = {};
    };
    using LocalSubMeshOffsets = std::vector<std::pair<i32, u32>>; // <offset, count>
    struct MeshBuildResult
    {
        MeshData            mesh = {};
        LocalSubMeshOffsets sub_meshes_offsets = {};
    };

    MeshBuildResult          build_meshes(fastgltf::Asset /* const */ & asset);
    BuildImagesResult        build_images(fastgltf::Asset /* const */ & asset, std::filesystem::path const & gltf_path);
    std::vector<GPUMaterial> build_materials(fastgltf::Asset & asset, LocalImageCache const & image_cache,
                                             std::vector<daxa::ImageId> const & images,
                                             std::vector<bool> &                out_transparent);
    std::vector<Node> build_nodes(fastgltf::Asset /* const */ & asset, LocalSubMeshOffsets const & sub_meshes_offsets);

}; // namespace utils::gltf
