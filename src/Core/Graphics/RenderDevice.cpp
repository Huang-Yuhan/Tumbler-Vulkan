#include "Core/Graphics/RenderDevice.h"
#include "Core/Graphics/VulkanContext.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/VulkanUtils.h"

RenderDevice::RenderDevice() = default;

RenderDevice::~RenderDevice() {
    Cleanup();
}

RenderDevice::RenderDevice(RenderDevice&& other) noexcept
    : ContextRef(other.ContextRef)
    , Device(other.Device)
    , Allocator(other.Allocator)
    , PhysicalDevice(other.PhysicalDevice) {
    other.ContextRef = nullptr;
    other.Device = VK_NULL_HANDLE;
    other.Allocator = VK_NULL_HANDLE;
    other.PhysicalDevice = VK_NULL_HANDLE;
}

RenderDevice& RenderDevice::operator=(RenderDevice&& other) noexcept {
    if (this != &other) {
        Cleanup();
        ContextRef = other.ContextRef;
        Device = other.Device;
        Allocator = other.Allocator;
        PhysicalDevice = other.PhysicalDevice;
        other.ContextRef = nullptr;
        other.Device = VK_NULL_HANDLE;
        other.Allocator = VK_NULL_HANDLE;
        other.PhysicalDevice = VK_NULL_HANDLE;
    }
    return *this;
}

void RenderDevice::Init(VulkanContext* context) {
    ContextRef = context;
    Device = context->GetDevice();
    Allocator = context->GetAllocator();
    PhysicalDevice = context->GetPhysicalDevice();

    LOG_INFO("RenderDevice initialized");
}

void RenderDevice::Cleanup() {
    // RenderDevice 不拥有这些资源，只是引用
    // 实际的清理由 VulkanContext 负责
    ContextRef = nullptr;
    Device = VK_NULL_HANDLE;
    Allocator = VK_NULL_HANDLE;
    PhysicalDevice = VK_NULL_HANDLE;
}

void RenderDevice::CreateBuffer(
    size_t size,
    VkBufferUsageFlags usage,
    VmaMemoryUsage memoryUsage,
    AllocatedBuffer& outBuffer) {

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = memoryUsage;

    // 如果是 Staging Buffer (CPU可写)，我们需要 Map 权限
    if (memoryUsage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST) {
        vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VK_CHECK(vmaCreateBuffer(Allocator, &bufferInfo, &vmaAllocInfo,
        &outBuffer.Buffer, &outBuffer.Allocation, &outBuffer.Info));

    LOG_INFO("+++ VMA ALLOC Buffer: Size = {}, Handle = {}", size, (void*)outBuffer.Buffer);
}

void RenderDevice::DestroyBuffer(AllocatedBuffer& buffer) {
    if (buffer.Buffer != VK_NULL_HANDLE && buffer.Allocation != VK_NULL_HANDLE) {
        vmaDestroyBuffer(Allocator, buffer.Buffer, buffer.Allocation);
        LOG_INFO("--- VMA FREE Buffer: Handle = {}", (void*)buffer.Buffer);
        buffer.Buffer = VK_NULL_HANDLE;
        buffer.Allocation = VK_NULL_HANDLE;
    } else {
        LOG_WARN("--- VMA FREE Warning: Attempting to free null buffer");
    }
}

void RenderDevice::CreateImage(
    uint32_t width,
    uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    AllocatedImage& outImage) {

    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { width, height, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    VK_CHECK(vmaCreateImage(Allocator, &imageInfo, &allocInfo,
        &outImage.Image, &outImage.Allocation, nullptr));
}

void RenderDevice::DestroyImage(AllocatedImage& image) {
    if (image.Image != VK_NULL_HANDLE && image.Allocation != VK_NULL_HANDLE) {
        vmaDestroyImage(Allocator, image.Image, image.Allocation);
        image.Image = VK_NULL_HANDLE;
        image.Allocation = VK_NULL_HANDLE;
    }
}

VkImageView RenderDevice::CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags) {
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    VkImageView imageView;
    VK_CHECK(vkCreateImageView(Device, &viewInfo, nullptr, &imageView));
    return imageView;
}

void RenderDevice::DestroyImageView(VkImageView imageView) {
    if (imageView != VK_NULL_HANDLE) {
        vkDestroyImageView(Device, imageView, nullptr);
    }
}

VkSampler RenderDevice::CreateSampler(
    VkFilter magFilter,
    VkFilter minFilter,
    VkSamplerAddressMode addressMode,
    bool anisotropyEnable) {

    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = magFilter;
    samplerInfo.minFilter = minFilter;
    samplerInfo.addressModeU = addressMode;
    samplerInfo.addressModeV = addressMode;
    samplerInfo.addressModeW = addressMode;
    samplerInfo.anisotropyEnable = anisotropyEnable ? VK_TRUE : VK_FALSE;

    if (anisotropyEnable) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(PhysicalDevice, &properties);
        samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;
    }

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    VkSampler sampler;
    VK_CHECK(vkCreateSampler(Device, &samplerInfo, nullptr, &sampler));
    return sampler;
}

void RenderDevice::DestroySampler(VkSampler sampler) {
    if (sampler != VK_NULL_HANDLE) {
        vkDestroySampler(Device, sampler, nullptr);
    }
}
