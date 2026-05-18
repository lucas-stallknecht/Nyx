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
// Do NOT call flush() more than once. Do NOT construct UploadSession directly
struct UploadSession
{
    daxa::CommandRecorder recorder;

    daxa::BufferId create_buffer(void const * data, daxa::BufferInfo const & info);
    daxa::ImageId  create_image(void const * data, usize size, std::span<ImageMipInfo> mip_infos,
                                daxa::ImageInfo const & info);

    // Submit all recorded commands, block until the GPU is done, and free staging
    // memory. Call exactly once when all uploads in this session are recorded
    void flush();
};

inline UploadSession begin_upload_session()
{
    UploadSession session = {.recorder = gpu.device.create_command_recorder({})};
    session.recorder.pipeline_barrier({.dst_access = daxa::AccessConsts::TRANSFER_WRITE});
    return session;
}
