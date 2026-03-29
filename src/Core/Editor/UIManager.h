#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class AppWindow;
class VulkanRenderer;

class UIManager {
public:
    UIManager() = default;
    ~UIManager() = default;

    // 初始化与清理
    void Init(AppWindow* window, VulkanRenderer* renderer);
    void Cleanup(VkDevice device);

    // 帧周期
    void BeginFrame();
    void EndFrame();

    // 将 UI 录制到渲染器的 CommandBuffer 中
    void RecordDrawCommands(VkCommandBuffer cmdBuffer, VulkanRenderer* renderer, uint32_t imageIndex);

private:
    VkDescriptorPool ImGuiPool = VK_NULL_HANDLE;
    VkRenderPass UIRenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> UIFramebuffers;
    
    void InitUIRenderPass(VulkanRenderer* renderer);
    void InitUIFramebuffers(VulkanRenderer* renderer);
};