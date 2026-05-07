#pragma once

#include "model.hpp"
#include <fastgltf/core.hpp>

namespace utils::gltf
{

    using LocalImageCache = std::unordered_map<fastgltf::Image const *, u32>;
    struct BuildImagesResult
    {
        std::vector<ImageData> images = {};
        LocalImageCache image_cache = {};
    };

    std::vector<MeshData> build_meshes(fastgltf::Asset /* const */ & asset);
    BuildImagesResult build_images(fastgltf::Asset /* const */ & asset);
    std::vector<MaterialData> build_materials(fastgltf::Asset & asset, LocalImageCache const & image_cache);
    std::vector<Node> build_nodes(fastgltf::Asset /* const */ & asset);

}; // namespace utils::gltf
