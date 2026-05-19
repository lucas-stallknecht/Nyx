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
#include <imgui_impl_glfw.h>
#include <imgui.h>

namespace
{
    void update_ui(f32 dt, int total_triangle_count, Scene & scene, Renderer & renderer)
    {
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::Begin("Settings");
        ImGui::Text("%.1f ms/frame (%.1f FPS)", static_cast<double>(1000.0f * dt), static_cast<double>(1.0f / dt));
        ImGui::Text("Draw calls: %i", gpu.stats.drawcall_count);
        ImGui::Text("Triangle count: %i / %i ", gpu.stats.triangle_count, total_triangle_count);
        ImGui::Separator();
        ImGui::Spacing();
        ImGui::Combo("Debug View", &renderer.frame_data.debug_view, DEBUG_VIEW_NAMES, IM_ARRAYSIZE(DEBUG_VIEW_NAMES));
        ImGui::Checkbox("Draw Bounding Boxes", &scene.draw_aabb);
        ImGui::Spacing();
        ImGui::Spacing();
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("Ambient light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Intensity###ambient", &renderer.frame_data.ambient_light_intensity, 0.01f, 0.0f, 10.0f);
            ImGui::ColorEdit3("Color###ambient", &renderer.frame_data.ambient_light_color.x);
        }

        if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::DragFloat("Intensity###dir", &renderer.frame_data.dir_light_intensity, 0.01f, 0.0f, 10.0f);
            ImGui::DragFloat3("Direction###dir", &renderer.frame_data.dir_light_direction.x, 0.01f, -1.0f, 1.0f);
            ImGui::ColorEdit3("Color###dir", &renderer.frame_data.dir_light_color.x);
        }

        if (ImGui::CollapsingHeader("Point lights", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::SliderInt("Count", reinterpret_cast<int *>(&renderer.frame_data.num_point_lights), 0,
                             MAX_POINT_LIGHTS);

            for (u32 i = 0; i < renderer.frame_data.num_point_lights; ++i)
            {
                PointLight & light = renderer.frame_data.point_lights[i];

                ImGui::PushID(static_cast<int>(i));

                if (ImGui::TreeNode("Point Light"))
                {
                    ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
                    ImGui::DragFloat3("Position", &light.position.x, 0.05f);
                    ImGui::ColorEdit3("Color", &light.color.x);
                    ImGui::DragFloat("Radius", &light.radius, 0.01f, 0.01f, 10.0f);
                    ImGui::TreePop();
                }
                ImGui::PopID();
            }
        }

        if (ImGui::CollapsingHeader("Directional Shadow", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("PCF Enabled", std::bit_cast<bool *>(&renderer.frame_data.pcf_enabled));
            ImGui::DragFloat("Light Distance", &renderer.light_distance, 0.1f, 1.0f, 200.0f);
            ImGui::DragFloat("Shadow Range", &renderer.shadow_range, 0.1f, 1.0f, 200.0f);
            ImGui::DragFloat("Shadow Near", &renderer.shadow_near, 0.01f, 0.001f, 20.0f);
            ImGui::DragFloat("Shadow Far", &renderer.shadow_far, 0.1f, 1.0f, 500.0f);
        }

        if (ImGui::CollapsingHeader("Post processing", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Bloom enabled", std::bit_cast<bool *>(&renderer.frame_data.bloom_enabled));
            ImGui::DragFloat("Bloom intensity", &renderer.frame_data.bloom_intensity, 0.001f, 0.001f, 5.0f);
            ImGui::DragFloat("Exposure", &renderer.frame_data.exposure, 0.001f, 0.001f, 8.0f);
        }

        if (ImGui::CollapsingHeader("SSAO", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enabled###SSAO", std::bit_cast<bool *>(&renderer.frame_data.ssao_enabled));
            ImGui::DragFloat("Radius", &renderer.frame_data.ssao_radius, 0.001f, 0.001f, 1.0f);
            ImGui::DragFloat("Bias", &renderer.frame_data.ssao_bias, 0.001f, 0.001f, 0.1f);
        }

        if (ImGui::CollapsingHeader("SSR", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Checkbox("Enabled###SSR", std::bit_cast<bool *>(&renderer.frame_data.ssr_enabled));
            ImGui::DragFloat("Intensity", &renderer.frame_data.ssr_reflection_intensity, 0.01f, 0.01f, 5.0f);
            ImGui::DragFloat("Min Mask", &renderer.frame_data.ssr_min_mask, 0.001f, 0.0f, 1.0f);
            ImGui::DragFloat("Max Mask", &renderer.frame_data.ssr_max_mask, 0.001f, 0.0f, 1.0f);
            ImGui::DragFloat("Screen Edge Fade", &renderer.frame_data.ssr_screen_edge_fade, 0.01f, 0.0f, 1.0f);
            ImGui::DragFloat("Max Distance", &renderer.frame_data.ssr_max_distance, 1.0f, 1.0f, 500.0f);
            ImGui::DragInt("Max Samples", &renderer.frame_data.ssr_num_samples, 1, 1, 512);
        }

        ImGui::End();
    }

    void handle_inputs(Window & window, Camera & camera, f32 dt)
    {
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
    f32    aspect = static_cast<f32>(window.width) / static_cast<f32>(window.height);
    Camera camera = {};
    camera.update_proj(aspect);

    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window.glfw_window_ptr, true);

    Renderer renderer = {};
    renderer.init(window);

    Scene                    scene = {};
    std::vector<std::string> model_names = {
        // "crytek_sponza/crytek_sponza.gltf",
        "intel_sponza/intel_sponza_main.gltf",
        "intel_sponza/intel_sponza_curtains.gltf",
    };
    int total_triangle_count = 0;
    for (auto const & name : model_names)
    {
        auto model_result = asset_manager.load_model(std::string(ASSETS_DIR) + name);
        if (!model_result)
        {
            fmt::println("[App] {}", model_result.error().message);
            continue;
        }
        total_triangle_count += scene.add_model(*(model_result.value()));
    }

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
            aspect = static_cast<f32>(window.width) / static_cast<f32>(window.height);
            camera.update_proj(aspect);
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
        f32       dt = io.DeltaTime;
        handle_inputs(window, camera, dt);
        update_ui(dt, total_triangle_count, scene, renderer);

        scene.update(camera);
        renderer.render(camera, scene);

        gpu.device.collect_garbage();
    }

    fmt::println("[App] Shutting down");
    scene.clear();
    ImGui_ImplGlfw_Shutdown();
    asset_manager.cleanup();

    return 0;
}
