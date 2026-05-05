#include "renderer.hpp"
#include "forward_rendering/forward_rendering.inl"
#include "forward_rendering/shadow_mapping.inl"
#include "gpu_context.hpp"

#include <utility>

void Renderer::init(Window const & window, RenderDependencies dependencies)
{
    init_pipelines();
    init_resources(window, dependencies);
    init_task_graphs(dependencies);
}

void Renderer::init_pipelines()
{
    shadow_mapping_pipeline =
        gpu.pipeline_manager
            .add_raster_pipeline2({
                .vertex_shader_info =
                    daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"forward_rendering/shadow_mapping.glsl"}},
                .fragment_shader_info =
                    daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"forward_rendering/shadow_mapping.glsl"}},
                .depth_test =
                    daxa::DepthTestInfo{
                        .depth_attachment_format = daxa::Format::D32_SFLOAT,
                        .enable_depth_write = true,
                    },
                .raster =
                    {
                        .face_culling = daxa::FaceCullFlagBits::BACK_BIT,
                        .depth_bias_enable = true,
                    },
                .push_constant_size = sizeof(DrawShadowDepthPC),
                .name = "shadow depth pipeline",
            })
            .value();
    forward_pipeline =
        gpu.pipeline_manager
            .add_raster_pipeline2({
                .vertex_shader_info =
                    daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"forward_rendering/forward_rendering.glsl"}},
                .fragment_shader_info =
                    daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"forward_rendering/forward_rendering.glsl"}},
                .color_attachments = {{.format = gpu.swapchain.get_format()}},
                .depth_test =
                    daxa::DepthTestInfo{
                        .depth_attachment_format = daxa::Format::D32_SFLOAT,
                        .enable_depth_write = true,
                    },
                .raster = {.face_culling = daxa::FaceCullFlagBits::FRONT_BIT},
                .push_constant_size = sizeof(DrawForwardPC),
                .name = "pbr pipeline",
            })
            .value();
}

void Renderer::init_resources(Window const & window, RenderDependencies dependencies)
{
    default_linear_sampler = gpu.device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::LINEAR,
        .mipmap_filter = daxa::Filter::LINEAR,
        .address_mode_u = daxa::SamplerAddressMode::REPEAT,
        .address_mode_v = daxa::SamplerAddressMode::REPEAT,
        .mip_lod_bias = -0.5f,
        .enable_anisotropy = true,
        .max_anisotropy = 8.0f,
        .name = "default linear sampler",
    });
    shadow_sampler = gpu.device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::LINEAR,
        .mipmap_filter = daxa::Filter::LINEAR,
        .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_BORDER,
        .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_BORDER,
        .name = "shadow sampler",
    });
    t_depth_image = daxa::ExternalTaskImage({
        .image = gpu.device.create_image({
            .format = daxa::Format::D32_SFLOAT,
            .size = {.x = window.width, .y = window.height, .z = 1},
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
            .name = "depth image",
        }),
        .name = "task depth image",
    });
    t_shadow_depth_image = daxa::ExternalTaskImage({
        .image = gpu.device.create_image({
            .format = daxa::Format::D32_SFLOAT,
            .size = {.x = SHADOW_IMAGE_SIZE, .y = SHADOW_IMAGE_SIZE, .z = 1},
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
            .name = "shadow depth image",
        }),
        .name = "task shadow depth image",
    });

    global_buffer = gpu.device.create_buffer({
        .size = sizeof(GlobalRenderingBuffer),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "global rendering buffer",
    });
    auto * buffer_ptr = gpu.device.buffer_host_address_as<GlobalRenderingBuffer>(global_buffer).value();
    *buffer_ptr = {
        .default_linear_sampler = default_linear_sampler,
        .shadow_sampler = shadow_sampler,
        .camera_buffer = gpu.device.device_address(dependencies.camera_buffer).value(),
        .light_buffer = gpu.device.device_address(dependencies.light_buffer).value(),
    };
}

