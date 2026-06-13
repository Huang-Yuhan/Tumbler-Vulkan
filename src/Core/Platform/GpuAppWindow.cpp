#include "Core/Platform/GpuAppWindow.h"
#include "Core/Utils/Log.h"

#include <GLFW/glfw3.h>

namespace Tumbler {

bool GpuAppWindow::Init(const Config& cfg) {
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

void GpuAppWindow::Shutdown() {
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    glfwTerminate();
}

bool GpuAppWindow::ShouldClose() const {
    return glfwWindowShouldClose(m_Window);
}

void GpuAppWindow::PollEvents() {
    glfwPollEvents();
}

VkSurfaceKHR GpuAppWindow::CreateSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, m_Window, nullptr, &surface) != VK_SUCCESS) {
        LOG_ERROR("Failed to create Vulkan surface");
        return VK_NULL_HANDLE;
    }
    return surface;
}

void GpuAppWindow::GetFramebufferSize(int* w, int* h) const {
    glfwGetFramebufferSize(m_Window, w, h);
}

GLFWwindow* GpuAppWindow::GetHandle() const {
    return m_Window;
}

} // namespace Tumbler
