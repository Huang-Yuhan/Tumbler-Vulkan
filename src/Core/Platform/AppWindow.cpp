#include "AppWindow.h"
#include "Core/Utils/Log.h"

#include <GLFW/glfw3.h>

namespace Tumbler {

std::expected<void, WindowError> AppWindow::Init(int width, int height, const char* title) {
    if (!glfwInit()) {
        LOG_ERROR("glfwInit failed");
        return std::unexpected(WindowError::GLFWInitFailed);
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    m_Window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!m_Window) {
        LOG_ERROR("glfwCreateWindow failed ({}x{}, \"{}\")", width, height, title);
        glfwTerminate();
        return std::unexpected(WindowError::WindowCreationFailed);
    }

    LOG_INFO("AppWindow initialized ({}x{})", width, height);
    return {};
}

void AppWindow::Shutdown() {
    if (m_Window) {
        glfwDestroyWindow(m_Window);
        m_Window = nullptr;
    }
    glfwTerminate();
    LOG_INFO("AppWindow shutdown");
}

bool AppWindow::ShouldClose() const {
    return m_Window && glfwWindowShouldClose(m_Window);
}

void AppWindow::PollEvents() const {
    glfwPollEvents();
}

std::expected<VkSurfaceKHR, WindowError> AppWindow::CreateSurface(VkInstance instance) const {
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (glfwCreateWindowSurface(instance, m_Window, nullptr, &surface) != VK_SUCCESS) {
        LOG_ERROR("glfwCreateWindowSurface failed");
        return std::unexpected(WindowError::SurfaceCreationFailed);
    }
    return surface;
}

void AppWindow::GetFramebufferSize(int* w, int* h) const {
    glfwGetFramebufferSize(m_Window, w, h);
}

} // namespace Tumbler
