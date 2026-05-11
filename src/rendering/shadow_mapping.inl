#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>

#define SHADOW_MAP_SIZE 4096

struct ShadowPassPC
{
    daxa_f32mat4x4 model_matrix;
    daxa_BufferPtr(GPUGlobals) global_buffer;
    daxa_BufferPtr(Vertex) vertex_buffer;
};

#if defined(__cplusplus)

#include "../scene.hpp"
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::RasterPipelineCompileInfo2 shadow_mapping_pipeline_info()
{
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"rendering/shadow_mapping.glsl"}},
        .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"rendering/shadow_mapping.glsl"}},
        .depth_test =
            daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
            },
        .raster =
            {
                .face_culling = daxa::FaceCullFlagBits::FRONT_BIT,
                .front_face_winding = daxa::FrontFaceWinding::COUNTER_CLOCKWISE,
                .depth_bias_enable = true,
            },
        .push_constant_size = sizeof(ShadowPassPC),
        .name = "shadow depth pipeline",
    };
}

inline void shadow_mapping_callback(daxa::TaskInterface ti, daxa::RasterPipeline const * pipeline, Scene const ** scene,
                                    daxa::TaskImageView depth_target, daxa::BufferId global_buffer)
{
    daxa::RenderCommandRecorder cr = std::move(ti.recorder)
                                         .begin_renderpass({
                                             .depth_attachment =
                                                 daxa::RenderAttachmentInfo{
                                                     .image_view = ti.view(depth_target),
                                                     .load_op = daxa::AttachmentLoadOp::CLEAR,
                                                     .clear_value = daxa::DepthValue{.depth = 1.0f, .stencil = 0},
                                                 },
                                             .render_area = {.width = SHADOW_MAP_SIZE, .height = SHADOW_MAP_SIZE},
                                         });
    cr.set_pipeline(*pipeline);
    cr.set_depth_bias({.constant_factor = -0.0025f, .slope_factor = 1.75f});

    ShadowPassPC push = {
        .global_buffer = ti.device.device_address(global_buffer).value(),
    };

    for (auto const & draw : (*scene)->opaque_draws)
    {
        cr.set_index_buffer({.buffer = draw.index_buffer, .index_type = daxa::IndexType::uint32});
        push.model_matrix = draw.transform;
        push.vertex_buffer = draw.vertex_buffer;
        cr.push_constant(push);
        cr.draw_indexed({.index_count = draw.index_count, .first_index = draw.first_index});
    }

    ti.recorder = std::move(cr).end_renderpass();
}

#endif // __cplusplus
