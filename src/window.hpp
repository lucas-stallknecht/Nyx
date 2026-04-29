#pragma once

#include "types.hpp"
#include <daxa/daxa.hpp>

#include <GLFW/glfw3.h>

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_NATIVE_INCLUDE_NONE
using HWND = void *;
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#include <GLFW/glfw3native.h>
#include <glm/glm.hpp>

enum class MouseState
{
    Not_Captured,
    First_Captured,
    Fully_Captured,
};

enum class WindowInitResult
{
    Success,
    GLFW_Init_Failed,
    Create_Window_Failed
};

struct Window
{
    Window(u32 width, u32 height) : width(width), height(height) {};

    u32 width, height;
    GLFWwindow * glfw_window_ptr = nullptr;
    bool swapchain_out_of_date = false;
    bool minimized = false;
    std::array<bool, 512> pressed_keys = {false};
    MouseState mouse_state = MouseState::Not_Captured;
    vec2 last_mouse_position = {};
    vec2 mouse_delta = {};

    WindowInitResult init();
    void cleanup();
    bool should_close() { return glfwWindowShouldClose(glfw_window_ptr); }
    void update()
    {
        glfwPollEvents();
        glfwSwapBuffers(glfw_window_ptr);
    }
    daxa::NativeWindowInfo get_native_window_info() const;
    vec2 consume_mouse_delta();
};
