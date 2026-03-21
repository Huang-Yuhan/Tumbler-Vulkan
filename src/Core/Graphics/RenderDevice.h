#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include "VulkanTypes.h"

// 前置声明
class VulkanContext;

/**
 * @brief 渲染设备类
 * @details 负责 GPU 资源（Buffer、Image）的创建和销毁
 * @note 这是 VulkanRenderer 拆分后的子系统，职责单一
 */
class RenderDevice {
public:
    RenderDevice();
    ~RenderDevice();

    // 禁止拷贝
    RenderDevice(const RenderDevice&) = delete;
    RenderDevice& operator=(const RenderDevice&) = delete;

    // 允许移动
    RenderDevice(RenderDevice&& other) noexcept;
    RenderDevice& operator=(RenderDevice&& other) noexcept;

    /**
     * @brief 初始化渲染设备
     * @param context Vulkan 上下文
     */
    void Init(VulkanContext* context);

    /**
     * @brief 清理所有资源
     */
    void Cleanup();

    // ==========================================
    // Buffer 管理
    // ==========================================

    /**
     * @brief 创建 GPU 缓冲区
     * @param size 缓冲区大小（字节）
     * @param usage 缓冲区用途
     * @param memoryUsage 内存使用策略
     * @param outBuffer 输出缓冲区
     */
    void CreateBuffer(
        size_t size,
        VkBufferUsageFlags usage,
        VmaMemoryUsage memoryUsage,
        AllocatedBuffer& outBuffer
    );

    /**
     * @brief 销毁 GPU 缓冲区
     * @param buffer 要销毁的缓冲区
     */
    void DestroyBuffer(AllocatedBuffer& buffer);

    // ==========================================
    // Image 管理
    // ==========================================

    /**
     * @brief 创建 GPU 图像
     * @param width 图像宽度
     * @param height 图像高度
     * @param format 图像格式
     * @param tiling 图像平铺模式
     * @param usage 图像用途
     * @param outImage 输出图像
     */
    void CreateImage(
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageTiling tiling,
        VkImageUsageFlags usage,
        AllocatedImage& outImage
    );

    /**
     * @brief 销毁 GPU 图像
     * @param image 要销毁的图像
     */
    void DestroyImage(AllocatedImage& image);

    /**
     * @brief 创建图像视图
     * @param image 目标图像
     * @param format 图像格式
     * @param aspectFlags 图像方面掩码
     * @return 创建的图像视图
     */
    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

    /**
     * @brief 销毁图像视图
     * @param imageView 要销毁的图像视图
     */
    void DestroyImageView(VkImageView imageView);

    /**
     * @brief 创建采样器
     * @param magFilter 放大过滤器
     * @param minFilter 缩小过滤器
     * @param addressMode 寻址模式
     * @param anisotropyEnable 是否启用各向异性
     * @return 创建的采样器
     */
    VkSampler CreateSampler(
        VkFilter magFilter = VK_FILTER_LINEAR,
        VkFilter minFilter = VK_FILTER_LINEAR,
        VkSamplerAddressMode addressMode = VK_SAMPLER_ADDRESS_MODE_REPEAT,
        bool anisotropyEnable = true
    );

    /**
     * @brief 销毁采样器
     * @param sampler 要销毁的采样器
     */
    void DestroySampler(VkSampler sampler);

    // ==========================================
    // Getter
    // ==========================================

    [[nodiscard]] VkDevice GetDevice() const { return Device; }
    [[nodiscard]] VmaAllocator GetAllocator() const { return Allocator; }
    [[nodiscard]] VulkanContext* GetContext() const { return ContextRef; }

private:
    VulkanContext* ContextRef = nullptr;
    VkDevice Device = VK_NULL_HANDLE;
    VmaAllocator Allocator = VK_NULL_HANDLE;
    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
};
