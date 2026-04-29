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
    auto window = Window(800, 600);
    WindowInitResult window_res = window.init();
    if (window_res != WindowInitResult::Success)
    {
        auto int_res = static_cast<int>(window_res);
        fmt::println("Failed to initialize Window: {}", int_res);
        return int_res;
    }

    Camera camera = {};

    gpu_context_init(window);
    std::shared_ptr<daxa::RasterPipeline> pbr_pipeline =
        gpu.pipeline_manager
            .add_raster_pipeline2({
                .vertex_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"pbr/pbr.glsl"}},
                .fragment_shader_info = daxa::ShaderCompileInfo2{.source = daxa::ShaderFile{"pbr/pbr.glsl"}},
                .color_attachments = {{.format = gpu.swapchain.get_format()}},
                .raster = {},
                .push_constant_size = sizeof(DrawPBRPush),
                .name = "pbr pipeline",
            })
            .value();

    daxa::BufferId vertex_buffer = gpu.device.create_buffer({
        .size = sizeof(Vertex) * 3,
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "tringle data",
    });
    auto * buffer_ptr = gpu.device.buffer_host_address_as<std::array<Vertex, 3>>(vertex_buffer).value();
    *buffer_ptr = std::array{
        Vertex{.position = {-0.5f, -0.5f, 0.0f}, .color = {0.329f, 0.173f, 0.612f}},
        Vertex{.position = {+0.5f, -0.5f, 0.0f}, .color = {0.455f, 0.286f, 0.682f}},
        Vertex{.position = {+0.0f, +0.5f, 0.0f}, .color = {0.671f, 0.506f, 0.816f}},
    };

    daxa::BufferId cam_buffer = gpu.device.create_buffer({
        .size = sizeof(CameraInfo),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "camera",
    });

    auto loop_task_graph = daxa::TaskGraph({
        .device = gpu.device,
        .swapchain = gpu.swapchain,
        .name = "loop",
    });
    auto swapchain_image = daxa::ExternalTaskImage({
        .is_swapchain_image = true,
        .name = "task swapchain image",
    });

    loop_task_graph.register_image(swapchain_image);
    loop_task_graph.add_task(
        daxa::RasterTask("draw pbr")
            .color_attachment.writes(swapchain_image)
            .executes(
                [pbr = pbr_pipeline.get(), view = swapchain_image.view(), cam = &camera, cam_buffer,
                 vertex_buffer](daxa::TaskInterface ti)
                {
                    auto size = ti.info(view).value().size;
                    daxa::RenderCommandRecorder cr =
                        std::move(ti.recorder)
                            .begin_renderpass({
                                .color_attachments =
                                    std::array{
                                        daxa::RenderAttachmentInfo{
                                            .image_view = ti.view(view),
                                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                                            .clear_value = std::array<daxa::f32, 4>{0.1f, 0.1f, 0.1f, 1.0f},
                                        },
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

                    cr.push_constant(DrawPBRPush{
                        .cam_buffer = ti.device.device_address(cam_buffer).value(),
                        .vertex_buffer = ti.device.device_address(vertex_buffer).value(),
                    });
                    cr.draw({.vertex_count = 3});
                    ti.recorder = std::move(cr).end_renderpass();
                }));
    loop_task_graph.submit({});
    loop_task_graph.present({});
    loop_task_graph.complete({});

    ImGui::CreateContext();
    while (!window.should_close())
    {
        window.update();

        auto & io = ImGui::GetIO();
        f32 dt = io.DeltaTime;

        camera.rotate(dt * window.consume_mouse_delta());
        if (window.pressed_keys[GLFW_KEY_W])
            camera.move(dt, 0.0f, 0.0f);
        if (window.pressed_keys[GLFW_KEY_S])
            camera.move(-dt, 0.0f, 0.0f);
        if (window.pressed_keys[GLFW_KEY_A])
            camera.move(0.0f, 0.0f, -dt);
        if (window.pressed_keys[GLFW_KEY_D])
            camera.move(0.0f, 0.0f, dt);
        if (window.pressed_keys[GLFW_KEY_SPACE])
            camera.move(0.0f, dt, 0.0f);
        if (window.pressed_keys[GLFW_KEY_LEFT_CONTROL])
            camera.move(0.0f, -dt, 0.0f);

        daxa::ImageId new_image = gpu.swapchain.acquire_next_image();
        if (new_image.is_empty())
            continue;
        swapchain_image.set_image(new_image);
        loop_task_graph.execute({});

        if (window.swapchain_out_of_date)
        {
            gpu.swapchain.resize();
            window.swapchain_out_of_date = false;
        }
        gpu.device.collect_garbage();
    }

    gpu.device.destroy_buffer(vertex_buffer);
    gpu.device.destroy_buffer(cam_buffer);
    window.cleanup();
    return 0;
}
