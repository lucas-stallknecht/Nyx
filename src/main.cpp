#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "asset_manager.hpp"
#include "rendering/pbr/pbr.inl"
#include "rendering/shadow_mapping/shadow_mapping.inl"
#include "window.hpp"
#include "camera.hpp"
#include "gpu_context.hpp"
#include <daxa/utils/task_graph_types.hpp>
#include <daxa/utils/task_graph.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <imgui.h>

static constexpr usize SHADOW_MAP_SIZE = 4096;
static constexpr vec3 DIR_LIGHT_POSITION = {5.0f, 20.0f, 2.0f};

int main()
{
    Window window = {
        .width = 1920,
        .height = 1080,
    };
    WindowInitResult window_res = window.init();
    if (window_res != WindowInitResult::Success)
    {
        auto int_res = static_cast<int>(window_res);
        fmt::println("Failed to initialize Window: {}", int_res);
        return int_res;
    }

    Camera camera = {};

    gpu.init(window);
    std::shared_ptr<daxa::RasterPipeline> pbr_pipeline =
        gpu.pipeline_manager
            .add_raster_pipeline2({
                .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"rendering/pbr/pbr.glsl"}},
                .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"rendering/pbr/pbr.glsl"}},
                .color_attachments = {{.format = gpu.swapchain.get_format()}},
                .depth_test =
                    daxa::DepthTestInfo{
                        .depth_attachment_format = daxa::Format::D32_SFLOAT,
                        .enable_depth_write = true,
                    },
                .raster = {.face_culling = daxa::FaceCullFlagBits::FRONT_BIT},
                .push_constant_size = sizeof(DrawPBRPush),
                .name = "pbr pipeline",
            })
            .value();
    std::shared_ptr<daxa::RasterPipeline> shadow_depth_pipeline =
        gpu.pipeline_manager
            .add_raster_pipeline2({
                .vertex_shader_info =
                    daxa::ShaderCompileInfo2{.source =
                                                 daxa::ShaderFile{"rendering/shadow_mapping/shadow_mapping.glsl"}},
                .fragment_shader_info =
                    daxa::ShaderCompileInfo2{.source =
                                                 daxa::ShaderFile{"rendering/shadow_mapping/shadow_mapping.glsl"}},
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
                .push_constant_size = sizeof(DrawDirectionalDepthMap),
                .name = "shadow depth pipeline",
            })
            .value();

    Handle sponza_handle = {};
    asset_manager.load_model(std::string(ASSETS_DIR) + "models/sponza-ktx.glb", sponza_handle);
    Model * sponza = asset_manager.models.get(sponza_handle);

    daxa::BufferId cam_buffer = gpu.device.create_buffer({
        .size = sizeof(CameraInfo),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "camera buffer",
    });

    daxa::BufferId light_buffer = gpu.device.create_buffer({
        .size = sizeof(LightInfo),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "light buffer",
    });
    glm::mat4 light_proj = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 50.0f);
    glm::mat4 light_view = glm::lookAt(DIR_LIGHT_POSITION, {}, {0.0f, 1.0f, 0.0f});
    auto * buffer_ptr = gpu.device.buffer_host_address_as<LightInfo>(light_buffer).value();
    *buffer_ptr = {
        .dir_pos = std::bit_cast<daxa_f32vec3>(DIR_LIGHT_POSITION),
        .dir_matrix = std::bit_cast<daxa_f32mat4x4>(light_proj * light_view),
    };

    daxa::SamplerId default_sampler = gpu.device.create_sampler({
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
    daxa::SamplerId shadow_sampler = gpu.device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::LINEAR,
        .mipmap_filter = daxa::Filter::LINEAR,
        .address_mode_u = daxa::SamplerAddressMode::CLAMP_TO_BORDER,
        .address_mode_v = daxa::SamplerAddressMode::CLAMP_TO_BORDER,
        .name = "shadow sampler",
    });

    auto loop_task_graph = daxa::TaskGraph({
        .device = gpu.device,
        .swapchain = gpu.swapchain,
        .name = "loop",
    });
    auto t_swapchain_image = daxa::ExternalTaskImage({
        .is_swapchain_image = true,
        .name = "task swapchain image",
    });

    daxa::ImageId depth_image = gpu.device.create_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {.x = window.width, .y = window.height, .z = 1},
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
        .name = "depth image",
    });
    auto t_depth_image = daxa::ExternalTaskImage({
        .image = depth_image,
        .name = "task depth image",
    });
    daxa::ImageId shadow_depth_map = gpu.device.create_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {.x = SHADOW_MAP_SIZE, .y = SHADOW_MAP_SIZE, .z = 1},
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
        .name = "shadow depth map",
    });
    auto t_shadow_depth_map = daxa::ExternalTaskImage({
        .image = shadow_depth_map,
        .name = "task shadow depth map",
    });

    loop_task_graph.register_image(t_swapchain_image);
    loop_task_graph.register_image(t_depth_image);
    loop_task_graph.register_image(t_shadow_depth_map);
    loop_task_graph.add_task(
        daxa::RasterTask("draw shadow depth map")
            .depth_stencil_attachment.writes(t_shadow_depth_map)
            .executes(
                [pipeline = shadow_depth_pipeline.get(), depth_target = t_shadow_depth_map.view(), light_buffer,
                 sponza](daxa::TaskInterface ti)
                {
                    daxa::RenderCommandRecorder cr =
                        std::move(ti.recorder)
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
                    DrawDirectionalDepthMap push;
                    push.light_buffer = ti.device.device_address(light_buffer).value();
                    for (auto & node : sponza->nodes)
                    {
                        if (node.mesh_idx < -1)
                        {
                            continue;
                        }

                        Mesh & mesh = sponza->meshes[static_cast<u32>(node.mesh_idx)];
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
        daxa::RasterTask("draw pbr")
            .reads(daxa::TaskStages::FRAGMENT_SHADER, t_shadow_depth_map)
            .color_attachment.writes(t_swapchain_image)
            .depth_stencil_attachment.writes(t_depth_image)
            .executes(
                [pbr = pbr_pipeline.get(), color_target = t_swapchain_image.view(), depth_target = t_depth_image.view(),
                 shadow_map = t_shadow_depth_map.view(), cam = &camera, cam_buffer, light_buffer, sponza,
                 default_sampler, shadow_sampler](daxa::TaskInterface ti)
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
                                        .image_view = ti.view(depth_target),
                                        .load_op = daxa::AttachmentLoadOp::CLEAR,
                                        .clear_value = daxa::DepthValue{.depth = 1.0f, .stencil = 0},
                                    },
                                .render_area = {.width = size.x, .height = size.y},
                            });
                    cr.set_pipeline(*pbr);

                    auto aspect_ratio = static_cast<f32>(size.x) / static_cast<f32>(size.y);
                    auto * buffer_ptr = gpu.device.buffer_host_address_as<CameraInfo>(cam_buffer).value();
                    *buffer_ptr = {
                        .proj = std::bit_cast<daxa_f32mat4x4>(cam->get_proj(aspect_ratio)),
                        .view = std::bit_cast<daxa_f32mat4x4>(cam->get_view()),
                    };

                    for (auto & node : sponza->nodes)
                    {
                        if (node.mesh_idx < -1)
                        {
                            continue;
                        }

                        Mesh & mesh = sponza->meshes[static_cast<u32>(node.mesh_idx)];
                        cr.set_index_buffer({
                            .buffer = mesh.index_buffer,
                            .index_type = daxa::IndexType::uint32,
                        });
                        DrawPBRPush push = {};
                        push.shadow_map = ti.view(shadow_map);
                        push.model_matrix = std::bit_cast<daxa_f32mat4x4>(node.local_transform);
                        push.default_sampler = default_sampler;
                        push.shadow_sampler = shadow_sampler;
                        push.cam_buffer = ti.device.device_address(cam_buffer).value();
                        push.light_buffer = ti.device.device_address(light_buffer).value();
                        push.vertex_buffer = ti.device.device_address(mesh.vertex_buffer).value();
                        push.material_buffer = ti.device.device_address(sponza->material_buffer).value();

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

    ImGui::CreateContext();
    while (!window.should_close())
    {
        window.update();
        if (window.minimized)
        {
            continue;
        }
        if (window.swapchain_out_of_date)
        {
            gpu.swapchain.resize();
            window.swapchain_out_of_date = false;
        }

        daxa::PipelineReloadResult reloaded_result = gpu.pipeline_manager.reload_all();
        if (auto reload_err = daxa::get_if<daxa::PipelineReloadError>(&reloaded_result))
        {
            fmt::println("Failed to reload shaders: {}", reload_err->message);
        }
        if (daxa::get_if<daxa::PipelineReloadSuccess>(&reloaded_result))
        {
            fmt::println("Shaders successfuly reloaded");
        }

        ImGuiIO & io = ImGui::GetIO();
        f32 dt = io.DeltaTime;

        camera.rotate(dt * window.consume_mouse_delta());
        if (window.pressed_keys[GLFW_KEY_W])
        {
            camera.move_forward(dt);
        }
        if (window.pressed_keys[GLFW_KEY_S])
        {
            camera.move_forward(-dt);
        }
        if (window.pressed_keys[GLFW_KEY_D])
        {
            camera.move_right(dt);
        }
        if (window.pressed_keys[GLFW_KEY_A])
        {
            camera.move_right(-dt);
        }
        if (window.pressed_keys[GLFW_KEY_SPACE])
        {
            camera.move_up(dt);
        }
        if (window.pressed_keys[GLFW_KEY_LEFT_CONTROL])
        {
            camera.move_up(-dt);
        }

        daxa::ImageId new_image = gpu.swapchain.acquire_next_image();
        if (new_image.is_empty())
        {
            continue;
        }
        t_swapchain_image.set_image(new_image);
        loop_task_graph.execute({});

        gpu.device.collect_garbage();
    }
    gpu.device.wait_idle();
    gpu.device.destroy_sampler(default_sampler);
    gpu.device.destroy_sampler(shadow_sampler);
    gpu.device.destroy_image(depth_image);
    gpu.device.destroy_image(shadow_depth_map);
    gpu.device.destroy_buffer(cam_buffer);
    gpu.device.destroy_buffer(light_buffer);
    window.cleanup();
    asset_manager.cleanup();

    return 0;
}
