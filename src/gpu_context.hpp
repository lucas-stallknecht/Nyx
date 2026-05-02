#pragma once

#include "window.hpp"
#include <daxa/daxa.hpp>
#include <daxa/utils/pipeline_manager.hpp>

struct GPUContext
{
    bool initialized = false;
    daxa::Instance instance;
    daxa::Device device;
    daxa::Swapchain swapchain;
    daxa::PipelineManager pipeline_manager;

    void init(Window const & window);
};

extern GPUContext gpu;
