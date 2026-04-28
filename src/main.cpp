#include "pbr/pbr.inl"
#include "window.hpp"
#include "gpu_context.hpp"
#include <daxa/utils/task_graph_types.hpp>
#include <daxa/utils/task_graph.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>

int main()
{
    Window window(800, 600);
    WindowInitResult window_res = window.init();
    if (window_res != WindowInitResult::Success)
    {
        fmt::println("Failed to initialize Window: {}", static_cast<int>(window_res));
        return window_res;
    }

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
        Vertex{.position = {-0.5f, +0.5f, 0.0f}, .color = {0.329f, 0.173f, 0.612f}},
        Vertex{.position = {+0.5f, +0.5f, 0.0f}, .color = {0.455f, 0.286f, 0.682f}},
        Vertex{.position = {+0.0f, -0.5f, 0.0f}, .color = {0.671f, 0.506f, 0.816f}},
    };

    auto loop_task_graph = daxa::TaskGraph({
        .device = gpu.device,
        .swapchain = gpu.swapchain,
        .name = "loop",
    });
    auto swapchain_image = daxa::ExternalTaskImage({
        .is_swapchain_image = true,
        .name = "task swapchain image",
    });

    auto draw_task =
        daxa::RasterTask("draw pbr")
            .color_attachment.reads_writes(swapchain_image)
            .executes(
                [pbr = pbr_pipeline.get(), view = swapchain_image.view(), vertex_buffer](daxa::TaskInterface ti)
                {
                    auto info = ti.info(view).value();
                    daxa::RenderCommandRecorder cr =
                        std::move(ti.recorder)
                            .begin_renderpass({
                                .color_attachments =
                                    std::array{
                                        daxa::RenderAttachmentInfo{
                                            .image_view = ti.view(view),
                                            .load_op = daxa::AttachmentLoadOp::CLEAR,
                                            .clear_value = std::array<daxa::f32, 4>{0.435f, 0.325f, 0.616f, 1.0f},
                                        },
                                    },
                                .render_area = {.width = info.size.x, .height = info.size.y},
                            });
                    cr.set_pipeline(*pbr);
                    cr.push_constant(DrawPBRPush{
                        .vertex_buffer = ti.device.device_address(vertex_buffer).value(),
                    });
                    cr.draw({.vertex_count = 3});
                    ti.recorder = std::move(cr).end_renderpass();
                });
    loop_task_graph.register_image(swapchain_image);
    loop_task_graph.add_task(draw_task);
    loop_task_graph.submit({});
    loop_task_graph.present({});
    loop_task_graph.complete({});

    while (!window.should_close())
    {
        window.update();

        daxa::ImageId new_image = gpu.swapchain.acquire_next_image();
        if (new_image.is_empty())
        {
            continue;
        }
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
    window.cleanup();
    return 0;
}
