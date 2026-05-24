#pragma once

#include "camera.hpp"
#include "gpu_globals.inl"
#include "scene.hpp"
#include "window.hpp"
#include <daxa/daxa.hpp>
#include <daxa/utils/imgui.hpp>
#include <daxa/utils/task_graph.hpp>
#include <memory>

struct ResizableImage
{
    std::function<daxa::ImageInfo(Window const & w)> info_create;
    daxa::ExternalTaskImage &                        target_image;
};

struct Renderer
{
    ~Renderer();

    daxa::ImGuiRenderer                    imgui_renderer;
    std::shared_ptr<daxa::RasterPipeline>  prepass_pipeline;
    std::shared_ptr<daxa::RasterPipeline>  shadow_pipeline;
    std::shared_ptr<daxa::ComputePipeline> ssao_pipeline;
    std::shared_ptr<daxa::ComputePipeline> blur_pipeline;
    std::shared_ptr<daxa::ComputePipeline> bright_parts_pipeline;
    std::shared_ptr<daxa::ComputePipeline> gaussian_blur_pipeline;
    std::shared_ptr<daxa::RasterPipeline>  opaque_pipeline;
    std::shared_ptr<daxa::RasterPipeline>  transparent_pipeline;
    std::shared_ptr<daxa::RasterPipeline>  debug_wireframe_pipeline;
    std::shared_ptr<daxa::ComputePipeline> ssr_pipeline;
    std::shared_ptr<daxa::ComputePipeline> volumetric_lighting_pipeline;
    std::shared_ptr<daxa::ComputePipeline> composite_pipeline;
    daxa::SamplerId                        default_linear_sampler;
    daxa::SamplerId                        default_nearest_sampler;
    daxa::SamplerId                        shadow_sampler;
    daxa::SamplerId                        ssao_noise_sampler;
    daxa::BufferId                         global_buffer;
    daxa::BufferId                         ssao_kernel_buffer;
    daxa::ImageId                          ssao_noise_image;
    daxa::ExternalTaskImage                t_draw_image;
    daxa::ExternalTaskImage                t_draw_image_msaa;
    daxa::ExternalTaskImage                t_brightcolor_image;
    daxa::ExternalTaskImage                t_brightcolor_image_ping0;
    daxa::ExternalTaskImage                t_depth_image;
    daxa::ExternalTaskImage                t_depth_image_msaa;
    daxa::ExternalTaskImage                t_slim_gbuffer;
    daxa::ExternalTaskImage                t_shadow_map;
    daxa::ExternalTaskImage                t_ssao_image;
    daxa::ExternalTaskImage                t_ssao_blurred_image;
    daxa::ExternalTaskImage                t_ssr_image;
    daxa::ExternalTaskImage                t_volumetric_lighting_image;
    daxa::ExternalTaskImage                t_volumetric_lighting_image_ping0;
    daxa::TaskGraph                        loop_task_graph;
    daxa::TaskGraphDebugUi                 task_graph_debug_ui;

    GPUFrameData frame_data = {
        .ambient_light_color = {1.0f, 1.0f, 1.0f},
        .ambient_light_intensity = 0.08f,
        .dir_light_direction = {0.25f, 1.0f, 0.1f},
        .dir_light_intensity = 5.0f,
        .dir_light_color = {1.0f, 1.0f, 1.0f},
        .num_point_lights = 0,
        .pcf_enabled = true,
        .bloom_enabled = true,
        .bloom_intensity = 1.0f,
        .exposure = 1.0f,
        .ssao_enabled = true,
        .ssao_radius = 0.2f,
        .ssao_bias = 0.001f,
        .ssr_enabled = false,
        .ssr_min_mask = 0.02f,
        .ssr_max_mask = 1.0f,
        .ssr_reflection_intensity = 1.0,
        .ssr_screen_edge_fade = 0.2f,
        .ssr_num_samples = 256,
        .ssr_max_distance = 100.0,
        .vlight_enabled = true,
        .vlight_num_samples = 64,
        .vlight_step_size = 0.2f,
        .vlight_density = 0.003f,
    };
    f32 light_distance = 25.0f;
    f32 shadow_range = 15.0f;
    f32 shadow_near = 0.1f;
    f32 shadow_far = 50.0f;

    void init(Window const & window);
    void resize_resources(Window const & window);
    void render(Camera const & camera, Scene const & scene);

  private:
    bool                        initialized = false;
    Scene const *               scene = nullptr;
    daxa_b32                    blur_brightpass = true;
    std::vector<ResizableImage> resizable_iamges = {};

    void init_resources(Window const & window);
    void init_task_graphs();
    void init_ssao();
    void create_resizable_image(Window const & window, daxa::ExternalTaskImage & t_image,
                                std::function<daxa::ImageInfo(Window const & w)> const & info_create,
                                std::string const &                                      name);
};
