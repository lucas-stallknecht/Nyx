#pragma once

#include <daxa/daxa.hpp>
#include <vulkan/vulkan.h>
#include <ktx.h>

inline daxa::Format vk_to_daxa_format(ktx_uint32_t fmt)
{
    auto vk_format = static_cast<VkFormat>(fmt);
    switch (vk_format)
    {
    case VK_FORMAT_R8G8B8A8_UNORM: return daxa::Format::R8G8B8A8_UNORM;
    case VK_FORMAT_R8G8B8A8_SRGB: return daxa::Format::R8G8B8A8_SRGB;
    case VK_FORMAT_R8G8B8_UNORM: return daxa::Format::R8G8B8_UNORM;
    case VK_FORMAT_R8G8B8_SRGB: return daxa::Format::R8G8B8_SRGB;
    case VK_FORMAT_R8_UNORM: return daxa::Format::R8_UNORM;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return daxa::Format::R16G16B16A16_SFLOAT;
    case VK_FORMAT_R32G32B32A32_SFLOAT: return daxa::Format::R32G32B32A32_SFLOAT;
    default: return daxa::Format::R8G8B8A8_UNORM; // fallback
    }
}
