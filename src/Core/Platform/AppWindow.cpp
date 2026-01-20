//
// Created by Icecream_Sarkaz on 2026/1/15.
//

#include "AppWindow.h"
#include <stdexcept>

#include "Core/Utils/Log.h"
#include "Core/Utils/VulkanUtils.h"

AppWindow::AppWindow(const AppWindowConfig& config)
    : WindowConfig(config)
{
    Init();
}

AppWindow::~AppWindow()
{
    Shutdown();
}

void AppWindow::Init()
{
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, WindowConfig.Resizable ? GLFW_TRUE : GLFW_FALSE);

    Handle = glfwCreateWindow(static_cast<int>(WindowConfig.Width),
                              static_cast<int>(WindowConfig.Height),
                              WindowConfig.Title.c_str(),
                              nullptr,
                              nullptr);
    if (!Handle) {
        glfwTerminate();
        LOG_ERROR("Failed to create GLFW window");
        throw std::runtime_error("Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(Handle, this);

    LOG_INFO("Created window: {} ({}x{})", WindowConfig.Title, WindowConfig.Width, WindowConfig.Height);

}

void AppWindow::Shutdown()
{
    if (Handle) {
        glfwDestroyWindow(Handle);
        Handle = nullptr;
    }
    glfwTerminate();
    LOG_INFO("Destroyed GLFW window");
}

void AppWindow::PollEvents()
{
    glfwPollEvents();
}

bool AppWindow::ShouldClose() const
{
    return glfwWindowShouldClose(Handle);
}

void AppWindow::GetFramebufferSize(int& width, int& height) const
{
    glfwGetFramebufferSize(Handle, &width, &height);
}

VkSurfaceKHR AppWindow::CreateSurface(VkInstance instance)
{
    VkSurfaceKHR surface;

    VK_CHECK(glfwCreateWindowSurface(instance, Handle, nullptr, &surface));
    return surface;
}