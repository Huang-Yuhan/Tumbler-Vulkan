#include "Swapchain.h"
#include "VulkanDevice.h"
#include "Core/Utils/Log.h"

#include <algorithm>

#include <vk_mem_alloc.h>

namespace Tumbler {

std::expected<void, SwapchainError> Swapchain::Init(const VulkanDevice& device,
                                                     VkSurfaceKHR surface,
                                                     int width, int height) {
    m_Device         = device.GetDevice();
    m_PhysicalDevice = device.GetPhysicalDevice();
    m_Surface        = surface;
    m_GraphicsFamily = device.GetQueueFamilies().graphics;
    m_PresentFamily  = device.GetQueueFamilies().present;

    // Create VMA allocator (lightweight, will be shared later)
    VmaAllocatorCreateInfo allocatorInfo{
        .physicalDevice = m_PhysicalDevice,
        .device = m_Device,
        .instance = device.GetInstance(),
        .vulkanApiVersion = VK_API_VERSION_1_4,
    };
    vmaCreateAllocator(&allocatorInfo, &m_Allocator);

    Recreate(width, height);

    LOG_INFO("Swapchain initialized ({}x{})", width, height);
    return {};
}

void Swapchain::Shutdown() {
    Destroy();
    if (m_Allocator) {
        vmaDestroyAllocator(m_Allocator);
        m_Allocator = VK_NULL_HANDLE;
    }
    LOG_INFO("Swapchain shutdown");
}

void Swapchain::Recreate(int width, int height) {
    Destroy();

    // ---- Surface capabilities ----
    VkSurfaceCapabilitiesKHR caps{};
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_PhysicalDevice, m_Surface, &caps);

    // ---- Format: prefer SRGB, fallback UNORM ----
    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_PhysicalDevice, m_Surface, &formatCount, formats.data());

    m_Format = VK_FORMAT_B8G8R8A8_UNORM;
    VkColorSpaceKHR colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    for (const auto& f : formats) {
        if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            m_Format = f.format;
            break;
        }
    }

    // ---- Present mode: MAILBOX > FIFO ----
    uint32_t modeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &modeCount, nullptr);
    std::vector<VkPresentModeKHR> modes(modeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(m_PhysicalDevice, m_Surface, &modeCount, modes.data());

    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    for (const auto& m : modes) {
        if (m == VK_PRESENT_MODE_MAILBOX_KHR) { presentMode = m; break; }
    }

    // ---- Extent ----
    m_Extent = caps.currentExtent;
    if (m_Extent.width == UINT32_MAX) {
        m_Extent.width  = std::clamp(static_cast<uint32_t>(width),
                                     caps.minImageExtent.width, caps.maxImageExtent.width);
        m_Extent.height = std::clamp(static_cast<uint32_t>(height),
                                     caps.minImageExtent.height, caps.maxImageExtent.height);
    }

    // ---- Image count: triple buffering ----
    uint32_t imageCount = std::max(3u, caps.minImageCount);
    if (caps.maxImageCount > 0) imageCount = std::min(imageCount, caps.maxImageCount);

    // ---- Swapchain create ----
    uint32_t families[] = {m_GraphicsFamily, m_PresentFamily};
    VkSwapchainCreateInfoKHR sci{
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = m_Surface,
        .minImageCount = imageCount,
        .imageFormat = m_Format,
        .imageColorSpace = colorSpace,
        .imageExtent = m_Extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .imageSharingMode = (m_GraphicsFamily != m_PresentFamily)
                            ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = (m_GraphicsFamily != m_PresentFamily) ? 2u : 0u,
        .pQueueFamilyIndices = (m_GraphicsFamily != m_PresentFamily) ? families : nullptr,
        .preTransform = caps.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = presentMode,
        .clipped = VK_TRUE,
    };

    if (vkCreateSwapchainKHR(m_Device, &sci, nullptr, &m_Swapchain) != VK_SUCCESS) {
        LOG_ERROR("vkCreateSwapchainKHR failed");
        return;
    }

    // ---- Retrieve images ----
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, nullptr);
    m_Images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_Device, m_Swapchain, &imageCount, m_Images.data());

    // ---- Image views ----
    m_ImageViews.resize(imageCount);
    for (uint32_t i = 0; i < imageCount; i++) {
        VkImageViewCreateInfo viewInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = m_Images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = m_Format,
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };
        vkCreateImageView(m_Device, &viewInfo, nullptr, &m_ImageViews[i]);
    }

    // ---- Depth image (D32) ----
    VkImageCreateInfo depthInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .extent = {m_Extent.width, m_Extent.height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    };

    VmaAllocationCreateInfo depthAlloc{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };
    vmaCreateImage(m_Allocator, &depthInfo, &depthAlloc,
                   &m_DepthImage, &m_DepthAllocation, nullptr);

    VkImageViewCreateInfo depthView{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = m_DepthImage,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = VK_FORMAT_D32_SFLOAT,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };
    vkCreateImageView(m_Device, &depthView, nullptr, &m_DepthView);

    m_NeedsRecreate = false;
}

void Swapchain::Destroy() {
    if (m_DepthView)  { vkDestroyImageView(m_Device, m_DepthView, nullptr); m_DepthView = VK_NULL_HANDLE; }
    if (m_DepthImage) { vmaDestroyImage(m_Allocator, m_DepthImage, m_DepthAllocation);
                        m_DepthImage = VK_NULL_HANDLE; }

    for (auto v : m_ImageViews) vkDestroyImageView(m_Device, v, nullptr);
    m_ImageViews.clear();
    m_Images.clear();

    if (m_Swapchain) { vkDestroySwapchainKHR(m_Device, m_Swapchain, nullptr); m_Swapchain = VK_NULL_HANDLE; }
}

VkResult Swapchain::AcquireNextImage(uint32_t* imageIndex, VkSemaphore semaphore, VkFence fence) {
    VkResult r = vkAcquireNextImageKHR(m_Device, m_Swapchain, UINT64_MAX, semaphore, fence, imageIndex);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) m_NeedsRecreate = true;
    return r;
}

VkResult Swapchain::Present(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore) {
    VkPresentInfoKHR info{
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = waitSemaphore ? 1u : 0u,
        .pWaitSemaphores = &waitSemaphore,
        .swapchainCount = 1,
        .pSwapchains = &m_Swapchain,
        .pImageIndices = &imageIndex,
    };
    VkResult r = vkQueuePresentKHR(queue, &info);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) m_NeedsRecreate = true;
    return r;
}

} // namespace Tumbler
