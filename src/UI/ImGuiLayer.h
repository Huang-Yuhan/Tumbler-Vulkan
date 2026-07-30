#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace Tumbler {

// RAII wrapper around ImGui's GLFW+Vulkan backend.
// Enables Docking branch + Unity-style dark theme.
// Designed for Dynamic Rendering (Vulkan 1.4).
class ImGuiLayer {
public:
    ImGuiLayer() = default;
    ~ImGuiLayer() = default;

    ImGuiLayer(const ImGuiLayer&)            = delete;
    ImGuiLayer& operator=(const ImGuiLayer&) = delete;

    struct Config {
        GLFWwindow*      window;
        VkInstance       instance;
        VkPhysicalDevice physicalDevice;
        VkDevice         device;
        uint32_t         queueFamily;
        VkQueue          queue;
        uint32_t         minImageCount;
        VkFormat         colorFormat;
        VkFormat         depthFormat;
    };

    bool Init(const Config& config);
    void Shutdown();

    // Call each frame before building any ImGui widgets.
    void BeginFrame();

    // Call INSIDE a dynamic rendering pass to record ImGui draw commands.
    void EndFrame(VkCommandBuffer cmd);

    // Called after swapchain recreation.
    void OnSwapchainRecreate(uint32_t minImageCount);

private:
    void ApplyUnityTheme();
    VkDevice    m_Device      = VK_NULL_HANDLE;
    bool        m_Initialized  = false;
};

} // namespace Tumbler
