#include "FTexture.h"
#include <Core/Graphics/VulkanContext.h>
#include <utility>

FTexture::FTexture(VulkanContext* context, const AllocatedImage &image, const VkSampler sampler)
    : Context(context), Image(image), Sampler(sampler) {
}

FTexture::~FTexture() {
    if (Context) {
        VkDevice device = Context->GetDevice();
        VmaAllocator allocator = Context->GetAllocator();

        // 自动清理 Sampler
        if (Sampler != VK_NULL_HANDLE) {
            vkDestroySampler(device, Sampler, nullptr);
        }

        // 自动清理 ImageView
        if (Image.ImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, Image.ImageView, nullptr);
        }

        // 自动清理显存
        if (Image.Image != VK_NULL_HANDLE) {
            vmaDestroyImage(allocator, Image.Image, Image.Allocation);
        }
    }
}

// 移动构造函数 (Move Constructor) - 转移所有权
FTexture::FTexture(FTexture&& other) noexcept 
    : Context(other.Context), Image(other.Image), Sampler(other.Sampler) {
    
    // 把原来的置空，防止它析构时释放资源
    other.Context = nullptr;
    other.Image.Image = VK_NULL_HANDLE;
    other.Image.ImageView = VK_NULL_HANDLE;
    other.Sampler = VK_NULL_HANDLE;
}

// 移动赋值 (Move Assignment)
FTexture& FTexture::operator=(FTexture&& other) noexcept {
    if (this != &other) {
        // 先释放自己的旧资源
        this->~FTexture();
        
        // 接管新资源
        Context = other.Context;
        Image = other.Image;
        Sampler = other.Sampler;

        // 置空对方
        other.Context = nullptr;
        other.Image.Image = VK_NULL_HANDLE;
        other.Image.ImageView = VK_NULL_HANDLE;
        other.Sampler = VK_NULL_HANDLE;
    }
    return *this;
}