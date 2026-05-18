#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

#define SSAO_N_SAMPLES 64
#define SSAO_NOISE_DIM 4
#define SSAO_N_ROTATIONS (SSAO_NOISE_DIM * SSAO_NOISE_DIM)

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(SSAOHead)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::SAMPLE, REGULAR_2D, depth_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::SAMPLE, REGULAR_2D, slim_gbuffer)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::WRITE, REGULAR_2D, ssao_image)
DAXA_DECL_TASK_HEAD_END

struct SSAOPC
{
    daxa_BufferPtr(GPUGlobals) global_buffer;
    daxa_ImageViewId noise_image;
    daxa_SamplerId noise_sampler;
    daxa_BufferPtr(vec3) kernel_buffer;
    DAXA_TH_BLOB(SSAOHead, attachments)
};

#if defined(__cplusplus)

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::ComputePipelineCompileInfo2 ssao_pipeline_info()
{
    return {
        .source = daxa::ShaderFile{"postprocess/ssao.glsl"},
        .push_constant_size = sizeof(SSAOPC),
        .name = "ssao pipeline",
    };
}

inline void ssao_callback(daxa::TaskInterface ti, daxa::ComputePipeline const * pipeline, daxa_b32 const * enabled,
                          daxa::BufferId global_buffer, daxa::BufferId kernel_buffer, daxa::ImageId noise_image,
                          daxa::SamplerId noise_sampler)
{
    if (!(*enabled))
    {
        return;
    }

    auto const & AT = SSAOHead::Info::AT;
    daxa::Extent3D size = ti.info(AT.depth_image).value().size;
    daxa::CommandRecorder & cr = ti.recorder;

    cr.set_pipeline(*pipeline);
    cr.push_constant(SSAOPC{
        .global_buffer = ti.device.device_address(global_buffer).value(),
        .noise_image = noise_image.default_view(),
        .noise_sampler = noise_sampler,
        .kernel_buffer = ti.device.device_address(kernel_buffer).value(),
        .attachments = ti.attachment_shader_blob,
    });
    cr.dispatch({
        .x = size.x / 8u,
        .y = size.y / 8u,
        .z = 1,
    });
}

#endif // __cplusplus
