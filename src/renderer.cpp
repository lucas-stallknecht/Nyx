#ifndef GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#endif
#include "renderer.hpp"

#include "gpu_debug.inl"
#include "raster/prepass.inl"
#include "raster/shadow_mapping.inl"
#include "raster/forward.inl"
#include "raster/debug_wireframe.inl"
#include "postprocess/ssao.inl"
#include "postprocess/composite.inl"
#include "postprocess/blur.inl"
#include "postprocess/ssr.inl"
#include "postprocess/gaussian_blur.inl"
#include "gpu_context.hpp"
#include "utils/upload.hpp"
#include <fmt/core.h>
#include <glm/gtc/matrix_transform.hpp>
#include <imgui.h>
#include <random>
#include <bit>

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
    daxa::ImageInfo make_slim_gbuffer_indo(Window const & w)
    {
        return {
            .format = daxa::Format::R16G16B16A16_UNORM,
            .size = {.x = w.width, .y = w.height, .z = 1},
            .usage = daxa::ImageUsageFlagBits::COLOR_ATTACHMENT | daxa::ImageUsageFlagBits::SHADER_SAMPLED |
                     daxa::ImageUsageFlagBits::SHADER_STORAGE,
            .name = "slim gbuffer image",
        };
    }
    daxa::ImageInfo make_ssao_info(Window const & w)
    {
        return {
            .format = daxa::Format::R16_SFLOAT,
            .size = {.x = w.width / 2, .y = w.height / 2, .z = 1},
            .usage = daxa::ImageUsageFlagBits::SHADER_SAMPLED | daxa::ImageUsageFlagBits::SHADER_STORAGE,
            .name = "ssao image",
        };
    }
    daxa::ImageInfo make_ssao_blur_info(Window const & w)
    {
        daxa::ImageInfo info = make_ssao_info(w);
        info.name = "ssao blurred image";
        return info;
    }
    daxa::ImageInfo make_ssr_info(Window const & w)
    {
        daxa::ImageInfo info = make_draw_info(w);
        info.usage = daxa::ImageUsageFlagBits::SHADER_STORAGE;
        info.name = "ssr image";
        return info;
    }
    daxa::ImageInfo make_brightcolor_info(Window const & w)
    {
        daxa::ImageInfo info = make_draw_info(w);
        info.name = "bright parts image";
        return info;
    }
} // namespace

void Renderer::init(Window const & window)
{
    assert(!initialized && "Renderer already initialized");
    assert(window.glfw_window_ptr && "Window must be initialized before Renderer");
    imgui_renderer = daxa::ImGuiRenderer({
        .device = gpu.device,
        .format = gpu.swapchain.get_format(),
    });

    init_resources(window);
    init_ssao();

    fmt::println("[Renderer] Compiling shaders...");

    prepass_pipeline = gpu.pipeline_manager.add_raster_pipeline2(prepass_pipeline_info()).value();
    shadow_pipeline = gpu.pipeline_manager.add_raster_pipeline2(shadow_mapping_pipeline_info()).value();
    ssao_pipeline = gpu.pipeline_manager.add_compute_pipeline2(ssao_pipeline_info()).value();
    blur_pipeline = gpu.pipeline_manager.add_compute_pipeline2(blur_pipeline_info()).value();
    gaussian_blur_pipeline = gpu.pipeline_manager.add_compute_pipeline2(gaussian_blur_pipeline_info()).value();
    opaque_pipeline = gpu.pipeline_manager.add_raster_pipeline2(opaque_pipeline_info()).value();
    transparent_pipeline = gpu.pipeline_manager.add_raster_pipeline2(transparent_pipeline_info()).value();
    debug_wireframe_pipeline = gpu.pipeline_manager.add_raster_pipeline2(debug_wireframe_pipeline_info()).value();
    ssr_pipeline = gpu.pipeline_manager.add_compute_pipeline2(ssr_pipeline_info()).value();
    composite_pipeline = gpu.pipeline_manager.add_compute_pipeline2(composite_pipeline_info()).value();

    fmt::println("[Renderer] Shaders ready");

    init_task_graphs();
    task_graph_debug_ui = daxa::TaskGraphDebugUi({
        .device = gpu.device,
        .imgui_renderer = imgui_renderer,
        .buffer_layout_cache_folder = "./tg_dbg_cache",
    });

    initialized = true;
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
        .enable_compare = true,
        .compare_op = daxa::CompareOp::GREATER_OR_EQUAL,
        .name = "shadow sampler",
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
    create_resizable_image(window, t_draw_image, make_draw_info, "task draw image");
    create_resizable_image(window, t_brightcolor_image, make_brightcolor_info, "task bright color image");
    create_resizable_image(window, t_depth_image, make_depth_info, "task depth image");
    create_resizable_image(window, t_slim_gbuffer, make_slim_gbuffer_indo, "task slim gbuffer image");
    create_resizable_image(window, t_ssao_image, make_ssao_info, "task ssao image");
    create_resizable_image(window, t_ssao_blurred_image, make_ssao_blur_info, "task ssao blurred image");
    create_resizable_image(window, t_ssr_image, make_ssr_info, "task ssr image");
    create_resizable_image(window, t_brightcolor_blurred_images[0], make_brightcolor_info,
                           "task bright color blurred image 1");
    create_resizable_image(window, t_brightcolor_blurred_images[1], make_brightcolor_info,
                           "task bright color blurred image 2");

    global_buffer = gpu.device.create_buffer({
        .size = sizeof(GPUGlobals),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "global rendering buffer",
    });
}

