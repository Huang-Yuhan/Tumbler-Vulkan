#pragma once

#include "VulkanDevice.h"
#include "VulkanSwapChain.h"
#include <core/VulkanWindow.h>

// std
#include <cassert>
#include <memory>
#include <vector>

namespace Tumbler {

class Renderer {
public:
    Renderer(VulkanWindow &window, VulkanDevice &device);
    ~Renderer();

    Renderer(const Renderer &) = delete;
    Renderer &operator=(const Renderer &) = delete;

    // --- 核心渲染流程 API ---
    
    // 开启新的一帧：获取图像、处理 Resize、重置并返回 CommandBuffer
    VkCommandBuffer beginFrame();
    
    // 结束当前帧：提交 CommandBuffer、呈现图像
    void endFrame();

    // 开启基于 SwapChain 的 RenderPass (自动设置 Viewport/Scissor)
    void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
    
    // 结束 RenderPass
    void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

    // --- Getters ---
    
    bool isFrameInProgress() const { return isFrameStarted; }
    
    // 获取当前帧使用的 Command Buffer (仅在 beginFrame 后有效)
    VkCommandBuffer getCurrentCommandBuffer() const {
        assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
        return commandBuffers[currentFrameIndex];
    }

    // 获取 RenderPass (用于创建 Pipeline)
    VkRenderPass getSwapChainRenderPass() const { return vulkanSwapChain->getRenderPass(); }
    
    // 获取当前的 SwapChain 图像数量
    float getAspectRatio() const { return vulkanSwapChain->extentAspectRatio(); }

    int getFrameIndex() const {
        assert(isFrameStarted && "Cannot get frame index when frame not in progress");
        return currentFrameIndex;
    }

private:
    void createCommandBuffers();
    void freeCommandBuffers();
    void recreateSwapChain();

    // 引用核心组件
    VulkanWindow &window;
    VulkanDevice &device;

    // 拥有 SwapChain
    std::unique_ptr<VulkanSwapChain> vulkanSwapChain;
    std::vector<VkCommandBuffer> commandBuffers;

    // 状态追踪
    uint32_t currentImageIndex; // SwapChain 返回的 Image Index (0, 1, 2...)
    int currentFrameIndex{0};   // 我们的逻辑帧索引 (0, 1)
    bool isFrameStarted{false};
};

} // namespace Tumbler