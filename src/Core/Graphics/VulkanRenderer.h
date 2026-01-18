#pragma once

#include "VulkanContext.h"
#include "VulkanSwapchain.h"
#include <vulkan/vulkan.h>
#include <vector>

// 前置声明
class AppWindow;
class FScene;
class VulkanRenderer {
public:
    VulkanRenderer() = default;
    ~VulkanRenderer() { Cleanup(); }

    // 初始化整个渲染系统
    void Init(AppWindow* window);
    void Cleanup();

    // 每一帧调用的绘制函数
    void Render(FScene* scene);

    // Getters
    VkDevice GetDevice() const { return Context.GetDevice(); }

private:
    // ==========================================
    // 核心模块 (组合模式)
    // ==========================================
    VulkanContext Context;
    VulkanSwapchain Swapchain;

    // ==========================================
    // 命令相关
    // ==========================================
    VkCommandPool CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer MainCommandBuffer = VK_NULL_HANDLE;

    // ==========================================
    // 同步对象 (Synchronization)
    // ==========================================
    // 1. 图像已准备好 (Swapchain -> GPU)
    VkSemaphore ImageAvailableSemaphore = VK_NULL_HANDLE;
    // 2. 渲染已完成 (GPU -> Presentation)
    VkSemaphore RenderFinishedSemaphore = VK_NULL_HANDLE;
    // 3. CPU 等待 GPU (防止 CPU 跑太快)
    VkFence RenderFence = VK_NULL_HANDLE;

    // ==========================================
    // 内部流程
    // ==========================================
    void InitCommands();
    void InitSyncStructures();

    // 录制每一帧的命令 (目前是空的)
    void RecordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex);
};