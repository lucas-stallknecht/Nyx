#include "upload.hpp"

daxa::BufferId UploadSession::create_buffer(void const * data, daxa::BufferInfo const & info)
{
    daxa::BufferId buffer = gpu.device.create_buffer(info);

    daxa::BufferId staging = gpu.device.create_buffer({
        .size = info.size,
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "staging buffer",
    });
    recorder.destroy_buffer_deferred(staging);

    void * mapped = gpu.device.buffer_host_address(staging).value();
    std::memcpy(mapped, data, info.size);

    recorder.copy_buffer_to_buffer({
        .src_buffer = staging,
        .dst_buffer = buffer,
        .size = info.size,
    });

    return buffer;
}

daxa::ImageId UploadSession::create_image(void const * data, usize size, std::span<ImageMipInfo> mip_infos,
                                          daxa::ImageInfo const & info)
{
    daxa::ImageId image = gpu.device.create_image(info);

    recorder.pipeline_image_barrier({
        .dst_access = daxa::AccessConsts::TRANSFER_WRITE,
        .image = image,
        .layout_operation = daxa::ImageLayoutOperation::TO_GENERAL,
    });

    daxa::BufferId staging = gpu.device.create_buffer({
        .size = size,
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "image staging buffer",
    });
    recorder.destroy_buffer_deferred(staging);

    void * mapped = gpu.device.buffer_host_address(staging).value();
    std::memcpy(mapped, data, size);
    if (mip_infos.empty())
    {
        recorder.copy_buffer_to_image({
            .src_buffer = staging,
            .buffer_offset = 0,
            .dst_image = image,
            .image_slice = {.mip_level = 0},
            .image_extent = info.size,
        });
    }
    else
    {
        for (u32 i = 0; i < mip_infos.size(); i++)
        {
            recorder.copy_buffer_to_image({
                .src_buffer = staging,
                .buffer_offset = mip_infos[i].offset,
                .dst_image = image,
                .image_slice = {.mip_level = i},
                .image_extent = mip_infos[i].extent,
            });
        }
    }

    recorder.pipeline_image_barrier({
        .src_access = daxa::AccessConsts::TRANSFER_WRITE,
        .dst_access = daxa::AccessConsts::READ,
        .image = image,
    });

    return image;
}

void UploadSession::flush()
{
    // Final barrier covers all buffer copies in this session
    recorder.pipeline_barrier({
        .src_access = daxa::AccessConsts::TRANSFER_WRITE,
        .dst_access = daxa::AccessConsts::READ,
    });

    gpu.device.wait_on_submit({
        daxa::QUEUE_MAIN,
        gpu.device.submit_commands({
            .command_lists = std::array{recorder.complete_current_commands()},
        }),
    });
    gpu.device.collect_garbage();
}
