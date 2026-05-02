#pragma once

#include "model.hpp"
#include "utils/handle_map.hpp"
#include <daxa/daxa.hpp>

struct Texture
{
    Handle handle;
    daxa::ImageId image;
};

struct AssetManager
{
    static constexpr usize max_models = 100;

    HandleMap<Model, max_models> models = {};
    std::unordered_map<std::string, Handle> model_cache = {};

    void cleanup();

    enum class LoadModelResult
    {
        Success,
        File_Not_Found,
        Failed_To_Load,
    };
    LoadModelResult load_model(std::string_view path, Handle & out);
};

extern AssetManager asset_manager;
