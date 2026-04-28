#pragma once

#include <daxa/daxa.hpp>
#include "types.hpp"

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

enum WindowInitResult
{
    Success,
    GLFW_Init_Failed,
    Create_Window_Failed
};

struct Window
{
    Window(u32 width, u32 height) : width(width), height(height) {};

    GLFWwindow * glfw_window_ptr;
    u32 width, height;
    bool swapchain_out_of_date;
    bool minimized;

    WindowInitResult init();
    void cleanup();
    bool should_close() { return glfwWindowShouldClose(glfw_window_ptr); }
    void update()
    {
        glfwPollEvents();
        glfwSwapBuffers(glfw_window_ptr);
    }
    daxa::NativeWindowInfo get_native_window_info() const;
};
