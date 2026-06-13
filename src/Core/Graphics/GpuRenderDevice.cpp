#include "GpuRenderDevice.h"
#include "Core/Utils/Log.h"
#include "VulkanUtils.h"

#include <cstring>

namespace Tumbler {

bool GpuDevice::Init(VkInstance instance, VkDevice device, VkPhysicalDevice physicalDevice) {
    m_Device = device;

    VmaVulkanFunctions vulkanFunctions{};
    vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
    vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;

    VmaAllocatorCreateInfo allocatorInfo{
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
    };
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    allocatorInfo.pVulkanFunctions = &vulkanFunctions;
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_4;

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &m_Allocator));
    LOG_INFO("GpuDevice initialized (VMA)");
    return true;
}

void GpuDevice::Shutdown() {
    if (m_Allocator) {
        vmaDestroyAllocator(m_Allocator);
        m_Allocator = VK_NULL_HANDLE;
    }
}

BufferHandle GpuDevice::CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memory;

    return CreateBuffer(bufferInfo, allocInfo);
}

BufferHandle GpuDevice::CreateBuffer(const VkBufferCreateInfo& ci, const VmaAllocationCreateInfo& ai,
                                        const char* debugName) {
    BufferHandle handle;
    VmaAllocationInfo allocInfo{};

    VK_CHECK(vmaCreateBuffer(m_Allocator, &ci, &ai, &handle.Buffer, &handle.Allocation, &allocInfo));

    if (ci.usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo addrInfo{};
        addrInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        addrInfo.buffer = handle.Buffer;
        handle.Address = vkGetBufferDeviceAddress(m_Device, &addrInfo);
    }

    if (debugName) {
        vmaSetAllocationName(m_Allocator, handle.Allocation, debugName);
    }

    return handle;
}

void GpuDevice::DestroyBuffer(BufferHandle& handle) {
    if (handle.Buffer) {
        vmaDestroyBuffer(m_Allocator, handle.Buffer, handle.Allocation);
        handle.Buffer = VK_NULL_HANDLE;
        handle.Allocation = VK_NULL_HANDLE;
        handle.Address = 0;
    }
}

ImageHandle GpuDevice::CreateImage(const VkImageCreateInfo& ci, const VmaAllocationCreateInfo& ai) {
    ImageHandle handle;
    VK_CHECK(vmaCreateImage(m_Allocator, &ci, &ai, &handle.Image, &handle.Allocation, nullptr));
    return handle;
}

void GpuDevice::DestroyImage(ImageHandle& handle) {
    if (handle.Image) {
        vmaDestroyImage(m_Allocator, handle.Image, handle.Allocation);
        handle.Image = VK_NULL_HANDLE;
        handle.Allocation = VK_NULL_HANDLE;
    }
}

VkSampler GpuDevice::CreateSampler(VkFilter min, VkFilter mag, VkSamplerAddressMode addressMode, uint32_t maxLod) {
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = mag;
    samplerInfo.minFilter = min;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.maxAnisotropy = 1.0f;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = static_cast<float>(maxLod);
    samplerInfo.mipLodBias = 0.0f;

    VkSampler sampler;
    VK_CHECK(vkCreateSampler(m_Device, &samplerInfo, nullptr, &sampler));
    return sampler;
}

void GpuDevice::DestroySampler(VkSampler sampler) {
    if (sampler) {
        vkDestroySampler(m_Device, sampler, nullptr);
    }
}

} // namespace Tumbler
