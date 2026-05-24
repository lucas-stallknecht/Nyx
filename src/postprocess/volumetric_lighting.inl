#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(VolumetricLightingHead)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::SAMPLE, REGULAR_2D, depth_image)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::SAMPLE, REGULAR_2D, shadow_map)
DAXA_TH_IMAGE_ID(COMPUTE_SHADER::WRITE, REGULAR_2D, output_image)
DAXA_DECL_TASK_HEAD_END

struct VolumetricLightingPC
{
    daxa_BufferPtr(GPUGlobals) global_buffer;
    DAXA_TH_BLOB(VolumetricLightingHead, attachments)
};

#if defined(__cplusplus)

#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::ComputePipelineCompileInfo2 volumetric_lighting_pipeline_info()
{
    return {
        .source = daxa::ShaderFile{"postprocess/volumetric_lighting.glsl"},
        .push_constant_size = sizeof(VolumetricLightingPC),
        .name = "volumetric lighting pipeline",
    };
}

inline void volumetric_lighting_callback(daxa::TaskInterface ti, daxa::ComputePipeline const * pipeline,
                                         daxa_b32 const * enabled, daxa::BufferId global_buffer)
{
    if (!(*enabled))
    {
        return;
    }

    auto const &            AT = VolumetricLightingHead::Info::AT;
    daxa::Extent3D          size = ti.info(AT.output_image).value().size;
    daxa::CommandRecorder & cr = ti.recorder;

    cr.set_pipeline(*pipeline);
    cr.push_constant(VolumetricLightingPC{
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
