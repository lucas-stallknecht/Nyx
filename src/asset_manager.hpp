#pragma once

#include "model.hpp"
#include "utils/handle_map.hpp"
#include <daxa/daxa.hpp>
#include <expected>

enum class LoadModelError : u8
{
    File_Not_Found,
    Failed_To_Load,
};

enum class LoadTextureError : u8
{
    File_Not_Found,
    Failed_To_Load,
};

struct AssetManager
{
    static constexpr usize max_models = 100;
    static constexpr usize max_textures = 100;

    HandleMap<Model, max_models> models = {};
    std::unordered_map<std::string, Handle> model_cache = {};
    HandleMap<Texture, max_textures> textures = {};
    std::unordered_map<std::string, Handle> texture_cache = {};

    void cleanup();

    [[nodiscard]] std::expected<Handle, LoadModelError> load_model(std::string_view path);
    [[nodiscard]] std::expected<Handle, LoadTextureError> load_texture(std::string_view path);
};

extern AssetManager asset_manager;
