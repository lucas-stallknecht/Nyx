#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(BlurHead)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::SAMPLE, REGULAR_2D, input_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::WRITE, REGULAR_2D, blurred_image)
DAXA_DECL_TASK_HEAD_END

struct BlurPC
{
    daxa_BufferPtr(GPUGlobals) global_buffer;
    DAXA_TH_BLOB(BlurHead, attachments)
};

#if defined(__cplusplus)

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::ComputePipelineCompileInfo2 blur_pipeline_info()
{
    return {
        .source = daxa::ShaderFile{"postprocess/blur.glsl"},
        .push_constant_size = sizeof(BlurPC),
        .name = "blur pipeline",
    };
}

inline void blur_callback(daxa::TaskInterface ti, daxa::ComputePipeline const * pipeline, daxa_b32 const * enabled,
                          daxa::BufferId global_buffer)
{
    if (!(*enabled))
    {
        return;
    }

    auto const & AT = BlurHead::Info::AT;
    daxa::Extent3D size = ti.info(AT.input_image).value().size;
    daxa::CommandRecorder & cr = ti.recorder;

    cr.set_pipeline(*pipeline);
    cr.push_constant(BlurPC{
        .global_buffer = ti.device.device_address(global_buffer).value(),
        .attachments = ti.attachment_shader_blob,
    });
    cr.dispatch({
        .x = size.x / 8u,
        .y = size.y / 8u,
        .z = 1,
    });
}

#endif // __cplusplus
