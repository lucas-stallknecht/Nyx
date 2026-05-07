#pragma once

#include "model.hpp"
#include "types.hpp"
#include "../gpu_context.hpp"
#include <daxa/daxa.hpp>
#include <fmt/core.h>

inline void upload_buffer(daxa::BufferId buffer, void const * data, usize size)
{
    daxa::Device device = gpu.device;
    daxa::CommandRecorder cr = device.create_command_recorder({});

    cr.pipeline_barrier({
        .dst_access = daxa::AccessConsts::TRANSFER_WRITE,
    });

    daxa::BufferId staging_buffer = device.create_buffer({
        .size = size,
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "staging buffer",
    });
    cr.destroy_buffer_deferred(staging_buffer);

    void * mapped = device.buffer_host_address(staging_buffer).value();
    std::memcpy(mapped, data, size);
    cr.copy_buffer_to_buffer({
        .src_buffer = staging_buffer,
        .dst_buffer = buffer,
        .size = size,
    });

    cr.pipeline_barrier({
        .src_access = daxa::AccessConsts::TRANSFER_WRITE,
        .dst_access = daxa::AccessConsts::READ,
    });

    device.wait_on_submit({
        daxa::QUEUE_MAIN,
        device.submit_commands({
            .command_lists = std::array{cr.complete_current_commands()},
        }),
    });
    device.collect_garbage();
}

inline daxa::BufferId create_and_upload_buffer(void const * data, daxa::BufferInfo const & buffer_info)
{
    daxa::BufferId buffer = gpu.device.create_buffer(buffer_info);
    upload_buffer(buffer, data, buffer_info.size);

    return buffer;
}

inline void upload_image(daxa::ImageId image, void const * data, usize size, std::span<ImageMipInfo> mip_infos)
{
    daxa::Device device = gpu.device;
    daxa::CommandRecorder cr = device.create_command_recorder({});

    cr.pipeline_image_barrier({
        .dst_access = daxa::AccessConsts::TRANSFER_WRITE,
        .image = image,
        .layout_operation = daxa::ImageLayoutOperation::TO_GENERAL,
    });

    daxa::BufferId staging_buffer = device.create_buffer({
        .size = size,
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "image staging buffer",
    });
    cr.destroy_buffer_deferred(staging_buffer);

    void * mapped = device.buffer_host_address(staging_buffer).value();
    std::memcpy(mapped, data, size);

    for (u32 i = 0; i < mip_infos.size(); i++)
    {
        cr.copy_buffer_to_image({
            .src_buffer = staging_buffer,
            .buffer_offset = mip_infos[i].offset,
            .dst_image = image,
            .image_slice = {.mip_level = i},
            .image_extent = mip_infos[i].extent,
        });
    }

    cr.pipeline_image_barrier({
        .src_access = daxa::AccessConsts::TRANSFER_WRITE,
        .dst_access = daxa::AccessConsts::READ,
        .image = image,
    });
    device.wait_on_submit({
        daxa::QUEUE_MAIN,
        device.submit_commands({
            .command_lists = std::array{cr.complete_current_commands()},
        }),
    });
    device.collect_garbage();
}

inline daxa::ImageId create_and_upload_image(void const * data, usize size, std::span<ImageMipInfo> mip_infos,
                                             daxa::ImageInfo const & image_info)
{
    daxa::ImageId image = gpu.device.create_image(image_info);
    upload_image(image, data, size, mip_infos);

    return image;
}
