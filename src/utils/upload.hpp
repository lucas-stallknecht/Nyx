#pragma once

#include "types.hpp"
#include "../gpu_context.hpp"
#include <daxa/daxa.hpp>

inline void upload_buffer(daxa::BufferId buffer_id, void const * data, usize size)
{
    daxa::Device device = gpu.device;
    auto cr = device.create_command_recorder({.queue_type = daxa::QueueType::TRANSFER});
    daxa::BufferId staging_buffer_id = device.create_buffer({
        .size = size,
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "staging buffer",
    });
    cr.destroy_buffer_deferred(staging_buffer_id);

    void * mapped = device.buffer_host_address(staging_buffer_id).value();
    std::memcpy(mapped, data, size);
    cr.copy_buffer_to_buffer({
        .src_buffer = staging_buffer_id,
        .dst_buffer = buffer_id,
        .size = size,
    });
    daxa::ExecutableCommandList const cmd_list = cr.complete_current_commands();
    std::array<daxa::ExecutableCommandList, 1> lists = {cmd_list};
    device.submit_commands({
        .queue = daxa::QUEUE_TRANSFER_0,
        .command_lists = lists,
    });
    // TODO: extract this so multiple uploads are done at the same time
    device.wait_idle();
}

inline daxa::BufferId create_and_upload_buffer(void const * data, daxa::BufferInfo const & buffer_info)
{
    daxa::BufferId buffer_id = gpu.device.create_buffer(buffer_info);
    upload_buffer(buffer_id, data, buffer_info.size);

    return buffer_id;
}
