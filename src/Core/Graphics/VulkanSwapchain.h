#pragma once

#include <vulkan/vulkan.h>
#include <vector>

// 前置声明
class VulkanContext;

class VulkanSwapchain {
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain() { Cleanup(); }

    // 初始化：需要 Context 和窗口大小
    void Init(VulkanContext* context, uint32_t width, uint32_t height);
    void Cleanup();

    // ==========================================
    // 核心流程：每一帧都要调用
    // ==========================================

    // 1. 索取一张空闲的画布 (返回 imageIndex)
    VkResult AcquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t& outImageIndex);

    // 2. 把画好的画布推送到屏幕
    VkResult PresentImage(VkSemaphore renderFinishedSemaphore, uint32_t imageIndex);

    // ==========================================
    // Getters
    // ==========================================
    VkFormat GetImageFormat() const { return ImageFormat; }
    VkExtent2D GetExtent() const { return Extent; }
    const std::vector<VkImageView>& GetImageViews() const { return ImageViews; }
    size_t GetImageCount() const { return Images.size(); }

private:
    // 持有 Context 的指针，方便调用 vkDevice
    VulkanContext* ContextRef = nullptr;

    VkSwapchainKHR Swapchain = VK_NULL_HANDLE;

    // 交换链里的图片 (由 Vulkan 自动创建)
    std::vector<VkImage> Images;

    // 图片的“身份证” (我们需要手动创建)
    std::vector<VkImageView> ImageViews;

    VkFormat ImageFormat;
    VkExtent2D Extent;

    // 内部辅助函数
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
};