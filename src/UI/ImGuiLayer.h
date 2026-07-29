#pragma once

#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace Tumbler {

// Thin RAII wrapper around ImGui's GLFW+Vulkan backend.
// Designed for Dynamic Rendering (no render pass).
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
        uint32_t         minImageCount;   // typically 2 or 3
        VkFormat         colorFormat;
        VkFormat         depthFormat;
    };

    bool Init(const Config& config);
    void Shutdown();

    // Call each frame before building ImGui widgets
    void BeginFrame();

    // Call INSIDE a dynamic rendering pass to record ImGui draw commands
    void EndFrame(VkCommandBuffer cmd);

    // Called after swapchain recreation with the same format
    void OnSwapchainRecreate(uint32_t minImageCount);

private:
    VkDevice m_Device      = VK_NULL_HANDLE;
    bool     m_Initialized  = false;
};

} // namespace Tumbler
