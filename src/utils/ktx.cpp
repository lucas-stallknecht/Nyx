#include "ktx.hpp"

#include <fmt/format.h>

namespace
{

    inline daxa::Format vk_to_daxa_format_impl(ktx_uint32_t fmt)
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

    std::expected<ImageData, std::string> ktx_to_image_data_impl(ktxTexture2 * texture)
    {
        if (ktxTexture2_NeedsTranscoding(texture))
        {
            KTX_error_code transcode_result = ktxTexture2_TranscodeBasis(texture, KTX_TTF_RGBA32, 0);
            if (transcode_result != KTX_SUCCESS)
            {
                ktxTexture2_Destroy(texture);
                return std::unexpected(
                    fmt::format("Failed to transcode texture: {}", ktxErrorString(transcode_result)));
            }
        }

        ImageData image = {};
        image.mip_infos.reserve(texture->numLevels);
        for (u32 i = 0; i < texture->numLevels; ++i)
        {
            ktx_size_t mip_offset = 0;
            KTX_error_code ret = ktxTexture2_GetImageOffset(texture, i, 0, 0, &mip_offset);
            if (ret != KTX_SUCCESS)
            {
                return std::unexpected(
                    fmt::format("Failed to get image offset for mip {}: {}", i, ktxErrorString(ret)));
            }

            image.mip_infos.emplace_back(ImageMipInfo{
                .offset = mip_offset,
                .extent =
                    {
                        .x = texture->baseWidth >> i,
                        .y = texture->baseHeight >> i,
                        .z = 1,
                    },
            });
        }

        image.data.resize(texture->dataSize);
        memcpy(image.data.data(), texture->pData, texture->dataSize);

        image.info = {
            .dimensions = 2,
            .format = vk_to_daxa_format_impl(texture->vkFormat),
            .size = {texture->baseWidth, texture->baseHeight, 1},
            .mip_level_count = texture->numLevels,
            .array_layer_count = 1,
            .sample_count = 1,
            .usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
            .name = "texture",
        };

        ktxTexture2_Destroy(texture);

        return image;
    }

} // namespace

namespace utils::ktx
{

    std::expected<ImageData, std::string> create_from_memory(ktx_uint8_t const * bytes, ktx_size_t size)
    {
        ktxTexture2 * texture = nullptr;
        KTX_error_code result =
            ktxTexture2_CreateFromMemory(bytes, size, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

        if (result != KTX_SUCCESS)
        {
            return std::unexpected(fmt::format("Failed to load KTX texture: {}", ktxErrorString(result)));
        }
        return ktx_to_image_data_impl(texture);
    }

    std::expected<ImageData, std::string> create_from_file(char const * filename)
    {
        ktxTexture2 * texture = nullptr;
        KTX_error_code result =
            ktxTexture2_CreateFromNamedFile(filename, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &texture);

        if (result != KTX_SUCCESS)
        {
            return std::unexpected(fmt::format("Failed to load KTX texture: {} {}", filename, ktxErrorString(result)));
        }
        return ktx_to_image_data_impl(texture);
    }
} // namespace utils::ktx
