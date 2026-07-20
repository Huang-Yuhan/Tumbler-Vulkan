#include "GpuResource.h"
#include "DeletionQueue.h"

#include <cstring>
#include <utility>

namespace Tumbler {

// ===================================================================
// GpuBuffer
// ===================================================================

GpuBuffer::GpuBuffer(VkDevice device, DeletionQueue* dq, VmaAllocator allocator,
                     VkBuffer buf, VmaAllocation alloc, VkDeviceSize size)
    : m_Device(device), m_Allocator(allocator), m_Allocation(alloc), m_Buffer(buf),
      m_Size(size), m_DeletionQueue(dq) {
    if (m_Buffer) {
        VkBufferDeviceAddressInfo addrInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = m_Buffer,
        };
        m_Address = vkGetBufferDeviceAddress(m_Device, &addrInfo);
    }
}

GpuBuffer GpuBuffer::Create(VkDevice device, VmaAllocator allocator, DeletionQueue& dq,
                            VkDeviceSize size, VkBufferUsageFlags usage,
                            const void* data) {
    if (size == 0) return {};

    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
    };

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO;

    VkBuffer buffer;
    VmaAllocation allocation;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

    GpuBuffer result(device, &dq, allocator, buffer, allocation, size);

    if (data) {
        auto staging = CreateStaging(device, allocator, dq, size);
        auto mapped = staging.Map();
        std::memcpy(mapped.data(), data, size);
        staging.Unmap();
        // Upload requires CommandManager::ImmediateSubmit — the caller
        // is responsible for copying staging → device buffer.
    }

    return result;
}

GpuBuffer GpuBuffer::CreateStaging(VkDevice device, VmaAllocator allocator, DeletionQueue& dq,
                                   VkDeviceSize size) {
    VkBufferCreateInfo bufferInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    };

    VmaAllocationCreateInfo allocInfo{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO,
    };

    VkBuffer buffer;
    VmaAllocation allocation;
    vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, nullptr);

    return GpuBuffer(device, &dq, allocator, buffer, allocation, size);
}

GpuBuffer::~GpuBuffer() { Release(); }

GpuBuffer::GpuBuffer(GpuBuffer&& o) noexcept
    : m_Device(std::exchange(o.m_Device, VK_NULL_HANDLE))
    , m_Allocator(std::exchange(o.m_Allocator, VK_NULL_HANDLE))
    , m_Allocation(std::exchange(o.m_Allocation, VK_NULL_HANDLE))
    , m_Buffer(std::exchange(o.m_Buffer, VK_NULL_HANDLE))
    , m_Size(std::exchange(o.m_Size, 0))
    , m_Address(std::exchange(o.m_Address, 0))
    , m_DeletionQueue(std::exchange(o.m_DeletionQueue, nullptr))
    , m_MappedPtr(std::exchange(o.m_MappedPtr, nullptr)) {}

GpuBuffer& GpuBuffer::operator=(GpuBuffer&& o) noexcept {
    if (this != &o) {
        Release();
        m_Device = std::exchange(o.m_Device, VK_NULL_HANDLE);
        m_Allocator = std::exchange(o.m_Allocator, VK_NULL_HANDLE);
        m_Allocation = std::exchange(o.m_Allocation, VK_NULL_HANDLE);
        m_Buffer = std::exchange(o.m_Buffer, VK_NULL_HANDLE);
        m_Size = std::exchange(o.m_Size, 0);
        m_Address = std::exchange(o.m_Address, 0);
        m_DeletionQueue = std::exchange(o.m_DeletionQueue, nullptr);
        m_MappedPtr = std::exchange(o.m_MappedPtr, nullptr);
    }
    return *this;
}

void GpuBuffer::Release() {
    if (m_Buffer && m_DeletionQueue) {
        VmaAllocator allocator = m_Allocator;
        VmaAllocation allocation = m_Allocation;
        VkBuffer buffer = m_Buffer;
        m_DeletionQueue->Enqueue([allocator, allocation, buffer]() {
            vmaDestroyBuffer(allocator, buffer, allocation);
        });
    }
    m_Allocator = VK_NULL_HANDLE;
    m_Allocation = VK_NULL_HANDLE;
    m_Buffer = VK_NULL_HANDLE;
    m_Size = 0;
    m_Address = 0;
    m_MappedPtr = nullptr;
}

