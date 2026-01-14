#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <string>

namespace Tumbler
{
    class VulkanWindow
    {
    public:
        VulkanWindow(uint32_t width, uint32_t height,const std::string_view& title);
        ~VulkanWindow();

        //禁止拷贝构造和赋值
        VulkanWindow(const VulkanWindow&) = delete;
        VulkanWindow& operator=(const VulkanWindow&) = delete;

        //允许移动构造和赋值
        VulkanWindow(VulkanWindow&& other) noexcept;
        VulkanWindow& operator=(VulkanWindow&& other) noexcept;

        [[nodiscard]] GLFWwindow* getGLFWwindow()const;
        [[nodiscard]] bool shouldClose()const;
        void createSurface(VkInstance instance, VkSurfaceKHR* surface) const;

        [[nodiscard]] VkExtent2D getExtent()const;

        [[nodiscard]] bool wasWindowResized()const;
        void resetWindowResizedFlag();
    private:
        uint32_t m_width;
        uint32_t m_height;
        bool m_framebufferResized = false;
        GLFWwindow* m_window = nullptr;
        std::string m_title;

        void initWindow();
        static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    };
}