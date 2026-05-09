#pragma once

#include "../include/gpu_globals.inl"
#include <daxa/daxa.inl>
#include <daxa/utils/task_graph.inl>

DAXA_DECL_RASTER_TASK_HEAD_BEGIN(ForwardPassHead)
DAXA_TH_IMAGE(COLOR_ATTACHMENT, REGULAR_2D, color_target)
DAXA_TH_IMAGE(DEPTH_ATTACHMENT, REGULAR_2D, depth_target)
DAXA_TH_IMAGE_ID(FRAGMENT_SHADER::READ, REGULAR_2D, shadow_depth_image)
DAXA_DECL_TASK_HEAD_END

struct ForwardPassPC
{
    daxa_f32mat4x4 model_matrix;
    daxa_u32 material_idx;
    daxa_BufferPtr(GPUGlobals) global_buffer;
    daxa_BufferPtr(GPUMaterial) material_buffer;
    daxa_BufferPtr(Vertex) vertex_buffer;
    DAXA_TH_BLOB(ForwardPassHead, attachments)
};

#if defined(__cplusplus)

#include "../gpu_context.hpp"
#include "../scene.hpp"
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph.hpp>

inline daxa::RasterPipelineCompileInfo2 forward_pipeline_info()
{
    return {
        .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"rendering/forward.glsl"}},
        .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"rendering/forward.glsl"}},
        .color_attachments = {{.format = daxa::Format::R32G32B32A32_SFLOAT}},
        .depth_test =
            daxa::DepthTestInfo{
                .depth_attachment_format = daxa::Format::D32_SFLOAT,
                .enable_depth_write = true,
            },
        .raster = {.face_culling = daxa::FaceCullFlagBits::FRONT_BIT},
        .push_constant_size = sizeof(ForwardPassPC),
        .name = "forward rendering pipeline",
    };
}

inline void forward_callback(daxa::TaskInterface ti, daxa::RasterPipeline const * pipeline, Scene const ** scene,
                             daxa::BufferId global_buffer)
{
    auto const & AT = ForwardPassHead::Info::AT;
    daxa::Extent3D size = ti.info(AT.color_target).value().size;
    daxa::RenderCommandRecorder cr =
        std::move(ti.recorder)
            .begin_renderpass({
                .color_attachments =
                    std::array{
                        daxa::RenderAttachmentInfo{
                            .image_view = ti.view(AT.color_target),
                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                            .clear_value = std::array<daxa::f32, 4>{0.463f, 0.706f, 0.80f, 1.0f},
                        },
                    },
                .depth_attachment =
                    daxa::RenderAttachmentInfo{
                        .image_view = ti.view(AT.depth_target),
                        .load_op = daxa::AttachmentLoadOp::CLEAR,
                        .clear_value = daxa::DepthValue{.depth = 1.0f, .stencil = 0},
                    },
                .render_area = {.width = size.x, .height = size.y},
            });
    cr.set_pipeline(*pipeline);

    ForwardPassPC push = {
        .global_buffer = gpu.device.device_address(global_buffer).value(),
        .attachments = ti.attachment_shader_blob,
    };

    for (auto const & draw : (*scene)->opaque_draws)
    {
        cr.set_index_buffer({.buffer = draw.index_buffer, .index_type = daxa::IndexType::uint32});
        push.model_matrix = draw.transform;
        push.vertex_buffer = draw.vertex_buffer;
        push.material_buffer = draw.material_buffer;
        push.material_idx = draw.material_idx;
        cr.push_constant(push);
        cr.draw_indexed({.index_count = draw.index_count, .first_index = draw.first_index});
    }

    ti.recorder = std::move(cr).end_renderpass();
}

#endif // __cplusplus
