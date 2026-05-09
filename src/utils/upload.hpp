#pragma once

#include "model.hpp"
#include "types.hpp"
#include "../gpu_context.hpp"
#include <daxa/daxa.hpp>
#include <fmt/core.h>
#include <span>

// Batches all asset uploads into a single GPU submit+wait pair.
// Usage:
//   UploadSession session = begin_upload_session();
//   daxa::BufferId buf = session.create_buffer(data, info);
//   daxa::ImageId  img = session.create_image(data, size, mips, info);
//   session.flush();
//
// Do NOT call flush() more than once. Do NOT construct UploadSession directly.
struct UploadSession
{
    daxa::CommandRecorder recorder;

    // Record a device-local buffer upload. The staging buffer is destroyed
    // automatically after the command list completes (deferred destruction).
    daxa::BufferId create_buffer(void const * data, daxa::BufferInfo const & info)
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

    // Record a device-local image upload with full mip chain.
    // Each image gets its own layout-transition barriers so they can be
    // interleaved freely with buffer uploads in the same session.
    daxa::ImageId create_image(void const * data, usize size, std::span<ImageMipInfo> mip_infos,
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

        recorder.pipeline_image_barrier({
            .src_access = daxa::AccessConsts::TRANSFER_WRITE,
            .dst_access = daxa::AccessConsts::READ,
            .image = image,
        });

        return image;
    }

    // Submit all recorded commands, block until the GPU is done, and free staging
    // memory. Call exactly once when all uploads in this session are recorded
    void flush()
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
};

// Creates an UploadSession with a fresh command recorder and a global
// TRANSFER_WRITE dst barrier. Use session.create_buffer/create_image to
// record uploads, then call session.flush() exactly once.
inline UploadSession begin_upload_session()
{
    // Aggregate-initialize — CommandRecorder is not default-constructible.
    UploadSession session{gpu.device.create_command_recorder({})};
    session.recorder.pipeline_barrier({.dst_access = daxa::AccessConsts::TRANSFER_WRITE});
    return session;
}
