#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(BlurSSAOHead)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::READ, REGULAR_2D, ssao_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::WRITE, REGULAR_2D, ssao_blur_image)
DAXA_DECL_TASK_HEAD_END

struct BlurSSAOPC
{
    DAXA_TH_BLOB(BlurSSAOHead, attachments)
};

#if defined(__cplusplus)

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::ComputePipelineCompileInfo2 blur_ssao_pipeline_info()
{
    return {
        .source = daxa::ShaderFile{"rendering/ssao_blur.glsl"},
        .push_constant_size = sizeof(BlurSSAOPC),
        .name = "blur ssao pipeline",
    };
}

inline void blur_ssao_callback(daxa::TaskInterface ti, daxa::ComputePipeline const * pipeline)
{
    auto const & AT = BlurSSAOHead::Info::AT;
    daxa::Extent3D size = ti.info(AT.ssao_image).value().size;
    daxa::CommandRecorder & cr = ti.recorder;

    cr.set_pipeline(*pipeline);
    cr.push_constant(BlurSSAOPC{
        .attachments = ti.attachment_shader_blob,
    });
    cr.dispatch({
        .x = size.x / 8u,
        .y = size.y / 8u,
        .z = 1,
    });
}

#endif // __cplusplus
