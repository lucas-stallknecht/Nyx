#pragma once

#include "window.hpp"
#include <daxa/daxa.hpp>
#include <daxa/utils/pipeline_manager.hpp>
#include <daxa/utils/task_graph_types.hpp>

struct GPUContext
{
    bool initialized = false;
    daxa::Instance instance;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::PipelineManager pipeline_manager;
    daxa::ExternalTaskImage t_swapchain_image;

    void init(Window const & window);
};

extern GPUContext gpu;
