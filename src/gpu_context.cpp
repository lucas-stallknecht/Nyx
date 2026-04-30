#include "gpu_context.hpp"

GPUContext gpu = {};

#include <fmt/format.h>
#include <cassert>

void GPUContext::init(Window const & window)
{
    assert(!gpu.initialized && "GPUContext already initialized");
    assert(window.glfw_window_ptr && "Window must be initialized before GPUContext");
    gpu.instance = daxa::create_instance({});
    gpu.device = gpu.instance.create_device_2(gpu.instance.choose_device({}, {}));
    fmt::println("Chosen GPU: {}", reinterpret_cast<char const *>(gpu.device.properties().device_name));
    gpu.swapchain = gpu.device.create_swapchain({
        .native_window_info = window.get_native_window_info(),
        .surface_format = {.format = daxa::Format::R8G8B8A8_UNORM, .color_space = daxa::ColorSpace::SRGB_NONLINEAR},
        .present_mode = daxa::PresentMode::FIFO,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_STORAGE,
    });
    gpu.pipeline_manager = daxa::PipelineManager({
        .device = gpu.device,
        .root_paths =
            {
                DAXA_SHADER_INCLUDE_DIR,
                "src",
            },
        .default_language = daxa::ShaderLanguage::GLSL,
        .default_enable_debug_info = true,
    });
    gpu.initialized = true;
}
