#pragma once

#include "model.hpp"
#include "window.hpp"
#include <daxa/daxa.hpp>
#include <daxa/utils/task_graph.hpp>

static constexpr usize SHADOW_IMAGE_SIZE = 4096;

struct RenderDependencies
{
    daxa::ExternalTaskImage color_target;
    daxa::BufferId camera_buffer;
    daxa::BufferId light_buffer;
    Model * model;
};

struct Renderer
{
    std::shared_ptr<daxa::RasterPipeline> forward_pipeline;
    std::shared_ptr<daxa::RasterPipeline> shadow_mapping_pipeline;
    daxa::SamplerId default_linear_sampler;
    daxa::SamplerId shadow_sampler;
    daxa::BufferId global_buffer;
    daxa::TaskGraph loop_task_graph;
    daxa::ExternalTaskImage t_depth_image;
    daxa::ExternalTaskImage t_shadow_depth_image;

    void init(Window const & window, RenderDependencies dependencies);
    void cleanup();
    void resize_resources(Window const & window);
    void render();

  protected:
    void init_pipelines();
    void init_resources(Window const & window, RenderDependencies dependencies);
    void init_task_graphs(RenderDependencies dependencies);
};
