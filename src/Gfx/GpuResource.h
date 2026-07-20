#pragma once

#include <cstdint>
#include <span>
#include <vk_mem_alloc.h>
#include <vulkan/vulkan.h>

namespace Tumbler {

class DeletionQueue;

// ===================================================================
// GpuBuffer — RAII Vulkan buffer via VMA (move-only)
// ===================================================================
class GpuBuffer {
public:
    GpuBuffer() = default;

    static GpuBuffer Create(VkDevice device, VmaAllocator allocator,
                            DeletionQueue& deletionQueue,
                            VkDeviceSize size,
                            VkBufferUsageFlags usage,
                            const void* data = nullptr);

    static GpuBuffer CreateStaging(VkDevice device, VmaAllocator allocator,
                                   DeletionQueue& deletionQueue,
                                   VkDeviceSize size);

    ~GpuBuffer();

    // Move only
    GpuBuffer(GpuBuffer&& o) noexcept;
    GpuBuffer& operator=(GpuBuffer&& o) noexcept;
    GpuBuffer(const GpuBuffer&) = delete;
    GpuBuffer& operator=(const GpuBuffer&) = delete;

    VkBuffer Get()          const { return m_Buffer; }
    VkDeviceSize GetSize()   const { return m_Size; }
    VkDeviceAddress GetAddress() const { return m_Address; }

    std::span<std::byte> Map();
    void Unmap();

private:
    GpuBuffer(VkDevice device, DeletionQueue* dq, VmaAllocator allocator,
              VkBuffer buf, VmaAllocation alloc, VkDeviceSize size);
    void Release();

    VkDevice        m_Device      = VK_NULL_HANDLE;
    VmaAllocator    m_Allocator   = VK_NULL_HANDLE;
    VmaAllocation   m_Allocation  = VK_NULL_HANDLE;
    VkBuffer        m_Buffer      = VK_NULL_HANDLE;
    VkDeviceSize    m_Size        = 0;
    VkDeviceAddress  m_Address     = 0;
    DeletionQueue*  m_DeletionQueue = nullptr;
    void*           m_MappedPtr = nullptr;
};

// ===================================================================
// GpuImage — RAII Vulkan image via VMA (move-only)
// ===================================================================
class GpuImage {
public:
    GpuImage() = default;

    static GpuImage Create(VkDevice device, VmaAllocator allocator,
                           DeletionQueue& deletionQueue,
                           VkExtent3D extent, VkFormat format,
                           VkImageUsageFlags usage, uint32_t mipLevels = 1);

    ~GpuImage();

    // Move only
    GpuImage(GpuImage&& o) noexcept;
    GpuImage& operator=(GpuImage&& o) noexcept;
    GpuImage(const GpuImage&) = delete;
    GpuImage& operator=(const GpuImage&) = delete;

    VkImage     Get()   const { return m_Image; }
    VkImageView GetView() const { return m_View; }
    VkExtent3D  GetExtent() const { return m_Extent; }
    VkFormat    GetFormat() const { return m_Format; }

private:
    GpuImage(VkDevice device, DeletionQueue* dq, VmaAllocator allocator,
             VkImage img, VmaAllocation alloc, VkImageView view,
             VkExtent3D extent, VkFormat format);
    void Release();

    VkDevice       m_Device      = VK_NULL_HANDLE;
    VmaAllocator   m_Allocator   = VK_NULL_HANDLE;
    VmaAllocation  m_Allocation  = VK_NULL_HANDLE;
    VkImage        m_Image       = VK_NULL_HANDLE;
    VkImageView    m_View        = VK_NULL_HANDLE;
    VkExtent3D     m_Extent{};
    VkFormat       m_Format      = VK_FORMAT_UNDEFINED;
    DeletionQueue* m_DeletionQueue = nullptr;
};

} // namespace Tumbler
