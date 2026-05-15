#include "renderer.hpp"

#include "raster/depth_prepass.inl"
#include "raster/shadow_mapping.inl"
#include "raster/forward.inl"
#include "postprocess/ssao.inl"
#include "postprocess/composite.inl"
#include "postprocess/blur.inl"
#include "gpu_context.hpp"
#include "utils/upload.hpp"
#include <fmt/core.h>
#include <imgui.h>
#include <random>

namespace
{
    daxa::ImageInfo make_draw_info(Window const & w)
    {
        return {
            .format = daxa::Format::R32G32B32A32_SFLOAT,
            .size = {.x = w.width, .y = w.height, .z = 1},
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                     daxa::ImageUsageFlagBits::SHADER_STORAGE,
            .name = "draw image",
        };
    }
    daxa::ImageInfo make_depth_info(Window const & w)
    {
        return {
            .format = daxa::Format::D32_SFLOAT,
            .size = {.x = w.width, .y = w.height, .z = 1},
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                     daxa::ImageUsageFlagBits::SHADER_STORAGE,
            .name = "depth image",
        };
    }
    daxa::ImageInfo make_ssao_info(Window const & w)
    {
        return {
            .format = daxa::Format::R16_SFLOAT,
            .size = {.x = w.width, .y = w.height, .z = 1},
            .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE,
            .name = "ssao image",
        };
    }
    daxa::ImageInfo make_ssao_blur_info(Window const & w)
    {
        return {
            .format = daxa::Format::R16_SFLOAT,
            .size = {.x = w.width, .y = w.height, .z = 1},
            .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE,
            .name = "ssao blurred image",
        };
    }
} // namespace

void Renderer::init(Window const & window)
{
    imgui_renderer = daxa::ImGuiRenderer({
        .device = gpu.device,
        .format = gpu.swapchain.get_format(),
    });

    init_resources(window);
    init_ssao();

    fmt::println("[Renderer] Compiling shaders...");

    depth_prepass_pipeline = gpu.pipeline_manager.add_raster_pipeline2(depth_prepass_pipeline_info()).value();
    shadow_pipeline = gpu.pipeline_manager.add_raster_pipeline2(shadow_mapping_pipeline_info()).value();
    ssao_pipeline = gpu.pipeline_manager.add_compute_pipeline2(ssao_pipeline_info()).value();
    blur_pipeline = gpu.pipeline_manager.add_compute_pipeline2(blur_pipeline_info()).value();
    opaque_pipeline = gpu.pipeline_manager.add_raster_pipeline2(opaque_pipeline_info()).value();
    transparent_pipeline = gpu.pipeline_manager.add_raster_pipeline2(transparent_pipeline_info()).value();
    composite_pipeline = gpu.pipeline_manager.add_compute_pipeline2(composite_pipeline_info()).value();

    fmt::println("[Renderer] Shaders ready");

    init_task_graphs();
    task_graph_debug_ui = daxa::TaskGraphDebugUi({
        .device = gpu.device,
        .imgui_renderer = imgui_renderer,
        .buffer_layout_cache_folder = "./tg_dbg_cache",
    });
}

