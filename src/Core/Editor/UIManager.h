#pragma once
#include "RuntimeConsole.h"

#include <vulkan/vulkan.h>
#include <vector>

class AppWindow;
class InputManager;
class VulkanRenderer;

class UIManager {
public:
    UIManager() = default;
    ~UIManager() = default;

    // 初始化与清理
    void Init(AppWindow* window, VulkanRenderer* renderer, InputManager* inputManager = nullptr);
    void Cleanup(VkDevice device);

    // 帧周期
    void TickInput();
    void BeginFrame();
    void EndFrame();
    [[nodiscard]] RuntimeConsole& GetConsole() { return Console; }
    [[nodiscard]] const RuntimeConsole& GetConsole() const { return Console; }

    // 将 UI 录制到渲染器的 CommandBuffer 中
    void RecordDrawCommands(VkCommandBuffer cmdBuffer, VulkanRenderer* renderer, uint32_t imageIndex);

private:
    VkDescriptorPool ImGuiPool = VK_NULL_HANDLE;
    VkRenderPass UIRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> UIFramebuffers;
    std::vector<VkImageView> CachedSwapchainImageViews;
    VkExtent2D CachedSwapchainExtent{};
    RuntimeConsole Console;
    
    void InitUIRenderPass(VulkanRenderer* renderer);
    void InitUIFramebuffers(VulkanRenderer* renderer);
    void RecreateUIFramebuffers(VulkanRenderer* renderer);
    void EnsureFramebuffersUpToDate(VulkanRenderer* renderer);
};
