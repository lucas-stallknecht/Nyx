#pragma once

#include "window.hpp"
#include <daxa/daxa.hpp>
#include <daxa/utils/pipeline_manager.hpp>

struct GPUContext
{
    daxa::Instance instance;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::PipelineManager pipeline_manager;
    bool initialized = false;
};

extern GPUContext gpu;

void gpu_context_init(Window const & window);
