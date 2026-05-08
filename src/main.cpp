#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "shared.inl"
#include "asset_manager.hpp"
#include "window.hpp"
#include "camera.hpp"
#include "renderer.hpp"
#include "gpu_context.hpp"
#include <fmt/format.h>
#include <fmt/ranges.h>
#include <daxa/utils/task_graph.hpp>
#include <daxa/utils/imgui.hpp>
#include <imgui_impl_glfw.h>
#include <imgui.h>

namespace
{
    struct AppState
    {
        Camera camera = {};
        LightInfo light_info;
        float light_distance = 20.0f;

        float shadow_range = 15.0f;
        float shadow_near = 0.1f;
        float shadow_far = 50.0f;
    };

    static AppState app_state = {
        .light_info =
            {
                .dir_light_direction = {0.25f, 1.0f, 0.1f},
                .dir_light_color = {1.0f, 1.0f, 1.0f},
                .num_point_lights = 3,
            },
    };

    void update_ui()
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Lighting");
        ImGui::SeparatorText("Directional Light");

        ImGui::DragFloat3("Direction", &app_state.light_info.dir_light_direction.x, 0.01f);
        ImGui::ColorEdit3("Sun Color", &app_state.light_info.dir_light_color.x);

        ImGui::Spacing();

        ImGui::DragFloat("Light Distance", &app_state.light_distance, 0.1f, 1.0f, 200.0f);
        ImGui::DragFloat("Shadow Range", &app_state.shadow_range, 0.1f, 1.0f, 200.0f);
        ImGui::DragFloat("Shadow Near", &app_state.shadow_near, 0.01f, 0.001f, 20.0f);
        ImGui::DragFloat("Shadow Far", &app_state.shadow_far, 0.1f, 1.0f, 500.0f);

        ImGui::SeparatorText("Point Lights");
        ImGui::SliderInt("Count", reinterpret_cast<int *>(&app_state.light_info.num_point_lights), 0, MAX_POINT_LIGHTS);

        for (u32 i = 0; i < app_state.light_info.num_point_lights; ++i)
        {
            PointLight & light = app_state.light_info.point_lights[i];

            ImGui::PushID(static_cast<int>(i));

            if (ImGui::TreeNode("Point Light"))
            {
                ImGui::DragFloat3("Position", &light.position.x, 0.05f);
                ImGui::ColorEdit3("Color", &light.color.x);
                ImGui::DragFloat("Linear", &light.linear, 0.001f, 0.001f, 10.0f);
                ImGui::DragFloat("Quadratic", &light.quadratic, 0.001f, 0.001f, 20.0f);

                // Useful presets
                if (ImGui::Button("7m Range"))
                {
                    light.linear = 0.7f;
                    light.quadratic = 1.8f;
                }
                ImGui::SameLine();
                if (ImGui::Button("20m Range"))
                {
                    light.linear = 0.22f;
                    light.quadratic = 0.20f;
                }
                ImGui::SameLine();
                if (ImGui::Button("50m Range"))
                {
                    light.linear = 0.09f;
                    light.quadratic = 0.032f;
                }
                ImGui::TreePop();
            }
            ImGui::PopID();
        }

        ImGui::End();
    }

    void handle_inputs(Window & window, f32 dt)
    {
        Camera & camera = app_state.camera;
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
    }

    void update_camera_buufer(daxa::BufferId buffer, u32 width, u32 height)
    {
        auto aspect_ratio = static_cast<f32>(width) / static_cast<f32>(height);
        auto * cam_buffer_ptr = gpu.device.buffer_host_address_as<CameraInfo>(buffer).value();
        *cam_buffer_ptr = {
            .proj = std::bit_cast<daxa_f32mat4x4>(app_state.camera.get_proj(aspect_ratio)),
            .view = std::bit_cast<daxa_f32mat4x4>(app_state.camera.get_view()),
        };
    }

    void update_light_buffer(daxa::BufferId buffer)
    {
        vec3 light_dir = {
            app_state.light_info.dir_light_direction.x,
            app_state.light_info.dir_light_direction.y,
            app_state.light_info.dir_light_direction.z,
        };
        mat4 light_proj = glm::ortho(-app_state.shadow_range, app_state.shadow_range, -app_state.shadow_range,
                                     app_state.shadow_range, app_state.shadow_near, app_state.shadow_far);
        mat4 light_view = glm::lookAt(app_state.light_distance * light_dir, {}, {0.0f, 1.0f, 0.0f});
        auto * light_buffer_ptr = gpu.device.buffer_host_address_as<LightInfo>(buffer).value();
        *light_buffer_ptr = {
            .dir_light_direction = std::bit_cast<daxa_f32vec3>(glm::normalize(light_dir)),
            .dir_light_color = app_state.light_info.dir_light_color,
            .dir_light_matrix = std::bit_cast<daxa_f32mat4x4>(light_proj * light_view),
            .num_point_lights = app_state.light_info.num_point_lights,
        };
        for (u32 i = 0; i < app_state.light_info.num_point_lights; ++i)
        {
            PointLight & light = app_state.light_info.point_lights[i];
            light_buffer_ptr->point_lights[i] = light;
        }
    }

} // namespace

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

    auto sponza_handle = asset_manager.load_model(std::string(ASSETS_DIR) + "models/sponza-ktx.glb");
    Model * sponza = asset_manager.models.get(sponza_handle.value());

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window.glfw_window_ptr, true);

    Renderer renderer = {};
    renderer.init(window, {
                              .color_target = gpu.t_swapchain_image,
                              .camera_buffer = cam_buffer,
                              .light_buffer = light_buffer,
                              .model = sponza,
                          });

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
        handle_inputs(window, dt);
        update_ui();
        update_light_buffer(light_buffer);
        update_camera_buufer(cam_buffer, window.width, window.height);

        daxa::ImageId new_image = gpu.swapchain.acquire_next_image();
        if (new_image.is_empty())
        {
            continue;
        }
        gpu.t_swapchain_image.set_image(new_image);
        renderer.render();

        gpu.device.collect_garbage();
    }

    gpu.device.wait_idle();
    ImGui_ImplGlfw_Shutdown();
    renderer.cleanup();
    asset_manager.cleanup();
    window.cleanup();
    gpu.device.destroy_buffer(cam_buffer);
    gpu.device.destroy_buffer(light_buffer);

    return 0;
}
