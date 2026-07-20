#pragma once

#include <expected>
#include <vector>
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

namespace Tumbler {

enum class SwapchainError {
    CreateFailed,
    DepthImageFailed,
    AcquireOutOfDate,
};

class VulkanDevice;

class Swapchain {
public:
    std::expected<void, SwapchainError> Init(const VulkanDevice& device,
                                             VkSurfaceKHR surface,
                                             int width, int height);
    void Shutdown();

    // Called when window resizes
    void Recreate(int width, int height);

    VkResult AcquireNextImage(uint32_t* imageIndex, VkSemaphore semaphore, VkFence fence);
    VkResult Present(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore);

    // Accessors
    VkExtent2D  GetExtent()        const { return m_Extent; }
    VkFormat    GetFormat()        const { return m_Format; }
    uint32_t    GetImageCount()    const { return static_cast<uint32_t>(m_Images.size()); }
    VkImage     GetImage(uint32_t i)   const { return m_Images[i]; }
    VkImageView GetImageView(uint32_t i) const { return m_ImageViews[i]; }
    VkImage     GetDepthImage()       const { return m_DepthImage; }
    VkImageView GetDepthView()        const { return m_DepthView; }

    bool NeedsRecreate() const { return m_NeedsRecreate; }

private:
    void Destroy();

    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    VkFormat       m_Format    = VK_FORMAT_UNDEFINED;
    VkExtent2D     m_Extent{};

    std::vector<VkImage>     m_Images;
    std::vector<VkImageView> m_ImageViews;

    // Depth
    VkImage       m_DepthImage      = VK_NULL_HANDLE;
    VkImageView   m_DepthView       = VK_NULL_HANDLE;
    VmaAllocation  m_DepthAllocation = VK_NULL_HANDLE;

    // Cached handles
    VkDevice         m_Device         = VK_NULL_HANDLE;
    VkPhysicalDevice  m_PhysicalDevice = VK_NULL_HANDLE;
    VkSurfaceKHR     m_Surface        = VK_NULL_HANDLE;
    VmaAllocator     m_Allocator      = VK_NULL_HANDLE;
    uint32_t         m_GraphicsFamily = ~0u;
    uint32_t         m_PresentFamily  = ~0u;

    bool m_NeedsRecreate = false;
};

} // namespace Tumbler
