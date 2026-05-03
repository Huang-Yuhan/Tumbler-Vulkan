#include "FTexture.h"
#include <Core/Graphics/VulkanContext.h>
#include <utility>

FTexture::FTexture(VulkanContext* context, const AllocatedImage &image, const VkSampler sampler)
    : Context(context), Image(image), Sampler(sampler) {
}

FTexture::~FTexture()
{
    Release();
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

void FTexture::Release()
{
    if (!Context) return;
    VkDevice device = Context->GetDevice();
    VmaAllocator allocator = Context->GetAllocator();
    if (Sampler != VK_NULL_HANDLE) vkDestroySampler(device, std::exchange(Sampler, VK_NULL_HANDLE), nullptr);
    if (Image.ImageView != VK_NULL_HANDLE) vkDestroyImageView(device, std::exchange(Image.ImageView, VK_NULL_HANDLE), nullptr);
    if (Image.Image != VK_NULL_HANDLE) vmaDestroyImage(allocator, std::exchange(Image.Image, VK_NULL_HANDLE), Image.Allocation);
    Context = nullptr;
}

FTexture& FTexture::operator=(FTexture&& other) noexcept {
    if (this != &other) {
        Release();
        Context = other.Context;
        Image = other.Image;
        Sampler = other.Sampler;
        other.Context = nullptr;
        other.Image = {};
        other.Sampler = VK_NULL_HANDLE;
    }
    return *this;
}