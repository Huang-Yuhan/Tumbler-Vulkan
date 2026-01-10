
#include <core/VulkanWindow.h>
#include <stdexcept>

namespace Tumbler
{
    VulkanWindow::VulkanWindow(uint32_t width, uint32_t height,const std::string_view& title)
        :m_width(width),m_height(height),m_title(title)
    {
        initWindow();
    }

    VulkanWindow::~VulkanWindow()
    {
        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }
        glfwTerminate();
    }

    VulkanWindow::VulkanWindow(VulkanWindow&& other) noexcept
        :m_width(other.m_width),m_height(other.m_height),m_framebufferResized(other.m_framebufferResized),
        m_window(other.m_window),m_title(std::move(other.m_title))
    {
        other.m_window = nullptr;
    }

    VulkanWindow& VulkanWindow::operator=(VulkanWindow&& other) noexcept
    {
        if (this != &other)
        {
            if (m_window)
            {
                glfwDestroyWindow(m_window);
            }
            m_width = other.m_width;
            m_height = other.m_height;
            m_framebufferResized = other.m_framebufferResized;
            m_window = other.m_window;
            m_title = std::move(other.m_title);
            other.m_window = nullptr;
        }
        return *this;
    }


    void VulkanWindow::initWindow()
    {
        if (!glfwInit())
        {
            throw std::runtime_error("Failed to initialize GLFW");
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        m_window = glfwCreateWindow(static_cast<int>(m_width), static_cast<int>(m_height), m_title.c_str(), nullptr, nullptr);
        if (!m_window)
        {
            glfwTerminate();
            throw std::runtime_error("Failed to create GLFW window");
        }

        glfwSetWindowUserPointer(m_window, this);
        glfwSetFramebufferSizeCallback(m_window, framebufferResizeCallback);
    }

    void VulkanWindow::createSurface(VkInstance instance, VkSurfaceKHR* surface) const
    {
        if (glfwCreateWindowSurface(instance, m_window, nullptr, surface) != VK_SUCCESS)
        {
            throw std::runtime_error("Failed to create window surface");
        }
    }

    GLFWwindow* VulkanWindow::getGLFWwindow()const
    {
        return m_window;
    }
    VkExtent2D VulkanWindow::getExtent()const
    {
        return VkExtent2D{m_width,m_height};
    }

    bool VulkanWindow::shouldClose()const
    {
        return glfwWindowShouldClose(m_window);
    }

    void VulkanWindow::framebufferResizeCallback(GLFWwindow *window, int width, int height)
    {
        auto vulkanWindow = reinterpret_cast<VulkanWindow*>(glfwGetWindowUserPointer(window));
        vulkanWindow->m_framebufferResized = true;
        vulkanWindow->m_width = static_cast<uint32_t>(width);
        vulkanWindow->m_height = static_cast<uint32_t>(height);
    }

    bool VulkanWindow::wasWindowResized()const
    {
        return m_framebufferResized;
    }

    void VulkanWindow::resetWindowResizedFlag()
    {
        m_framebufferResized = false;
    }
}
