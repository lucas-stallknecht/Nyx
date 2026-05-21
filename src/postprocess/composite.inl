#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(CompositeHead)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::READ, REGULAR_2D, draw_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::READ, REGULAR_2D, ssr_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::READ, REGULAR_2D, bloom_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::WRITE, REGULAR_2D, output_image)
DAXA_DECL_TASK_HEAD_END

struct CompositePC
{
    daxa_BufferPtr(GPUGlobals) global_buffer;
    DAXA_TH_BLOB(CompositeHead, attachments)
};

#if defined(__cplusplus)

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::ComputePipelineCompileInfo2 composite_pipeline_info()
{
    return {
        .source = daxa::ShaderFile{"postprocess/composite.glsl"},
        .push_constant_size = sizeof(CompositePC),
        .name = "final composition pipeline",
    };
}

inline void composite_callback(daxa::TaskInterface ti, daxa::ComputePipeline const * pipeline,
                               daxa::BufferId global_buffer)
{
    auto const &            AT = CompositeHead::Info::AT;
    daxa::Extent3D          size = ti.info(AT.output_image).value().size;
    daxa::CommandRecorder & cr = ti.recorder;

    cr.set_pipeline(*pipeline);
    cr.push_constant(CompositePC{
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
