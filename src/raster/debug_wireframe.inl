#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

struct DebugWireframePC
{
    daxa_b32 culled;
    daxa_f32mat4x4 model_matrix;
    daxa_BufferPtr(GPUGlobals) global_buffer;
};

#if defined(__cplusplus)

#include "../scene.hpp"
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>
#include <fmt/core.h>

inline daxa::RasterPipelineCompileInfo2 debug_wireframe_pipeline_info()
{
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"raster/debug_wireframe.glsl"}},
        .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"raster/debug_wireframe.glsl"}},
        .color_attachments =
            {
                {.format = daxa::Format::R32G32B32A32_SFLOAT},
            },
        .raster =
            {
                .primitive_topology = daxa::PrimitiveTopology::LINE_LIST,
                .face_culling = daxa::FaceCullFlagBits::NONE,
                .line_width = 0.2f,
            },
        .push_constant_size = sizeof(DebugWireframePC),
        .name = "debug wireframe pipeline",
    };
}

inline void debug_wireframe_callback(daxa::TaskInterface ti, daxa::RasterPipeline const * pipeline,
                                     Scene const ** scene, daxa::BufferId global_buffer,
                                     daxa::TaskImageView color_target)
{
    if (!(*scene)->draw_aabb)
    {
        return;
    }
    daxa::Extent3D size = ti.info(color_target).value().size;
    daxa::RenderCommandRecorder cr = std::move(ti.recorder)
                                         .begin_renderpass({
                                             .color_attachments =
                                                 std::array{
                                                     daxa::RenderAttachmentInfo{
                                                         .image_view = ti.view(color_target),
                                                         .load_op = daxa::AttachmentLoadOp::LOAD,
                                                     },
                                                 },
                                             .render_area = {.width = size.x, .height = size.y},
                                         });

    DebugWireframePC push = {.global_buffer = ti.device.buffer_device_address(global_buffer).value()};
    cr.set_pipeline(*pipeline);
    for (auto const & draw : (*scene)->debug_draws)
    {
        push.culled = draw.culled;
        push.model_matrix = draw.transform;
        cr.push_constant(push);
        cr.draw({.vertex_count = 24});
    }

    ti.recorder = std::move(cr).end_renderpass();
}

#endif // __cplusplus
