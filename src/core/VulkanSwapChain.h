#pragma once

#include "VulkanDevice.h"

// 引入 Vulkan 头文件
#include <vulkan/vulkan.h>

#include <string>
#include <vector>
#include <memory>

namespace Tumbler {

class VulkanSwapChain {
public:
    static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

    VulkanSwapChain(VulkanDevice &deviceRef, VkExtent2D windowExtent);
    ~VulkanSwapChain();

    // 禁止拷贝，因为管理着 Vulkan 句柄
    VulkanSwapChain(const VulkanSwapChain &) = delete;
    VulkanSwapChain &operator=(const VulkanSwapChain &) = delete;

    // --- 核心操作 ---
    
    // 获取下一张可用的图像索引
    VkResult acquireNextImage(uint32_t *imageIndex);
    
    // 提交指令并呈现图像
    VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);

    // --- Getters ---
    VkFramebuffer getFrameBuffer(int index) const { return swapChainFramebuffers[index]; }
    VkRenderPass getRenderPass() const { return renderPass; }
    VkImageView getImageView(int index) const { return swapChainImageViews[index]; }
    size_t imageCount() const { return swapChainImages.size(); }
    VkFormat getSwapChainImageFormat() const { return swapChainImageFormat; }
    VkExtent2D getSwapChainExtent() const { return swapChainExtent; }
    uint32_t width() const { return swapChainExtent.width; }
    uint32_t height() const { return swapChainExtent.height; }

    float extentAspectRatio() {
        return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
    }

private:
    void init();
    void createSwapChain();
    void createImageViews();
    void createRenderPass();
    void createFramebuffers();
    void createSyncObjects();

    // 辅助函数
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;

    std::vector<VkFramebuffer> swapChainFramebuffers;
    VkRenderPass renderPass;

    std::vector<VkImage> swapChainImages;
    std::vector<VkImageView> swapChainImageViews;

    VulkanDevice &device;
    VkExtent2D windowExtent;

    VkSwapchainKHR swapChain;

    // 同步对象 (现在由 SwapChain 管理每一帧的信号量)
    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> imagesInFlight;
    size_t currentFrame = 0;
};

} // namespace Tumbler