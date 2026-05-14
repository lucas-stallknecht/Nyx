#pragma once

#include "window.hpp"
#include <daxa/daxa.hpp>
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph_types.hpp>

struct RenderingStats
{
    int triangle_count = 0;
    int drawcall_count = 0;
};

struct GPUContext
{
    bool initialized = false;
    daxa::Instance instance;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::PipelineManager pipeline_manager;
    daxa::ExternalTaskImage t_swapchain_image;
    RenderingStats stats = {};

    void init(Window const & window);
};

extern GPUContext gpu;
