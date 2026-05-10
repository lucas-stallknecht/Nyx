#include "renderer.hpp"

#include "rendering/shadow_mapping.inl"
#include "rendering/forward.inl"
#include "rendering/draw_swapchain.inl"
#include "gpu_context.hpp"
#include <imgui.h>

void Renderer::init(Window const & window)
{
    imgui_renderer = daxa::ImGuiRenderer({
        .device = gpu.device,
        .format = gpu.swapchain.get_format(),
    });

    init_resources(window);

    shadow_pipeline = gpu.pipeline_manager.add_raster_pipeline2(shadow_mapping_pipeline_info()).value();
    forward_pipeline = gpu.pipeline_manager.add_raster_pipeline2(forward_pipeline_info()).value();
    draw_swapchain_pipeline = gpu.pipeline_manager.add_compute_pipeline2(draw_swapchain_pipeline_info()).value();

    init_task_graphs();
}

void Renderer::init_resources(Window const & window)
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
    t_draw_image = daxa::ExternalTaskImage({
        .image = gpu.device.create_image({
            .format = daxa::Format::R32G32B32A32_SFLOAT,
            .size = {.x = window.width, .y = window.height, .z = 1},
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                     daxa::ImageUsageFlagBits::SHADER_STORAGE,
            .name = "draw image",
        }),
        .name = "task draw image",
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
            .size = {.x = SHADOW_MAP_SIZE, .y = SHADOW_MAP_SIZE, .z = 1},
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
            .name = "shadow depth image",
        }),
        .name = "task shadow depth image",
    });
    global_buffer = gpu.device.create_buffer({
        .size = sizeof(GPUGlobals),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "global rendering buffer",
    });
    camera_buffer = gpu.device.create_buffer({
        .size = sizeof(GPUCamera),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "camera buffer",
    });
    frame_data_buffer = gpu.device.create_buffer({
        .size = sizeof(GPULightInfo),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "light buffer",
    });
    auto * buffer_ptr = gpu.device.buffer_host_address_as<GPUGlobals>(global_buffer).value();
    *buffer_ptr = {
        .default_linear_sampler = default_linear_sampler,
        .shadow_sampler = shadow_sampler,
        .camera_buffer = gpu.device.device_address(camera_buffer).value(),
        .frame_data_buffer = gpu.device.device_address(frame_data_buffer).value(),
    };
}

void Renderer::init_task_graphs()
{
    loop_task_graph = daxa::TaskGraph({
        .device = gpu.device,
        .swapchain = gpu.swapchain,
        .name = "loop",
    });

    loop_task_graph.register_image(gpu.t_swapchain_image);
    loop_task_graph.register_image(t_draw_image);
    loop_task_graph.register_image(t_depth_image);
    loop_task_graph.register_image(t_shadow_depth_image);

    // Since this->scene value changes every frame, we must pass a pointer to it
    loop_task_graph.add_task(daxa::RasterTask("draw shadow depth")
                                 .depth_stencil_attachment.writes(t_shadow_depth_image.view())
                                 .executes(shadow_mapping_callback, shadow_pipeline.get(), &scene,
                                           t_shadow_depth_image.view(), global_buffer));
    loop_task_graph.add_task(daxa::RasterTask("draw forward")
                                 .uses_head<ForwardPassHead::Info>()
                                 .head_views({
                                     .color_target = t_draw_image.view(),
                                     .depth_target = t_depth_image.view(),
                                     .shadow_depth_image = t_shadow_depth_image.view(),
                                 })
                                 .executes(forward_callback, forward_pipeline.get(), &scene, global_buffer));
    loop_task_graph.add_task(daxa::ComputeTask("draw to swapchain")
                                 .uses_head<DrawSwapchainHead::Info>()
                                 .head_views({
                                     .draw_image = t_draw_image.view(),
                                     .swapchain_image = gpu.t_swapchain_image.view(),
                                 })
                                 .executes(draw_swapchain_callback, draw_swapchain_pipeline.get(), global_buffer));
    loop_task_graph.add_task(daxa::RasterTask("draw GUI")
                                 .color_attachment.writes(gpu.t_swapchain_image)
                                 .executes(
                                     [this, cv = gpu.t_swapchain_image.view()](daxa::TaskInterface ti)
                                     {
                                         ImGui::Render();
                                         daxa::Extent3D size = ti.info(cv).value().size;
                                         imgui_renderer.record_commands(daxa::ImGuiRecordCommandsInfo{
                                             .draw_data = ImGui::GetDrawData(),
                                             .recorder = ti.recorder,
                                             .target_image = ti.id(cv),
                                             .size_x = size.x,
                                             .size_y = size.y,
                                         });
                                     }));

    loop_task_graph.submit({});
    loop_task_graph.present({});
    loop_task_graph.complete({});
}

void Renderer::render(FrameUniforms const & uniforms, Scene const & s)
{
    daxa::ImageId new_image = gpu.swapchain.acquire_next_image();
    if (new_image.is_empty())
    {
        return;
    }
    gpu.t_swapchain_image.set_image(new_image);

    // Update internal scene lookup
    scene = &s;
    *gpu.device.buffer_host_address_as<GPUCamera>(camera_buffer).value() = uniforms.camera;
    *gpu.device.buffer_host_address_as<GPUFrameData>(frame_data_buffer).value() = uniforms.frame_data;
    loop_task_graph.execute({});
}

void Renderer::resize_resources(Window const & window)
{
    gpu.device.destroy_image(t_depth_image.info().image);
    t_depth_image.set_image(gpu.device.create_image({
        .format = daxa::Format::D32_SFLOAT,
        .size = {.x = window.width, .y = window.height, .z = 1},
        .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT,
        .name = "depth image",
    }));
    gpu.device.destroy_image(t_draw_image.info().image);
    t_draw_image.set_image(gpu.device.create_image({
        .format = daxa::Format::R32G32B32A32_SFLOAT,
        .size = {.x = window.width, .y = window.height, .z = 1},
        .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                 daxa::ImageUsageFlagBits::SHADER_STORAGE,
        .name = "draw image",
    }));
}

void Renderer::cleanup() const
{
    gpu.device.destroy_sampler(default_linear_sampler);
    gpu.device.destroy_sampler(shadow_sampler);
    gpu.device.destroy_buffer(camera_buffer);
    gpu.device.destroy_buffer(frame_data_buffer);
    gpu.device.destroy_buffer(global_buffer);
    gpu.device.destroy_image(t_depth_image.info().image);
    gpu.device.destroy_image(t_shadow_depth_image.info().image);
    gpu.device.destroy_image(t_draw_image.info().image);
}
