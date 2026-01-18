#include "Core/Graphics/VulkanSwapchain.h"
#include "Core/Graphics/VulkanContext.h"
#include "Core/Utils/Log.h"

#include <algorithm> // for std::clamp

void VulkanSwapchain::Init(VulkanContext* context, uint32_t width, uint32_t height) {
    ContextRef = context;
    VkDevice device = context->GetDevice();
    VkPhysicalDevice physicalDevice = context->GetPhysicalDevice();
    VkSurfaceKHR surface = context->GetSurface();

    // 1. 查询 Swapchain 支持细节
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    if (formatCount != 0) {
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    if (presentModeCount != 0) {
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());
    }

    // 2. 选择最佳配置
    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(presentModes);
    VkExtent2D extent = ChooseSwapExtent(capabilities, width, height);

    // 3. 决定图片数量 (通常是 Min + 1 实现双重/三重缓冲)
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // 4. 创建 Swapchain
    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1; // 除非你是 VR，否则总是 1
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 我们直接往上画

    // 处理队列家族 (如果 Graphics 和 Present 不在同一个队列)
    // 咱们简单起见，假设大部分现代显卡都在同一个队列
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // 不透明
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE; // 被遮挡的像素不画
    createInfo.oldSwapchain = VK_NULL_HANDLE; // 暂时不处理窗口 Resize 时的旧链

    if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &Swapchain) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create swap chain!");
    }

    // 保存一下选中的格式和大小
    ImageFormat = surfaceFormat.format;
    Extent = extent;

    // 5. 获取 Images (Vulkan 已经创建好了，我们拿句柄)
    vkGetSwapchainImagesKHR(device, Swapchain, &imageCount, nullptr);
    Images.resize(imageCount);
    vkGetSwapchainImagesKHR(device, Swapchain, &imageCount, Images.data());

    // 6. 创建 ImageViews
    ImageViews.resize(Images.size());
    for (size_t i = 0; i < Images.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = Images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = ImageFormat;
        
        // 默认映射
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // 描述图像的用途 (作为颜色层，也就是 BaseMipLevel = 0)
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &ImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create image views!");
        }
    }

    LOG_INFO("Swapchain Created. Size: {}x{}, Images: {}", Extent.width, Extent.height, Images.size());
}

void VulkanSwapchain::Cleanup() {
    if (ContextRef) {
        VkDevice device = ContextRef->GetDevice();
        
        for (auto imageView : ImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        ImageViews.clear();

        if (Swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, Swapchain, nullptr);
            Swapchain = VK_NULL_HANDLE;
        }
    }
}

VkResult VulkanSwapchain::AcquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t& outImageIndex) {
    // timeout = UINT64_MAX 表示无限等待，直到有一张图可用
    return vkAcquireNextImageKHR(ContextRef->GetDevice(), Swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &outImageIndex);
}

VkResult VulkanSwapchain::PresentImage(VkSemaphore renderFinishedSemaphore, uint32_t imageIndex) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore; // 等这盏灯绿了再显示

    VkSwapchainKHR swapChains[] = {Swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    return vkQueuePresentKHR(ContextRef->GetGraphicsQueue(), &presentInfo);
}

// ==========================================
// 辅助选择函数
// ==========================================

VkSurfaceFormatKHR VulkanSwapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // 优先选 SRGB (标准颜色空间)
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    // 没找到就随便返回第一个
    return availableFormats[0];
}

VkPresentModeKHR VulkanSwapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // 优先选 Mailbox (类似三重缓冲，延迟低且无撕裂)
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }
    // 兜底选 FIFO (Vulkan 规范强制要求支持这个，相当于垂直同步)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanSwapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    } else {
        // 处理高 DPI 屏幕或窗口大小不确定的情况
        VkExtent2D actualExtent = {width, height};
        actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}