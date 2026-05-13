#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "asset_manager.hpp"
#include "scene.hpp"
#include "window.hpp"
#include "camera.hpp"
#include "renderer.hpp"
#include "gpu_context.hpp"
#include "gpu_debug.inl"
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
        GPUFrameData frame_data = {};
        f32 light_distance = 20.0f;
        f32 shadow_range = 15.0f;
        f32 shadow_near = 0.1f;
        f32 shadow_far = 50.0f;
    };

    static AppState app_state = {
        .frame_data =
            {
                .ambient_light_color = {1.0f, 1.0f, 1.0f},
                .ambient_light_intensity = 0.1f,
                .dir_light_direction = {0.25f, 1.0f, 0.1f},
                .dir_light_intensity = 3.0f,
                .dir_light_color = {1.0f, 1.0f, 1.0f},
                .num_point_lights = 1,
                .pcf_enabled = true,
                .exposure = 1.0f,
                .ssao_enabled = true,
                .ssao_radius = 0.3f,
                .ssao_bias = 0.001f,
            },
    };

    void update_ui()
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Settings");

        ImGui::Combo("Debug View", &app_state.frame_data.debug_view, DEBUG_VIEW_NAMES, IM_ARRAYSIZE(DEBUG_VIEW_NAMES));
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Ambient light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Intensity###ambient", &app_state.frame_data.ambient_light_intensity, 0.01f, 0.0f, 10.0f);
            ImGui::ColorEdit3("Color###ambient", &app_state.frame_data.ambient_light_color.x);
        }

        if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Intensity###dir", &app_state.frame_data.dir_light_intensity, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat3("Direction###dir", &app_state.frame_data.dir_light_direction.x, 0.01f, -1.0f, 1.0f);
            ImGui::ColorEdit3("Color###dir", &app_state.frame_data.dir_light_color.x);
        }

        if (ImGui::CollapsingHeader("Shadow Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enabled", std::bit_cast<bool *>(&app_state.frame_data.pcf_enabled));
            ImGui::DragFloat("Light Distance", &app_state.light_distance, 0.1f, 1.0f, 200.0f);
            ImGui::DragFloat("Shadow Range", &app_state.shadow_range, 0.1f, 1.0f, 200.0f);
            ImGui::DragFloat("Shadow Near", &app_state.shadow_near, 0.01f, 0.001f, 20.0f);
            ImGui::DragFloat("Shadow Far", &app_state.shadow_far, 0.1f, 1.0f, 500.0f);
        }

        if (ImGui::CollapsingHeader("Point lights", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderInt("Count", reinterpret_cast<int *>(&app_state.frame_data.num_point_lights), 0,
                             MAX_POINT_LIGHTS);

            for (u32 i = 0; i < app_state.frame_data.num_point_lights; ++i)
            {
                PointLight & light = app_state.frame_data.point_lights[i];

                ImGui::PushID(static_cast<int>(i));

                if (ImGui::TreeNode("Point Light"))
                {
                    ImGui::DragFloat3("Position", &light.position.x, 0.05f);
                    ImGui::ColorEdit3("Color", &light.color.x);
                    ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat("Radius", &light.radius, 0.01f, 0.01f, 10.0f);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }

        if (ImGui::CollapsingHeader("Post Processing", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Exposure", &app_state.frame_data.exposure, 0.001f, 0.001f, 8.0f);
        }

        if (ImGui::CollapsingHeader("SSAO", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enabled###SSAO", std::bit_cast<bool *>(&app_state.frame_data.ssao_enabled));
            ImGui::DragFloat("Radius", &app_state.frame_data.ssao_radius, 0.001f, 0.001f, 1.0f);
            ImGui::DragFloat("Bias", &app_state.frame_data.ssao_bias, 0.001f, 0.001f, 0.1f);
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
            glm::normalize(vec3(app_state.frame_data.dir_light_direction.x, app_state.frame_data.dir_light_direction.y,
                                app_state.frame_data.dir_light_direction.z));
        mat4 const light_proj = glm::ortho(-app_state.shadow_range, app_state.shadow_range, -app_state.shadow_range,
                                           app_state.shadow_range, app_state.shadow_near, app_state.shadow_far);
        mat4 const light_view = glm::lookAt(app_state.light_distance * light_dir, {}, {0.0f, 1.0f, 0.0f});

        mat4 const cam_proj = app_state.camera.get_proj(aspect);
        mat4 const cam_view = app_state.camera.get_view();
        FrameUniforms frame = {};
        frame.camera = {
            .proj = std::bit_cast<daxa_f32mat4x4>(cam_proj),
            .inv_proj = std::bit_cast<daxa_f32mat4x4>(glm::inverse(cam_proj)),
            .view = std::bit_cast<daxa_f32mat4x4>(cam_view),
            .inv_view = std::bit_cast<daxa_f32mat4x4>(glm::inverse(cam_view)),
            .position = std::bit_cast<daxa_f32vec3>(app_state.camera.position),
        };
        frame.frame_data = app_state.frame_data;
        frame.frame_data.dir_light_direction = std::bit_cast<daxa_f32vec3>(light_dir);
        frame.frame_data.dir_light_matrix = std::bit_cast<daxa_f32mat4x4>(light_proj * light_view);
        return frame;
    }

} // namespace

int main()
{
    fmt::println("[App] Starting up");

    Window window = {
        .width = 1920,
        .height = 1080,
    };
    if (auto r = window.init(); !r)
    {
        fmt::println("[App] {}", r.error());
        return 1;
    }
    gpu.init(window);

    auto model_result = asset_manager.load_model(std::string(ASSETS_DIR) + "models/sponza-ktx.glb");
    if (!model_result)
    {
        fmt::println("[App] {}", model_result.error().message);
        gpu.device.wait_idle();
        asset_manager.cleanup();
        return 1;
    }
    Model * model = *model_result;

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

    fmt::println("[App] Shutting down");
    gpu.device.wait_idle();
    ImGui_ImplGlfw_Shutdown();
    renderer.cleanup();
    asset_manager.cleanup();
    window.cleanup();

    return 0;
}
