#pragma once

#include "gpu_globals.inl"
#include "scene.hpp"
#include "window.hpp"
#include <daxa/daxa.hpp>
#include <daxa/utils/imgui.hpp>
#include <daxa/utils/task_graph.hpp>
#include <memory>

struct FrameUniforms
{
    GPUCamera camera;
    GPUFrameData frame_data;
};

struct Renderer
{
    daxa::ImGuiRenderer imgui_renderer;
    std::shared_ptr<daxa::RasterPipeline> prepass_pipeline;
    std::shared_ptr<daxa::RasterPipeline> shadow_pipeline;
    std::shared_ptr<daxa::ComputePipeline> ssao_pipeline;
    std::shared_ptr<daxa::ComputePipeline> blur_pipeline;
    std::shared_ptr<daxa::ComputePipeline> gaussian_blur_pipeline;
    std::shared_ptr<daxa::RasterPipeline> opaque_pipeline;
    std::shared_ptr<daxa::RasterPipeline> transparent_pipeline;
    std::shared_ptr<daxa::RasterPipeline> debug_wireframe_pipeline;
    std::shared_ptr<daxa::ComputePipeline> ssr_pipeline;
    std::shared_ptr<daxa::ComputePipeline> composite_pipeline;
    daxa::SamplerId default_linear_sampler;
    daxa::SamplerId default_nearest_sampler;
    daxa::SamplerId shadow_sampler;
    daxa::SamplerId ssao_noise_sampler;
    daxa::BufferId camera_buffer;
    daxa::BufferId frame_data_buffer;
    daxa::BufferId params_buffer;
    daxa::BufferId global_buffer;
    daxa::BufferId ssao_kernel_buffer;
    daxa::ImageId ssao_noise_image;
    daxa::ExternalTaskImage t_draw_image;
    daxa::ExternalTaskImage t_brightcolor_image;
    std::array<daxa::ExternalTaskImage, 2> t_brightcolor_blurred_images;
    daxa::ExternalTaskImage t_depth_image;
    daxa::ExternalTaskImage t_slim_gbuffer;
    daxa::ExternalTaskImage t_shadow_map;
    daxa::ExternalTaskImage t_ssao_image;
    daxa::ExternalTaskImage t_ssao_blurred_image;
    daxa::ExternalTaskImage t_ssr_image;
    daxa::TaskGraph loop_task_graph;
    daxa::TaskGraphDebugUi task_graph_debug_ui;

    void init(Window const & window);
    void cleanup() const;
    void resize_resources(Window const & window);

    void render(FrameUniforms const & uniforms, Scene const & scene);

  private:
    Scene const * scene = nullptr;
    GPUFrameData frame_data = {};
    daxa_b32 blur_brightpass = true;
    void init_resources(Window const & window);
    void init_task_graphs();
    void init_ssao();
};
