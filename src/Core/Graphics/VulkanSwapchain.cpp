#include "VulkanSwapchain.h"
#include "Core/Utils/Log.h"
#include "RenderDevice.h"
#include "VulkanUtils.h"

#include <algorithm>
#include <vk_mem_alloc.h>

namespace Tumbler {

bool VulkanSwapchain::Init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
                           uint32_t graphicsQueueFamily, RenderDevice& renderDevice, int width, int height) {
    m_Instance = instance;
    m_PhysicalDevice = physicalDevice;
    m_Device = device;
    m_Surface = surface;
    m_RenderDevice = &renderDevice;

    Recreate(width, height);

    // 查找 Present Queue 族 (通常和 Graphics 相同)
    m_PresentQueueFamily = graphicsQueueFamily;

    LOG_INFO("VulkanSwapchain initialized ({}x{})", width, height);
    return true;
}

void VulkanSwapchain::Shutdown() {
    DestroySwapchainObjects();
    LOG_INFO("VulkanSwapchain shutdown");
}

void VulkanSwapchain::Recreate(int width, int height) {
    DestroySwapchainObjects();

    VkSurfaceCapabilitiesKHR capabilities{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &capabilities);

    // 选择表面格式: SRGB 优先, fallback UNORM
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());

    m_ImageFormat = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            m_ImageFormat = f.format;
            colorSpace = f.colorSpace;
            break;
        }
    }

    // 选择呈现模式: MAILBOX 优先, fallback FIFO
    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, nullptr);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &presentModeCount, presentModes.data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& m : presentModes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) {
            presentMode = m;
            break;
        }
    }

    // 确定 extent
    m_Extent = capabilities.currentExtent;
    if (m_Extent.width == UINT32_MAX) {
        m_Extent.width = std::clamp(static_cast<uint32_t>(width), capabilities.minImageExtent.width,
                                    capabilities.maxImageExtent.width);
        m_Extent.height = std::clamp(static_cast<uint32_t>(height), capabilities.minImageExtent.height,
                                     capabilities.maxImageExtent.height);
    }

    // Triple buffering
    uint32_t imageCount = std::max(3u, capabilities.minImageCount);
    if (capabilities.maxImageCount > 0) {
        imageCount = std::min(imageCount, capabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = m_Surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = m_ImageFormat;
    swapchainInfo.imageColorSpace = colorSpace;
    swapchainInfo.imageExtent = m_Extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.presentMode = presentMode;
    swapchainInfo.clipped = VK_TRUE;
    swapchainInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_Device, &swapchainInfo, nullptr, &m_Swapchain));

    // 获取 Swapchain Images
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
    m_Images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_Images.data());

    // 为每个 Image 创建 ImageView
    m_ImageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = m_Images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = m_ImageFormat;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_Device, &viewInfo, nullptr, &m_ImageViews[i]));
    }

    // 创建深度缓冲 (D32_SFLOAT)
    VkImageCreateInfo depthInfo{};
    depthInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthInfo.imageType = VK_IMAGE_TYPE_2D;
    depthInfo.format = VK_FORMAT_D32_SFLOAT;
    depthInfo.extent = {m_Extent.width, m_Extent.height, 1};
    depthInfo.mipLevels = 1;
    depthInfo.arrayLayers = 1;
    depthInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    depthInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    VmaAllocationCreateInfo depthAllocInfo{};
    depthAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    VK_CHECK(vmaCreateImage(m_RenderDevice->GetAllocator(), &depthInfo, &depthAllocInfo, &m_DepthImage,
                            &m_DepthAllocation, nullptr));

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = m_DepthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = VK_FORMAT_D32_SFLOAT;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.baseMipLevel = 0;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.baseArrayLayer = 0;
    depthViewInfo.subresourceRange.layerCount = 1;

    VK_CHECK(vkCreateImageView(m_Device, &depthViewInfo, nullptr, &m_DepthImageView));

    m_NeedsRecreate = false;
}

void VulkanSwapchain::DestroySwapchainObjects() {
    if (m_DepthImageView) {
        vkDestroyImageView(m_Device, m_DepthImageView, nullptr);
        m_DepthImageView = VK_NULL_HANDLE;
    }
    if (m_DepthImage) {
        vmaDestroyImage(m_RenderDevice->GetAllocator(), m_DepthImage, m_DepthAllocation);
        m_DepthImage = VK_NULL_HANDLE;
        m_DepthAllocation = VK_NULL_HANDLE;
    }

    for (auto view : m_ImageViews) {
        vkDestroyImageView(m_Device, view, nullptr);
    }
    m_ImageViews.clear();
    m_Images.clear();

    if (m_Swapchain) {
        vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr);
        m_Swapchain = VK_NULL_HANDLE;
    }
}

VkResult VulkanSwapchain::AcquireNextImage(uint32_t* imageIndex, VkSemaphore semaphore, VkFence fence) {
    VkResult result = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, semaphore, fence, imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_NeedsRecreate = true;
    }

    return result;
}

VkResult VulkanSwapchain::Present(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = waitSemaphore ? 1u : 0u;
    presentInfo.pWaitSemaphores = &waitSemaphore;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_Swapchain;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(queue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        m_NeedsRecreate = true;
    }

    return result;
}

} // namespace Tumbler