void Renderer::init_resources(Window const & window)
{
    default_linear_sampler = gpu.device.create_sampler({
        .magnification_filter = daxa::Filter::LINEAR,
        .minification_filter = daxa::Filter::LINEAR,
        .mipmap_filter = daxa::Filter::LINEAR,
        .address_mode_u = daxa::SamplerAddressMode::REPEAT,
        .address_mode_v = daxa::SamplerAddressMode::REPEAT,
        .mip_lod_bias = -0.25f,
        .enable_anisotropy = true,
        .max_anisotropy = 8.0f,
        .name = "default linear sampler",
    });
    default_nearest_sampler = gpu.device.create_sampler({
        .magnification_filter = daxa::Filter::NEAREST,
        .minification_filter = daxa::Filter::NEAREST,
        .address_mode_u = daxa::SamplerAddressMode::REPEAT,
        .address_mode_v = daxa::SamplerAddressMode::REPEAT,
        .name = "default nearest sampler",
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
        .image = gpu.device.create_image(make_draw_info(window)),
        .name = "task draw image",
    });
    t_depth_image = daxa::ExternalTaskImage({
        .image = gpu.device.create_image(make_depth_info(window)),
        .name = "task depth image",
    });
    t_shadow_map = daxa::ExternalTaskImage({
        .image = gpu.device.create_image({
            .format = daxa::Format::D32_SFLOAT,
            .size = {.x = SHADOW_MAP_SIZE, .y = SHADOW_MAP_SIZE, .z = 1},
            .usage = daxa::ImageUsageFlagBits::DEPTH_STENCIL_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED,
            .name = "shadow map",
        }),
        .name = "task shadow map",
    });
    t_ssao_image = daxa::ExternalTaskImage({
        .image = gpu.device.create_image(make_ssao_info(window)),
        .name = "task ssao image",
    });
    t_ssao_blurred_image = daxa::ExternalTaskImage({
        .image = gpu.device.create_image(make_ssao_blur_info(window)),
        .name = "task ssao blurred image",
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
        .size = sizeof(GPUFrameData),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "frame data buffer",
    });
    auto * buffer_ptr = gpu.device.buffer_host_address_as<GPUGlobals>(global_buffer).value();
    *buffer_ptr = {
        .default_linear_sampler = default_linear_sampler,
        .default_nearest_sampler = default_nearest_sampler,
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
    loop_task_graph.register_image(t_shadow_map);
    loop_task_graph.register_image(t_ssao_image);
    loop_task_graph.register_image(t_ssao_blurred_image);

    // Since this->scene value changes every frame, we must pass a pointer to it
    loop_task_graph.add_task(daxa::RasterTask("draw depth prepass")
                                 .depth_stencil_attachment.writes(t_depth_image.view())
                                 .executes(depth_prepass_callback, depth_prepass_pipeline.get(), &scene,
                                           t_depth_image.view(), global_buffer));
    loop_task_graph.add_task(daxa::ComputeTask("compute ssao")
                                 .uses_head<SSAOHead::Info>()
                                 .head_views({
                                     .depth_image = t_depth_image.view(),
                                     .ssao_image = t_ssao_image.view(),
                                 })
                                 .executes(ssao_callback, ssao_pipeline.get(), global_buffer, ssao_kernel_buffer,
                                           ssao_noise_image, ssao_noise_sampler));
    loop_task_graph.add_task(daxa::ComputeTask("blur ssao")
                                 .uses_head<BlurHead::Info>()
                                 .head_views({
                                     .input_image = t_ssao_image.view(),
                                     .blurred_image = t_ssao_blurred_image.view(),
                                 })
                                 .executes(blur_callback, blur_pipeline.get(), global_buffer));
    loop_task_graph.add_task(
        daxa::RasterTask("draw shadow depth")
            .depth_stencil_attachment.writes(t_shadow_map.view())
            .executes(shadow_mapping_callback, shadow_pipeline.get(), &scene, t_shadow_map.view(), global_buffer));
    loop_task_graph.add_task(
        daxa::RasterTask("draw forward")
            .uses_head<ForwardPassHead::Info>()
            .head_views({
                .color_target = t_draw_image.view(),
                .depth_target = t_depth_image.view(),
                .shadow_map = t_shadow_map.view(),
                .ssao_image = t_ssao_blurred_image.view(),
            })
            .executes(forward_callback, opaque_pipeline.get(), transparent_pipeline.get(), &scene, global_buffer));
    loop_task_graph.add_task(daxa::ComputeTask("composition")
                                 .uses_head<CompositeHead::Info>()
                                 .head_views({
                                     .draw_image = t_draw_image.view(),
                                     .output_image = gpu.t_swapchain_image.view(),
                                 })
                                 .executes(composite_callback, composite_pipeline.get(), global_buffer));
    loop_task_graph.add_task(daxa::RasterTask("draw GUI")
                                 .color_attachment.writes(gpu.t_swapchain_image)
                                 .executes(
                                     [this, cv = gpu.t_swapchain_image.view()](daxa::TaskInterface ti)
                                     {
                                         bool tg_debug_ui_open = task_graph_debug_ui.update(loop_task_graph);
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

void Renderer::init_ssao()
{

    std::random_device rd;
    auto gen = std::mt19937(rd());

    auto dist01 = std::uniform_real_distribution<float>(0.0f, 1.0f);
    auto dist_neg_pos = std::uniform_real_distribution<float>(-1.0f, 1.0f);

    std::vector<vec3> kernel = {};
    kernel.reserve(SSAO_N_SAMPLES);

    for (u32 i = 0; i < SSAO_N_SAMPLES; i++)
    {
        vec3 sample = {dist_neg_pos(gen), dist_neg_pos(gen), dist01(gen)};

        sample = glm::normalize(sample);

        float scale = dist01(gen);
        // Bias toward center (quadratic distribution) and offset
        scale = scale * scale;
        scale = glm::mix(0.1f, 1.0f, scale);
        sample *= scale;
        // Keep in hemisphere
        sample.z = abs(sample.z);

        kernel.push_back(sample);
    }

    std::vector<vec3> noise = {};
    noise.reserve(SSAO_N_ROTATIONS);
    for (u32 i = 0; i < SSAO_N_ROTATIONS; i++)
    {
        vec3 rotation = {dist_neg_pos(gen), dist_neg_pos(gen), 0.0f};
        noise.push_back(rotation);
    }

    UploadSession session = begin_upload_session();
    ssao_kernel_buffer = session.create_buffer(kernel.data(), {
                                                                  .size = SSAO_N_SAMPLES * sizeof(daxa_f32vec3),
                                                                  .memory_flags = daxa::MemoryFlagBits::NONE,
                                                                  .name = "ssao kernel buffer",
                                                              });
    ssao_noise_image = session.create_image(
        noise.data(), SSAO_N_ROTATIONS * sizeof(daxa_f32vec3), {},
        {
            .dimensions = 2,
            .format = daxa::Format::R16G16B16A16_SFLOAT,
            .size = {.x = SSAO_NOISE_DIM, .y = SSAO_NOISE_DIM, .z = 1},
            .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::TRANSFER_DST,
            .name = "ssao noise image",
        });
    session.flush();

    ssao_noise_sampler = gpu.device.create_sampler({
        .magnification_filter = daxa::Filter::NEAREST,
        .minification_filter = daxa::Filter::NEAREST,
        .address_mode_u = daxa::SamplerAddressMode::REPEAT,
        .address_mode_v = daxa::SamplerAddressMode::REPEAT,
        .name = "ssao noise sampler",
    });
}

void Renderer::render(FrameUniforms const & uniforms, Scene const & s)
{
    daxa::ImageId new_image = gpu.swapchain.acquire_next_image();
    if (new_image.is_empty())
    {
        return;
    }
    gpu.t_swapchain_image.set_image(new_image);

    gpu.stats.drawcall_count = 0;
    gpu.stats.triangle_count = 0;

    // Update internal scene lookup
    scene = &s;
    *gpu.device.buffer_host_address_as<GPUCamera>(camera_buffer).value() = uniforms.camera;
    *gpu.device.buffer_host_address_as<GPUFrameData>(frame_data_buffer).value() = uniforms.frame_data;
    loop_task_graph.execute({.debug_ui = &task_graph_debug_ui});
}

void Renderer::resize_resources(Window const & window)
{
    gpu.device.destroy_image(t_depth_image.info().image);
    t_depth_image.set_image(gpu.device.create_image(make_depth_info(window)));
    gpu.device.destroy_image(t_draw_image.info().image);
    t_draw_image.set_image(gpu.device.create_image(make_draw_info(window)));
    gpu.device.destroy_image(t_ssao_image.info().image);
    t_ssao_image.set_image(gpu.device.create_image(make_ssao_info(window)));
    gpu.device.destroy_image(t_ssao_blurred_image.info().image);
    t_ssao_blurred_image.set_image(gpu.device.create_image(make_ssao_blur_info(window)));
}

void Renderer::cleanup() const
{
    gpu.device.destroy_sampler(default_linear_sampler);
    gpu.device.destroy_sampler(default_nearest_sampler);
    gpu.device.destroy_sampler(shadow_sampler);
    gpu.device.destroy_sampler(ssao_noise_sampler);
    gpu.device.destroy_buffer(camera_buffer);
    gpu.device.destroy_buffer(frame_data_buffer);
    gpu.device.destroy_buffer(global_buffer);
    gpu.device.destroy_buffer(ssao_kernel_buffer);
    gpu.device.destroy_image(t_depth_image.info().image);
    gpu.device.destroy_image(t_shadow_map.info().image);
    gpu.device.destroy_image(t_draw_image.info().image);
    gpu.device.destroy_image(t_ssao_image.info().image);
    gpu.device.destroy_image(t_ssao_blurred_image.info().image);
    gpu.device.destroy_image(ssao_noise_image);
}
