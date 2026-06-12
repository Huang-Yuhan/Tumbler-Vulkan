// VulkanSwapchain.h — 交换链与深度缓冲管理
//
// 职责: 管理 VkSwapchainKHR 生命周期, 创建/销毁 ImageView 和深度缓冲,
//       处理窗口 resize 重建, 提供 Acquire/Present 接口.
//
// 依赖: VulkanContext, RenderDevice
// 层级: 图形基础设施 (Phase 3)

#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

#include <vector>

namespace Tumbler {

class RenderDevice;

class VulkanSwapchain {
public:
    bool Init(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VkSurfaceKHR surface,
              uint32_t graphicsQueueFamily, uint32_t presentQueueFamily, RenderDevice& renderDevice, int width,
              int height);
    void Shutdown();
    void Recreate(int width, int height);

    VkResult AcquireNextImage(uint32_t* imageIndex, VkSemaphore semaphore, VkFence fence);
    VkResult Present(VkQueue queue, uint32_t imageIndex, VkSemaphore waitSemaphore);

    bool NeedsRecreate() const { return m_NeedsRecreate; }

    VkExtent2D GetExtent() const { return m_Extent; }
    VkFormat GetFormat() const { return m_ImageFormat; }
    uint32_t GetImageCount() const { return static_cast<uint32_t>(m_Images.size()); }
    VkImage GetImage(uint32_t index) const { return m_Images[index]; }
    VkImageView GetImageView(uint32_t index) const { return m_ImageViews[index]; }
    VkImageView GetDepthImageView() const { return m_DepthImageView; }
    uint32_t GetPresentQueueFamily() const { return m_PresentQueueFamily; }

    VulkanSwapchain() = default;
    ~VulkanSwapchain() = default;
    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

private:
    void DestroySwapchainObjects();

    VkInstance m_Instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    VkSwapchainKHR m_Swapchain = VK_NULL_HANDLE;
    RenderDevice* m_RenderDevice = nullptr;

    VkFormat m_ImageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_Extent{};

    std::vector<VkImage> m_Images;
    std::vector<VkImageView> m_ImageViews;

    VkImage m_DepthImage = VK_NULL_HANDLE;
    VkImageView m_DepthImageView = VK_NULL_HANDLE;
    VmaAllocation m_DepthAllocation = VK_NULL_HANDLE;

    bool m_NeedsRecreate = false;
    uint32_t m_GraphicsQueueFamily = 0;
    uint32_t m_PresentQueueFamily = 0;
};

} // namespace Tumbler
