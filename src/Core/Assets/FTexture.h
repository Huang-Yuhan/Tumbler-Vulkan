#pragma once

#include <string>
#include <vk_mem_alloc.h>
#include <Core/Graphics/VulkanTypes.h>
#include <vulkan/vulkan.h>

class VulkanContext; // 前置声明

class FTexture {
public:
    FTexture(VulkanContext* context, const AllocatedImage &image, VkSampler sampler);
    ~FTexture();

    // 禁止拷贝 (防止资源被 double free)，允许移动
    FTexture(const FTexture&) = delete;
    FTexture& operator=(const FTexture&) = delete;
    FTexture(FTexture&& other) noexcept;
    FTexture& operator=(FTexture&& other) noexcept;

    // Getters
    [[nodiscard]] VkImageView GetImageView() const { return Image.ImageView; }
    [[nodiscard]] VkSampler GetSampler() const { return Sampler; }

private:
    void Release();

    VulkanContext* Context;
    AllocatedImage Image;
    VkSampler Sampler = VK_NULL_HANDLE;
};