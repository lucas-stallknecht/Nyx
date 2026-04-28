#include "window.hpp"

WindowInitResult Window::init()
{
    if (glfwInit() == GLFW_FALSE)
    {
        return WindowInitResult::GLFW_Init_Failed;
    }
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    glfw_window_ptr =
        glfwCreateWindow(static_cast<int>(width), static_cast<int>(height), "GPU Playground", nullptr, nullptr);
    if (glfw_window_ptr == nullptr)
    {
        return WindowInitResult::Create_Window_Failed;
    }

    glfwSetWindowUserPointer(glfw_window_ptr, this);
    glfwSetWindowSizeCallback(glfw_window_ptr,
                              [](GLFWwindow * window, int width, int height)
                              {
                                  auto * win = static_cast<Window *>(glfwGetWindowUserPointer(window));
                                  win->swapchain_out_of_date = true;
                                  win->minimized = (width == 0 || height == 0);
                              });

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
