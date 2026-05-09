#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(DrawSwapchainHead)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::READ, REGULAR_2D, draw_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::WRITE, REGULAR_2D, swapchain_image)
DAXA_DECL_TASK_HEAD_END

struct DrawSwapchainPC
{
    daxa_u32vec2 size;
    daxa_BufferPtr(GPUGlobals) global_buffer;
    DAXA_TH_BLOB(DrawSwapchainHead, attachments)
};

#if defined(__cplusplus)

#include "../gpu_context.hpp"
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::ComputePipelineCompileInfo2 draw_swapchain_pipeline_info()
{
    return {
        .source = daxa::ShaderFile{"rendering/draw_swapchain.glsl"},
        .push_constant_size = sizeof(DrawSwapchainPC),
        .name = "draw swapchain pipeline",
    };
}

inline void draw_swapchain_callback(daxa::TaskInterface ti, daxa::ComputePipeline const * pipeline,
                                    daxa::BufferId global_buffer)
{
    auto const & AT = DrawSwapchainHead::Info::AT;
    daxa::Extent3D size = ti.info(AT.swapchain_image).value().size;
    daxa::CommandRecorder & cr = ti.recorder;

    cr.set_pipeline(*pipeline);
    cr.push_constant(DrawSwapchainPC{
        .size = {.x = size.x, .y = size.y},
        .global_buffer = gpu.device.device_address(global_buffer).value(),
        .attachments = ti.attachment_shader_blob,
    });
    cr.dispatch({
        .x = size.x / 8u,
        .y = size.y / 8u,
        .z = 1,
    });
}

#endif // __cplusplus