void Renderer::init_task_graphs(RenderDependencies dependencies)
{
    loop_task_graph = daxa::TaskGraph({
        .device = gpu.device,
        .swapchain = gpu.swapchain,
        .name = "loop",
    });

    loop_task_graph.register_image(dependencies.color_target);
    loop_task_graph.register_image(t_depth_image);
    loop_task_graph.register_image(t_shadow_depth_image);

    loop_task_graph.add_task(
        daxa::RasterTask("draw shadow depth")
            .depth_stencil_attachment.writes(t_shadow_depth_image)
            .executes(
                [this, model = dependencies.model](daxa::TaskInterface ti)
                {
                    daxa::RenderCommandRecorder cr =
                        std::move(ti.recorder)
                            .begin_renderpass({
                                .depth_attachment =
                                    daxa::RenderAttachmentInfo{
                                        .image_view = ti.view(t_shadow_depth_image.view()),
                                        .load_op = daxa::AttachmentLoadOp::CLEAR,
                                        .clear_value = daxa::DepthValue{.depth = 1.0f, .stencil = 0},
                                    },
                                .render_area = {.width = SHADOW_IMAGE_SIZE, .height = SHADOW_IMAGE_SIZE},
                            });
                    cr.set_pipeline(*shadow_mapping_pipeline.get());

                    DrawShadowDepthPC push;
                    push.global_buffer = ti.device.device_address(global_buffer).value();

                    for (auto & node : model->nodes)
                    {
                        if (node.mesh_idx < 0)
                        {
                            continue;
                        }

                        Mesh & mesh = model->meshes[static_cast<u32>(node.mesh_idx)];
                        cr.set_index_buffer({
                            .buffer = mesh.index_buffer,
                            .index_type = daxa::IndexType::uint32,
                        });
                        push.model_matrix = std::bit_cast<daxa_f32mat4x4>(node.local_transform);
                        push.vertex_buffer = ti.device.device_address(mesh.vertex_buffer).value();
                        cr.push_constant(push);
                        cr.set_depth_bias({
                            .constant_factor = -0.001f,
                            .slope_factor = 1.75f,
                        });

                        for (auto & sub : mesh.sub_meshes)
                        {
                            cr.draw_indexed({
                                .index_count = sub.index_count,
                                .first_index = sub.index_offset,
                            });
                        }
                    }
                    ti.recorder = std::move(cr).end_renderpass();
                }));
    loop_task_graph.add_task(
        daxa::RasterTask("draw forward")
            .reads(daxa::TaskStages::FRAGMENT_SHADER, t_shadow_depth_image)
            .color_attachment.writes(dependencies.color_target)
            .depth_stencil_attachment.writes(t_depth_image)
            .executes(
                [this, color_target = dependencies.color_target.view(),
                 model = dependencies.model](daxa::TaskInterface ti)
                {
                    daxa::Extent3D size = ti.info(color_target).value().size;
                    daxa::RenderCommandRecorder cr =
                        std::move(ti.recorder)
                            .begin_renderpass({
                                .color_attachments =
                                    std::array{
                                        daxa::RenderAttachmentInfo{
                                            .image_view = ti.view(color_target),
                                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                                            .clear_value = std::array<daxa::f32, 4>{0.463f, 0.706f, 0.80f, 1.0},
                                        },
                                    },
                                .depth_attachment =
                                    daxa::RenderAttachmentInfo{
                                        .image_view = ti.view(t_depth_image.view()),
                                        .load_op = daxa::AttachmentLoadOp::CLEAR,
                                        .clear_value = daxa::DepthValue{.depth = 1.0f, .stencil = 0},
                                    },
                                .render_area = {.width = size.x, .height = size.y},
                            });
                    cr.set_pipeline(*forward_pipeline.get());

                    DrawForwardPC push = {};
                    push.shadow_depth_image = ti.view(t_shadow_depth_image.view());
                    push.global_buffer = ti.device.device_address(global_buffer).value();
                    for (auto & node : model->nodes)
                    {
                        if (node.mesh_idx < 0)
                        {
                            continue;
                        }

                        Mesh & mesh = model->meshes[static_cast<u32>(node.mesh_idx)];
                        cr.set_index_buffer({
                            .buffer = mesh.index_buffer,
                            .index_type = daxa::IndexType::uint32,
                        });

                        push.model_matrix = std::bit_cast<daxa_f32mat4x4>(node.local_transform);
                        push.vertex_buffer = ti.device.device_address(mesh.vertex_buffer).value();
                        push.material_buffer = ti.device.device_address(model->material_buffer).value();

                        for (auto & sub : mesh.sub_meshes)
                        {
                            push.material_idx = static_cast<daxa_u32>(sub.material_idx);
                            cr.push_constant(push);
                            cr.draw_indexed({
                                .index_count = sub.index_count,
                                .first_index = sub.index_offset,
                            });
                        }
                    }

                    ti.recorder = std::move(cr).end_renderpass();
                }));

    loop_task_graph.submit({});
    loop_task_graph.present({});
    loop_task_graph.complete({});
}

void Renderer::render() { loop_task_graph.execute({}); }

void Renderer::cleanup()
{
    gpu.device.destroy_sampler(default_linear_sampler);
    gpu.device.destroy_sampler(shadow_sampler);
    gpu.device.destroy_buffer(global_buffer);
    gpu.device.destroy_image(t_depth_image.info().image);
    gpu.device.destroy_image(t_shadow_depth_image.info().image);
}
