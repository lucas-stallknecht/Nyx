#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(BrightPartsHead)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::SAMPLE, REGULAR_2D, input_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::WRITE, REGULAR_2D, bright_parts_image)
DAXA_DECL_TASK_HEAD_END

struct BrightPartsPC
{
    daxa_BufferPtr(GPUGlobals) global_buffer;
    DAXA_TH_BLOB(BrightPartsHead, attachments)
};

#if defined(__cplusplus)

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::ComputePipelineCompileInfo2 bright_parts_pipeline_info()
{
    return {
        .source = daxa::ShaderFile{"postprocess/bright_parts.glsl"},
        .push_constant_size = sizeof(BrightPartsPC),
        .name = "bright parts pipeline",
    };
}

inline void bright_parts_callback(daxa::TaskInterface ti, daxa::ComputePipeline const * pipeline,
                                  daxa_b32 const * enabled, daxa::BufferId global_buffer)
{
    if (!(*enabled))
    {
        return;
    }

    auto const &            AT = BrightPartsHead::Info::AT;
    daxa::Extent3D          size = ti.info(AT.input_image).value().size;
    daxa::CommandRecorder & cr = ti.recorder;

    cr.set_pipeline(*pipeline);
    cr.push_constant(BrightPartsPC{
        .global_buffer = ti.device.device_address(global_buffer).value(),
        .attachments = ti.attachment_shader_blob,
    });
    cr.dispatch({
        .x = (size.x + 7u) / 8u,
        .y = (size.y + 7u) / 8u,
        .z = 1,
    });
}

#endif // __cplusplus
