#pragma once

#include "gpu_lighting.inl"
#include "scene.hpp"
#include "window.hpp"
#include <daxa/daxa.hpp>
#include <daxa/utils/imgui.hpp>
#include <daxa/utils/task_graph.hpp>
#include <memory>

struct FrameUniforms
{
    GPUCamera camera;
    GPULightInfo lights;
};

struct Renderer
{
    daxa::ImGuiRenderer imgui_renderer;
    std::shared_ptr<daxa::RasterPipeline> shadow_pipeline;
    std::shared_ptr<daxa::RasterPipeline> forward_pipeline;
    daxa::SamplerId default_linear_sampler;
    daxa::SamplerId shadow_sampler;
    daxa::BufferId camera_buffer;
    daxa::BufferId light_buffer;
    daxa::BufferId global_buffer;
    daxa::ExternalTaskImage t_depth_image;
    daxa::ExternalTaskImage t_shadow_depth_image;
    daxa::TaskGraph loop_task_graph;

    void init(Window const & window);
    void cleanup() const;
    void resize_resources(Window const & window);

    void render(FrameUniforms const & uniforms, Scene const & scene);

  private:
    Scene const * scene = nullptr;
    void init_resources(Window const & window);
    void init_task_graphs();
};