std::span<std::byte> GpuBuffer::Map() {
    if (m_MappedPtr) return {static_cast<std::byte*>(m_MappedPtr), static_cast<size_t>(m_Size)};
    vmaMapMemory(m_Allocator, m_Allocation, &m_MappedPtr);
    return {static_cast<std::byte*>(m_MappedPtr), static_cast<size_t>(m_Size)};
}

void GpuBuffer::Unmap() {
    vmaUnmapMemory(m_Allocator, m_Allocation);
    m_MappedPtr = nullptr;
}

// ===================================================================
// GpuImage
// ===================================================================

GpuImage::GpuImage(VkDevice device, DeletionQueue* dq, VmaAllocator allocator,
                   VkImage img, VmaAllocation alloc, VkImageView view,
                   VkExtent3D extent, VkFormat format)
    : m_Device(device), m_Allocator(allocator), m_Allocation(alloc), m_Image(img),
      m_View(view), m_Extent(extent), m_Format(format), m_DeletionQueue(dq) {}

GpuImage GpuImage::Create(VkDevice device, VmaAllocator allocator, DeletionQueue& dq,
                          VkExtent3D extent, VkFormat format,
                          VkImageUsageFlags usage, uint32_t mipLevels) {
    VkImageCreateInfo imageInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = extent,
        .mipLevels = mipLevels,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
    };

    VmaAllocationCreateInfo allocInfo{
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
    };

    VkImage image;
    VmaAllocation allocation;
    vmaCreateImage(allocator, &imageInfo, &allocInfo, &image, &allocation, nullptr);

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT || format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
        format == VK_FORMAT_D24_UNORM_S8_UINT) {
        aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo viewInfo{
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format,
        .subresourceRange = {
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = mipLevels,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    VkImageView view;
    vkCreateImageView(device, &viewInfo, nullptr, &view);

    return GpuImage(device, &dq, allocator, image, allocation, view, extent, format);
}

GpuImage::~GpuImage() { Release(); }

GpuImage::GpuImage(GpuImage&& o) noexcept
    : m_Device(std::exchange(o.m_Device, VK_NULL_HANDLE))
    , m_Allocator(std::exchange(o.m_Allocator, VK_NULL_HANDLE))
    , m_Allocation(std::exchange(o.m_Allocation, VK_NULL_HANDLE))
    , m_Image(std::exchange(o.m_Image, VK_NULL_HANDLE))
    , m_View(std::exchange(o.m_View, VK_NULL_HANDLE))
    , m_Extent(std::exchange(o.m_Extent, VkExtent3D{}))
    , m_Format(std::exchange(o.m_Format, VK_FORMAT_UNDEFINED))
    , m_DeletionQueue(std::exchange(o.m_DeletionQueue, nullptr)) {}

GpuImage& GpuImage::operator=(GpuImage&& o) noexcept {
    if (this != &o) {
        Release();
        m_Device = std::exchange(o.m_Device, VK_NULL_HANDLE);
        m_Allocator = std::exchange(o.m_Allocator, VK_NULL_HANDLE);
        m_Allocation = std::exchange(o.m_Allocation, VK_NULL_HANDLE);
        m_Image = std::exchange(o.m_Image, VK_NULL_HANDLE);
        m_View = std::exchange(o.m_View, VK_NULL_HANDLE);
        m_Extent = std::exchange(o.m_Extent, VkExtent3D{});
        m_Format = std::exchange(o.m_Format, VK_FORMAT_UNDEFINED);
        m_DeletionQueue = std::exchange(o.m_DeletionQueue, nullptr);
    }
    return *this;
}

void GpuImage::Release() {
    if (m_Image && m_DeletionQueue) {
        VkDevice device = m_Device;
        VmaAllocator allocator = m_Allocator;
        VmaAllocation allocation = m_Allocation;
        VkImage image = m_Image;
        VkImageView view = m_View;
        m_DeletionQueue->Enqueue([device, allocator, allocation, image, view]() {
            if (view) vkDestroyImageView(device, view, nullptr);
            vmaDestroyImage(allocator, image, allocation);
        });
    }
    m_Allocator = VK_NULL_HANDLE;
    m_Allocation = VK_NULL_HANDLE;
    m_Image = VK_NULL_HANDLE;
    m_View = VK_NULL_HANDLE;
}

} // namespace Tumbler
