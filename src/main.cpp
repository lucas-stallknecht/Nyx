#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "shared.inl"
#include "asset_manager.hpp"
#include "window.hpp"
#include "camera.hpp"
#include "renderer.hpp"
#include "gpu_context.hpp"
#include <daxa/utils/task_graph.hpp>
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <imgui.h>

static constexpr vec3 DIR_LIGHT_POSITION = {5.0f, 20.0f, 2.0f};

int main()
{
    Window window = {
        .width = 1920,
        .height = 1080,
    };
    WindowInitResult window_res = window.init();
    if (window_res != WindowInitResult::Success)
    {
        auto int_res = static_cast<int>(window_res);
        fmt::println("Failed to initialize Window: {}", int_res);
        return int_res;
    }
    gpu.init(window);

    Camera camera = {};
    daxa::BufferId cam_buffer = gpu.device.create_buffer({
        .size = sizeof(CameraInfo),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "camera buffer",
    });
    daxa::BufferId light_buffer = gpu.device.create_buffer({
        .size = sizeof(LightInfo),
        .memory_flags = daxa::MemoryFlagBits::HOST_ACCESS_SEQUENTIAL_WRITE,
        .name = "light buffer",
    });
    glm::mat4 light_proj = glm::ortho(-15.0f, 15.0f, -15.0f, 15.0f, 0.1f, 50.0f);
    glm::mat4 light_view = glm::lookAt(DIR_LIGHT_POSITION, {}, {0.0f, 1.0f, 0.0f});
    auto * light_buffer_ptr = gpu.device.buffer_host_address_as<LightInfo>(light_buffer).value();
    *light_buffer_ptr = {
        .sun_dir = std::bit_cast<daxa_f32vec3>(glm::normalize(DIR_LIGHT_POSITION)),
        .sun_matrix = std::bit_cast<daxa_f32mat4x4>(light_proj * light_view),
    };

    Handle sponza_handle = {};
    asset_manager.load_model(std::string(ASSETS_DIR) + "models/sponza-ktx.glb", sponza_handle);
    Model * sponza = asset_manager.models.get(sponza_handle);

    Renderer renderer = {};
    renderer.init(window, {
                              .color_target = gpu.t_swapchain_image,
                              .camera_buffer = cam_buffer,
                              .light_buffer = light_buffer,
                              .model = sponza,
                          });

    ImGui::CreateContext();
    while (!window.should_close())
    {
        window.update();
        if (window.minimized)
        {
            continue;
        }
        if (window.swapchain_out_of_date)
        {
            gpu.swapchain.resize();
            window.swapchain_out_of_date = false;
        }

        daxa::PipelineReloadResult reloaded_result = gpu.pipeline_manager.reload_all();
        if (auto reload_err = daxa::get_if<daxa::PipelineReloadError>(&reloaded_result))
        {
            fmt::println("Failed to reload shaders: {}", reload_err->message);
        }
        if (daxa::get_if<daxa::PipelineReloadSuccess>(&reloaded_result))
        {
            fmt::println("Shaders successfuly reloaded");
        }

        ImGuiIO & io = ImGui::GetIO();
        f32 dt = io.DeltaTime;

        camera.rotate(window.consume_mouse_delta());
        if (window.pressed_keys[GLFW_KEY_W])
        {
            camera.move_forward(dt);
        }
        if (window.pressed_keys[GLFW_KEY_S])
        {
            camera.move_forward(-dt);
        }
        if (window.pressed_keys[GLFW_KEY_D])
        {
            camera.move_right(dt);
        }
        if (window.pressed_keys[GLFW_KEY_A])
        {
            camera.move_right(-dt);
        }
        if (window.pressed_keys[GLFW_KEY_SPACE])
        {
            camera.move_up(dt);
        }
        if (window.pressed_keys[GLFW_KEY_LEFT_CONTROL])
        {
            camera.move_up(-dt);
        }

        daxa::ImageId new_image = gpu.swapchain.acquire_next_image();
        if (new_image.is_empty())
        {
            continue;
        }
        gpu.t_swapchain_image.set_image(new_image);

        auto aspect_ratio = static_cast<f32>(window.width) / static_cast<f32>(window.height);
        auto * cam_buffer_ptr = gpu.device.buffer_host_address_as<CameraInfo>(cam_buffer).value();
        *cam_buffer_ptr = {
            .proj = std::bit_cast<daxa_f32mat4x4>(camera.get_proj(aspect_ratio)),
            .view = std::bit_cast<daxa_f32mat4x4>(camera.get_view()),
        };
        renderer.render();

        gpu.device.collect_garbage();
    }
    gpu.device.wait_idle();
    renderer.cleanup();
    window.cleanup();
    asset_manager.cleanup();
    gpu.device.destroy_buffer(cam_buffer);
    gpu.device.destroy_buffer(light_buffer);

    return 0;
}
