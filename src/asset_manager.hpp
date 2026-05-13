#pragma once

#include "include/model.hpp"
#include <daxa/daxa.hpp>
#include <expected>
#include <unordered_map>

struct LoadModelError
{
    enum class Code : u8
    {
        File_Not_Found,
        Failed_To_Load
    };
    Code code = {};
    std::string message = {};
};

struct LoadTextureError
{
    enum class Code : u8
    {
        File_Not_Found,
        Failed_To_Load
    };
    Code code = {};
    std::string message = {};
};

struct AssetManager
{
    std::unordered_map<usize, Model> models = {};
    std::unordered_map<usize, daxa::ImageId> textures = {};

    void cleanup();

    // Load an asset from disk and return a stable pointer. Returns the cached pointer
    // immediately if the same path was already loaded
    [[nodiscard]] std::expected<Model *, LoadModelError> load_model(std::string_view path);
    [[nodiscard]] std::expected<daxa::ImageId, LoadTextureError> load_texture(std::string_view path);
    void unload_model(std::string_view path);
    void unload_texture(std::string_view path);
};

extern AssetManager asset_manager;
