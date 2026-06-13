// RenderDevice.h — GPU 资源创建/销毁 (VMA 封装)
//
// 职责: 通过 VMA 管理 Buffer/Image 生命周期, 提供简洁版 + 透传版重载,
//       Buffer 创建时自动获取 DeviceAddress.
//
// 依赖: VulkanContext
// 层级: 图形基础设施 (Phase 2)

#pragma once

#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Tumbler {

struct BufferHandle {
    VkBuffer Buffer = VK_NULL_HANDLE;
    VmaAllocation Allocation = VK_NULL_HANDLE;
    VkDeviceAddress Address = 0;
};

struct ImageHandle {
    VkImage Image = VK_NULL_HANDLE;
    VmaAllocation Allocation = VK_NULL_HANDLE;
};

class RenderDevice {
public:
    bool Init(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice);
    void Shutdown();

    VmaAllocator GetAllocator() const { return m_Allocator; }

    // Buffer — 简洁版
    BufferHandle CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory);

    // Buffer — 透传版
    BufferHandle CreateBuffer(const VkBufferCreateInfo& ci, const VmaAllocationCreateInfo& ai,
                              const char* debugName = nullptr);

    void DestroyBuffer(BufferHandle& handle);

    // Image
    ImageHandle CreateImage(const VkImageCreateInfo& ci, const VmaAllocationCreateInfo& ai);
    void DestroyImage(ImageHandle& handle);

    // Sampler
    VkSampler CreateSampler(VkFilter min, VkFilter mag, VkSamplerAddressMode addressMode, uint32_t maxLod);
    void DestroySampler(VkSampler sampler);

    RenderDevice() = default;
    ~RenderDevice() = default;
    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

private:
    VmaAllocator m_Allocator = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
};

} // namespace Tumbler
