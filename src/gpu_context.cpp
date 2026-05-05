#include "gpu_context.hpp"

#include <fmt/format.h>
#include <cassert>

GPUContext gpu = {};

void GPUContext::init(Window const & window)
{
    assert(!initialized && "GPUContext already initialized");
    assert(window.glfw_window_ptr && "Window must be initialized before GPUContext");
    instance = daxa::create_instance({});
    device = instance.create_device_2(instance.choose_device({}, {}));
    fmt::println("Chosen GPU: {}", reinterpret_cast<char const *>(device.properties().device_name));
    swapchain = device.create_swapchain({
        .native_window_info = window.get_native_window_info(),
        .surface_format = {.format = daxa::Format::R8G8B8A8_UNORM, .color_space = daxa::ColorSpace::SRGB_NONLINEAR},
        .present_mode = daxa::PresentMode::FIFO,
        .image_usage = daxa::ImageUsageFlagBits::TRANSFER_DST | daxa::ImageUsageFlagBits::SHADER_STORAGE,
    });
    pipeline_manager = daxa::PipelineManager({
        .device = device,
        .root_paths =
            {
                DAXA_SHADER_INCLUDE_DIR,
                "src",
            },
        .default_language = daxa::ShaderLanguage::GLSL,
        .default_enable_debug_info = true,
    });

    t_swapchain_image = daxa::ExternalTaskImage({
        .is_swapchain_image = true,
        .name = "task swapchain image",
    });

    initialized = true;
}
