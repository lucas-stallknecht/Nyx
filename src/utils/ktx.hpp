#pragma once

#include "model.hpp"
#include <daxa/daxa.hpp>
#include <expected>
#include <vulkan/vulkan.h>
#include <ktx.h>

namespace utils::ktx
{

    [[nodiscard]] std::expected<ImageData, std::string> create_from_memory(ktx_uint8_t const * bytes, ktx_size_t size);
    [[nodiscard]] std::expected<ImageData, std::string> create_from_file(char const * filename);

} // namespace utils::ktx
