#include "AppWindow.h"
#include "Core/Utils/Log.h"

#include <GLFW/glfw3.h>

namespace Tumbler {

bool AppWindow::Init(const Config& cfg) {
    if (!glfwInit()) {
        LOG_ERROR("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, cfg.Visible ? GLFW_TRUE : GLFW_FALSE);
    glfwWindowHint(GLFW_RESIZABLE, cfg.Resizable ? GLFW_TRUE : GLFW_FALSE);

    m_Window = glfwCreateWindow(cfg.Width, cfg.Height, cfg.Title, nullptr, nullptr);
    if (!m_Window) {
        LOG_ERROR("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    return true;
}

void AppWindow::Shutdown() {
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    glfwTerminate();
}

bool AppWindow::ShouldClose() const {
    return glfwWindowShouldClose(m_Window);
}

void AppWindow::PollEvents() {
    glfwPollEvents();
}

VkSurfaceKHR AppWindow::CreateSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, m_Window, nullptr, &surface) != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan surface");
        return VK_NULL_HANDLE;
    }
    return surface;
}

void AppWindow::GetFramebufferSize(int* w, int* h) const {
    glfwGetFramebufferSize(m_Window, w, h);
}

GLFWwindow* AppWindow::GetHandle() const {
    return m_Window;
}

} // namespace Tumbler