void Renderer::create_resizable_image(Window const & window, daxa::ExternalTaskImage & t_image,
                                      std::function<daxa::ImageInfo(Window const & w)> const & info_create,
                                      std::string const &                                      name)
{
    t_image = daxa::ExternalTaskImage({
        .image = gpu.device.create_image(info_create(window)),
        .name = name,
    });
    resizable_iamges.emplace_back(ResizableImage{
        .info_create = info_create,
        .target_image = t_image,
    });
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
    loop_task_graph.register_image(t_brightcolor_image);
    loop_task_graph.register_image(t_brightcolor_blurred_images[0]);
    loop_task_graph.register_image(t_brightcolor_blurred_images[1]);
    loop_task_graph.register_image(t_ssr_image);
    loop_task_graph.register_image(t_depth_image);
    loop_task_graph.register_image(t_shadow_map);
    loop_task_graph.register_image(t_ssao_image);
    loop_task_graph.register_image(t_ssao_blurred_image);
    loop_task_graph.register_image(t_slim_gbuffer);

    // Since this->scene value changes every frame, we must pass a pointer to it
    loop_task_graph.add_task(daxa::RasterTask("draw prepass")
                                 .depth_stencil_attachment.writes(t_depth_image.view())
                                 .color_attachment.writes(t_slim_gbuffer.view())
                                 .executes(prepass_callback, prepass_pipeline.get(), &scene, t_depth_image.view(),
                                           t_slim_gbuffer.view(), global_buffer));
    loop_task_graph.add_task(daxa::ComputeTask("compute ssao")
                                 .uses_head<SSAOHead::Info>()
                                 .head_views({
                                     .depth_image = t_depth_image.view(),
                                     .slim_gbuffer = t_slim_gbuffer.view(),
                                     .ssao_image = t_ssao_image.view(),
                                 })
                                 .executes(ssao_callback, ssao_pipeline.get(), &(frame_data.ssao_enabled),
                                           global_buffer, ssao_kernel_buffer, ssao_noise_image, ssao_noise_sampler));
    loop_task_graph.add_task(
        daxa::ComputeTask("blur ssao")
            .uses_head<BlurHead::Info>()
            .head_views({
                .input_image = t_ssao_image.view(),
                .blurred_image = t_ssao_blurred_image.view(),
            })
            .executes(blur_callback, blur_pipeline.get(), &(frame_data.ssao_enabled), global_buffer));
    loop_task_graph.add_task(
        daxa::RasterTask("draw shadow depth")
            .depth_stencil_attachment.writes(t_shadow_map.view())
            .executes(shadow_mapping_callback, shadow_pipeline.get(), &scene, t_shadow_map.view(), global_buffer));
    loop_task_graph.add_task(
        daxa::RasterTask("draw forward")
            .uses_head<ForwardPassHead::Info>()
            .head_views({
                .color_target = t_draw_image.view(),
                .bright_color_target = t_brightcolor_image.view(),
                .depth_target = t_depth_image.view(),
                .shadow_map = t_shadow_map.view(),
                .ssao_image = t_ssao_blurred_image.view(),
            })
            .executes(forward_callback, opaque_pipeline.get(), transparent_pipeline.get(), &scene, global_buffer));
    loop_task_graph.add_task(daxa::RasterTask("draw debug wireframes")
                                 .color_attachment.writes(t_draw_image.view())
                                 .executes(debug_wireframe_callback, debug_wireframe_pipeline.get(), &scene,
                                           global_buffer, t_draw_image.view()));
    loop_task_graph.add_task(daxa::ComputeTask("blur brightpass")
                                 .compute_shader.reads(t_brightcolor_image.view())
                                 .compute_shader.writes(t_brightcolor_blurred_images[0].view())
                                 .compute_shader.writes(t_brightcolor_blurred_images[1].view())
                                 .executes(gaussian_blur_callback, gaussian_blur_pipeline.get(), &blur_brightpass,
                                           global_buffer, t_brightcolor_image.view(),
                                           t_brightcolor_blurred_images[0].view(),
                                           t_brightcolor_blurred_images[1].view()));
    loop_task_graph.add_task(daxa::ComputeTask("ssr")
                                 .uses_head<SSRHead::Info>()
                                 .head_views({
                                     .depth_image = t_depth_image.view(),
                                     .slim_gbuffer = t_slim_gbuffer.view(),
                                     .input_image = t_draw_image.view(),
                                     .ssr_image = t_ssr_image.view(),
                                 })
                                 .executes(ssr_callback, ssr_pipeline.get(), &(frame_data.ssr_enabled), global_buffer));
    loop_task_graph.add_task(daxa::ComputeTask("composition")
                                 .uses_head<CompositeHead::Info>()
                                 .head_views({
                                     .draw_image = t_draw_image.view(),
                                     .ssr_image = t_ssr_image.view(),
                                     .bloom_image = t_brightcolor_blurred_images[0].view(),
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
    auto               gen = std::mt19937(rd());

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

void Renderer::render(Camera const & camera, Scene const & s)
{
    daxa::ImageId new_image = gpu.swapchain.acquire_next_image();
    if (new_image.is_empty())
    {
        return;
    }
    gpu.t_swapchain_image.set_image(new_image);

    gpu.stats.drawcall_count = 0;
    gpu.stats.triangle_count = 0;

    mat4 const cam_proj = camera.get_proj();
    mat4 const cam_view = camera.get_view();
    GPUCamera  gpu_camera = {
        .proj = std::bit_cast<daxa_f32mat4x4>(cam_proj),
        .inv_proj = std::bit_cast<daxa_f32mat4x4>(glm::inverse(cam_proj)),
        .view = std::bit_cast<daxa_f32mat4x4>(cam_view),
        .inv_view = std::bit_cast<daxa_f32mat4x4>(glm::inverse(cam_view)),
        .position = std::bit_cast<daxa_f32vec3>(camera.position),
    };

    // Frustum Culling DEBUG OVERRIDE
    if (frame_data.debug_view == static_cast<int>(DebugView::FrustumCulling))
    {
        vec3 focus = vec3(0.0f);
        vec3 debug_pos = focus + vec3(0.0f, 30.0f, 30.0f);
        mat4 debug_view = glm::lookAt(debug_pos, focus, vec3(0.0f, 1.0f, 0.0f));
        gpu_camera.view = std::bit_cast<daxa_f32mat4x4>(debug_view);
        gpu_camera.inv_view = std::bit_cast<daxa_f32mat4x4>(glm::inverse(debug_view));
        gpu_camera.position = std::bit_cast<daxa_f32vec3>(debug_pos);
    }

    vec3 const light_dir = glm::normalize(std::bit_cast<vec3>(frame_data.dir_light_direction));
    mat4 const light_proj =
        glm::ortho(-shadow_range, shadow_range, -shadow_range, shadow_range, shadow_near, shadow_far);
    mat4 const light_view = glm::lookAt(light_distance * light_dir, {}, vec3(0.0f, 1.0f, 0.0f));
    frame_data.dir_light_direction = std::bit_cast<daxa_f32vec3>(light_dir);
    frame_data.dir_light_matrix = std::bit_cast<daxa_f32mat4x4>(light_proj * light_view);
    *gpu.device.buffer_host_address_as<GPUGlobals>(global_buffer).value() = GPUGlobals{
        .default_linear_sampler = default_linear_sampler,
        .default_nearest_sampler = default_nearest_sampler,
        .shadow_sampler = shadow_sampler,
        .camera = gpu_camera,
        .frame_data = frame_data,
    };

    // Update scene internal lookup used in callbacks
    scene = &s;

    loop_task_graph.execute({.debug_ui = &task_graph_debug_ui});
}

void Renderer::resize_resources(Window const & window)
{
    for (auto & image : resizable_iamges)
    {
        gpu.device.destroy_image(image.target_image.info().image);
        image.target_image.set_image(gpu.device.create_image(image.info_create(window)));
    }
}

Renderer::~Renderer()
{
    if (!initialized)
    {
        return;
    }
    gpu.device.wait_idle();
    gpu.device.destroy_sampler(default_linear_sampler);
    gpu.device.destroy_sampler(default_nearest_sampler);
    gpu.device.destroy_sampler(shadow_sampler);
    gpu.device.destroy_sampler(ssao_noise_sampler);
    gpu.device.destroy_buffer(global_buffer);
    gpu.device.destroy_buffer(ssao_kernel_buffer);
    gpu.device.destroy_image(t_shadow_map.info().image);
    gpu.device.destroy_image(ssao_noise_image);
    for (auto & image : resizable_iamges)
    {
        gpu.device.destroy_image(image.target_image.info().image);
    }
}
