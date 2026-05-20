#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

struct PrepassPC
{
    daxa_f32mat4x4 model_matrix;
    daxa_u32       material_idx;
    daxa_BufferPtr(GPUGlobals) global_buffer;
    daxa_BufferPtr(GPUMaterial) material_buffer;
    daxa_BufferPtr(Vertex) vertex_buffer;
};

#if defined(__cplusplus)

#include "../gpu_context.hpp"
#include "../scene.hpp"
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::RasterPipelineCompileInfo2 prepass_pipeline_info()
{
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"raster/prepass.glsl"}},
        .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"raster/prepass.glsl"}},
        .color_attachments = {{.format = daxa::Format::R16G16B16A16_UNORM}},
        .depth_test =
            daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
                .depth_test_compare_op = daxa::CompareOp::GREATER_OR_EQUAL,
            },
        .raster =
            {
                .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
                .front_face_winding = daxa::FrontFaceWinding::COUNTER_CLOCKWISE,
            },
        .push_constant_size = sizeof(PrepassPC),
        .name = "prepass rendering pipeline",
    };
}

inline void prepass_callback(daxa::TaskInterface ti, daxa::RasterPipeline const * pipeline, Scene const ** scene,
                             daxa::TaskImageView depth_target, daxa::TaskImageView color_target,
                             daxa::BufferId global_buffer)
{
    daxa::Extent3D              size = ti.info(depth_target).value().size;
    daxa::RenderCommandRecorder cr = std::move(ti.recorder)
                                         .begin_renderpass({
                                             .color_attachments =
                                                 std::array{
                                                     daxa::RenderAttachmentInfo{
                                                         .image_view = ti.view(color_target),
                                                         .load_op = daxa::AttachmentLoadOp::CLEAR,
                                                         .clear_value = std::array<daxa::f32, 4>{0.0, 0.0, 0.0, 0.0},
                                                     },
                                                 },
                                             .depth_attachment =
                                                 daxa::RenderAttachmentInfo{
                                                     .image_view = ti.view(depth_target),
                                                     .load_op = daxa::AttachmentLoadOp::CLEAR,
                                                     .clear_value = daxa::DepthValue{.depth = 0.0f, .stencil = 0},
                                                 },
                                             .render_area = {.width = size.x, .height = size.y},
                                         });
    cr.set_pipeline(*pipeline);

    PrepassPC push = {
        .global_buffer = ti.device.device_address(global_buffer).value(),
    };

    daxa::BufferId latest_buffer = {};
    for (auto const & draw : (*scene)->opaque_draws)
    {
        if (draw.culled)
        {
            continue;
        }
        if (latest_buffer.is_empty() || latest_buffer != draw.index_buffer)
        {
            cr.set_index_buffer({.buffer = draw.index_buffer, .index_type = daxa::IndexType::uint32});
        }
        push.model_matrix = draw.transform;
        push.vertex_buffer = draw.vertex_buffer;
        push.material_buffer = draw.material_buffer;
        push.material_idx = draw.material_idx;
        cr.push_constant(push);
        cr.draw_indexed({.index_count = draw.index_count, .first_index = draw.first_index});
        gpu.stats.drawcall_count++;
    }

    ti.recorder = std::move(cr).end_renderpass();
}

#endif // __cplusplus
