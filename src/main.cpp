#include "asset_manager.hpp"
#include "pbr/pbr.inl"
#include "window.hpp"
#include "camera.hpp"
#include "gpu_context.hpp"
#include <daxa/utils/task_graph_types.hpp>
#include <daxa/utils/task_graph.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <imgui.h>

int main()
{
    auto window = Window(1920, 1080);
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
                .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"pbr/pbr.glsl"}},
                .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"pbr/pbr.glsl"}},
                .color_attachments = {{.format = gpu.swapchain.get_format()}},
                .depth_test =
                    daxa::DepthTestInfo{
                        .depth_attachment_format = daxa::Format::D32_SFLOAT,
                        .enable_depth_write = true,
                    },
                .push_constant_size = sizeof(DrawPBRPush),
                .name = "pbr pipeline",
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

    loop_task_graph.register_image(t_swapchain_image);
    loop_task_graph.register_image(t_depth_image);
    loop_task_graph.add_task(
        daxa::RasterTask("draw pbr")
            .color_attachment.writes(t_swapchain_image)
            .depth_stencil_attachment.writes(t_depth_image)
            .executes(
                [pbr = pbr_pipeline.get(), color_target = t_swapchain_image.view(), depth_target = t_depth_image.view(),
                 cam = &camera, cam_buffer, sponza, default_sampler](daxa::TaskInterface ti)
                {
                    auto size = ti.info(color_target).value().size;
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
                            continue;

                        auto & mesh = sponza->meshes[node.mesh_idx];
                        cr.set_index_buffer({
                            .buffer = mesh.index_buffer,
                            .index_type = daxa::IndexType::uint32,
                        });
                        DrawPBRPush push = {};
                        push.model_matrix = std::bit_cast<daxa_f32mat4x4>(node.local_transform);
                        push.default_sampler = default_sampler;
                        push.cam_buffer = ti.device.device_address(cam_buffer).value();
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
        auto reloaded_result = gpu.pipeline_manager.reload_all();

        auto & io = ImGui::GetIO();
        f32 dt = io.DeltaTime;

        camera.rotate(dt * window.consume_mouse_delta());
        if (window.pressed_keys[GLFW_KEY_W])
            camera.move_forward(dt);
        if (window.pressed_keys[GLFW_KEY_S])
            camera.move_forward(-dt);
        if (window.pressed_keys[GLFW_KEY_D])
            camera.move_right(dt);
        if (window.pressed_keys[GLFW_KEY_A])
            camera.move_right(-dt);
        if (window.pressed_keys[GLFW_KEY_SPACE])
            camera.move_up(dt);
        if (window.pressed_keys[GLFW_KEY_LEFT_CONTROL])
            camera.move_up(-dt);

        daxa::ImageId new_image = gpu.swapchain.acquire_next_image();
        if (new_image.is_empty())
            continue;
        t_swapchain_image.set_image(new_image);
        loop_task_graph.execute({});

        if (window.swapchain_out_of_date)
        {
            gpu.swapchain.resize();
            window.swapchain_out_of_date = false;
        }
        gpu.device.collect_garbage();
    }
    gpu.device.wait_idle();
    gpu.device.destroy_sampler(default_sampler);
    gpu.device.destroy_image(depth_image);
    gpu.device.destroy_buffer(cam_buffer);
    window.cleanup();
    asset_manager.cleanup();

    return 0;
}
