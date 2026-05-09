#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "asset_manager.hpp"
#include "scene.hpp"
#include "window.hpp"
#include "camera.hpp"
#include "renderer.hpp"
#include "gpu_context.hpp"
#include <fmt/core.h>
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
        GPULightInfo light_info;
        float light_distance = 20.0f;

        float shadow_range = 15.0f;
        float shadow_near = 0.1f;
        float shadow_far = 50.0f;
    };

    static AppState app_state = {
        .light_info =
            {
                .dir_light_direction = {0.25f, 1.0f, 0.1f},
                .dir_light_intensity = 5.0f,
                .dir_light_color = {1.0f, 1.0f, 1.0f},
                .num_point_lights = 1,
            },
    };

    void update_ui()
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("Lighting");
        ImGui::SeparatorText("Directional Light");

        ImGui::DragFloat("Intensity", &app_state.light_info.dir_light_intensity, 0.01f, 0.0f, 10.0f);
        ImGui::DragFloat3("Direction", &app_state.light_info.dir_light_direction.x, 0.01f);
        ImGui::ColorEdit3("Color", &app_state.light_info.dir_light_color.x);

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

    FrameUniforms build_frame_uniforms(Window const & window)
    {
        f32 const aspect = static_cast<f32>(window.width) / static_cast<f32>(window.height);
        vec3 const light_dir =
            glm::normalize(vec3(app_state.light_info.dir_light_direction.x, app_state.light_info.dir_light_direction.y,
                                app_state.light_info.dir_light_direction.z));
        mat4 const light_proj = glm::ortho(-app_state.shadow_range, app_state.shadow_range, -app_state.shadow_range,
                                           app_state.shadow_range, app_state.shadow_near, app_state.shadow_far);
        mat4 const light_view = glm::lookAt(app_state.light_distance * light_dir, {}, {0.0f, 1.0f, 0.0f});

        FrameUniforms frame;
        frame.camera = {
            .proj = std::bit_cast<daxa_f32mat4x4>(app_state.camera.get_proj(aspect)),
            .view = std::bit_cast<daxa_f32mat4x4>(app_state.camera.get_view()),
            .position = std::bit_cast<daxa_f32vec3>(app_state.camera.position),
        };
        frame.lights = app_state.light_info;
        frame.lights.dir_light_direction = std::bit_cast<daxa_f32vec3>(light_dir);
        frame.lights.dir_light_matrix = std::bit_cast<daxa_f32mat4x4>(light_proj * light_view);
        return frame;
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

    // daxa::ImageId env_map = asset_manager.load_texture(std::string(ASSETS_DIR) + "textures/bunker.ktx").value();
    Model * model = asset_manager.load_model(std::string(ASSETS_DIR) + "models/sponza-ktx.glb").value();

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window.glfw_window_ptr, true);

    Scene scene = {};

    Renderer renderer = {};
    renderer.init(window);

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
            renderer.resize_resources(window);
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

        scene.clear();
        scene.add_model(*model);

        renderer.render(build_frame_uniforms(window), scene);

        gpu.device.collect_garbage();
    }

    gpu.device.wait_idle();
    ImGui_ImplGlfw_Shutdown();
    renderer.cleanup();
    asset_manager.cleanup();
    window.cleanup();

    return 0;
}
