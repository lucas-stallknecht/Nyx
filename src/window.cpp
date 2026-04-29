#include "window.hpp"

#include <fmt/base.h>

WindowInitResult Window::init()
{
    if (glfwInit() == GLFW_FALSE)
        return WindowInitResult::GLFW_Init_Failed;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    glfw_window_ptr =
        glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), "GPU Playground", nullptr, nullptr);
    if (glfw_window_ptr == nullptr)
        return WindowInitResult::Create_Window_Failed;

    glfwSetWindowUserPointer(glfw_window_ptr, this);
    glfwSetWindowSizeCallback(glfw_window_ptr,
                              [](GLFWwindow * window, int width, int height)
                              {
                                  auto * win = static_cast<Window *>(glfwGetWindowUserPointer(window));
                                  win->swapchain_out_of_date = true;
                                  win->width = static_cast<u32>(width);
                                  win->height = static_cast<u32>(height);
                                  win->minimized = (width == 0 || height == 0);
                              });
    glfwSetKeyCallback(glfw_window_ptr,
                       [](GLFWwindow * window, int key, int scancode, int action, int _)
                       {
                           auto * win = static_cast<Window *>(glfwGetWindowUserPointer(window));
                           if (key >= 0 && key < static_cast<int>(win->pressed_keys.size()))
                           {
                               win->pressed_keys[static_cast<usize>(key)] =
                                   (action == GLFW_PRESS || action == GLFW_REPEAT);
                           }
                       });
    glfwSetCursorPosCallback(glfw_window_ptr,
                             [](GLFWwindow * window, double xpos, double ypos)
                             {
                                 auto * win = static_cast<Window *>(glfwGetWindowUserPointer(window));

                                 if (win->mouse_state == MouseState::Not_Captured)
                                     return;

                                 vec2 new_pos = {static_cast<f32>(xpos), static_cast<f32>(ypos)};

                                 if (win->mouse_state == MouseState::First_Captured)
                                 {
                                     win->last_mouse_position = new_pos;
                                     win->mouse_state = MouseState::Fully_Captured;
                                     return;
                                 }
                                 win->mouse_delta = new_pos - win->last_mouse_position;
                                 win->last_mouse_position = new_pos;
                             });
    glfwSetMouseButtonCallback(glfw_window_ptr,
                               [](GLFWwindow * window, int button, int action, int _)
                               {
                                   auto * win = static_cast<Window *>(glfwGetWindowUserPointer(window));
                                   if (button != GLFW_MOUSE_BUTTON_RIGHT)
                                       return;

                                   if (action == GLFW_PRESS)
                                   {
                                       win->mouse_state = MouseState::First_Captured;
                                       glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                                   }
                                   else if (action == GLFW_RELEASE)
                                   {
                                       win->mouse_state = MouseState::Not_Captured;
                                       win->mouse_delta = {};
                                       glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                                   }
                               });
    if (glfwRawMouseMotionSupported())
    {
        glfwSetInputMode(glfw_window_ptr, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    }

    return WindowInitResult::Success;
}

void Window::cleanup()
{
    glfwDestroyWindow(glfw_window_ptr);
    glfwTerminate();
}

daxa::NativeWindowInfo Window::get_native_window_info() const
{
#if defined(_WIN32)
    return daxa::NativeWindowInfoWin32{.hwnd = glfwGetWin32Window(glfw_window_ptr)};
#elif defined(__linux__)
    switch (glfwGetPlatform())
    {
    case GLFW_PLATFORM_WAYLAND:
        return daxa::NativeWindowInfoWayland{
            .display = glfwGetWaylandDisplay(),
            .surface = glfwGetWaylandWindow(glfw_window_ptr),
            .width = size_x,
            .height = size_y,
        };
    default: return daxa::NativeWindowInfoXlib{.window = reinterpret_cast<void *>(glfwGetX11Window(glfw_window_ptr))};
    }
#endif
}

vec2 Window::consume_mouse_delta()
{
    vec2 d = mouse_delta;
    mouse_delta = {};
    return d;
}
