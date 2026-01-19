#pragma once

#include <vulkan/vulkan.h>
#include <vector>

#include "AllocatedBuffer.h"

// 前置声明
class VulkanContext;

/**
 * @brief 托管 Image 资源、显存分配以及视图的组合结构体
 * * 这是一个完整的"纹理"概念的最小封装。
 * * 包含三个核心部分：原始图像对象 + 显存句柄 + 访问视图。
 */
struct AllocatedImage {
    // ========================================================================
    // 1. 核心资源 (本体)
    // ========================================================================

    /**
     * @brief Vulkan 图像句柄 (The Raw Data)
     * * 代表图像的元数据 (宽、高、格式、Mip级别等)。
     * * 注意：它仅仅是数据的"容器"，GPU 不能直接读取它来采样，
     * 也不能直接把它绑定为 Framebuffer 的附件 (Attachment)。
     */
    VkImage Image = VK_NULL_HANDLE;

    // ========================================================================
    // 2. 图像视图 (访问接口)
    // ========================================================================

    /**
     * @brief 图像视图 (The Lens / Interface)
     * * 描述了"如何"看待上面的 VkImage。
     * * 显卡需要通过 ImageView 来访问 Image 的具体部分。
     * * 作用：
     * - 在 Shader 中采样 (combined image sampler)
     * - 作为渲染目标 (Framebuffer Attachment)
     * - 解释颜色格式 (比如将 RGBA 解释为 sRGB 或 Linear)
     * - 选取特定的 Mip Level 或 Array Layer
     */
    VkImageView ImageView = VK_NULL_HANDLE;

    // ========================================================================
    // 3. 内存管理 (物理存储)
    // ========================================================================

    /**
     * @brief VMA 内存分配句柄
     * * 代表这在显存中占据的实际空间。
     * * 销毁时需调用 vmaDestroyImage(allocator, Image, Allocation)。
     * * 注意：ImageView 需要单独销毁 (vkDestroyImageView)。
     */
    VmaAllocation Allocation = VK_NULL_HANDLE;

    // ========================================================================
    // 工具函数
    // ========================================================================

    /**
     * @brief 检查资源是否有效
     * @return true 如果图像句柄和内存分配都存在
     * * 注意：有时候我们创建一个 Image 仅用于传输数据 (Transfer Src/Dst)，
     * 此时可能并没有创建 ImageView，所以这里只检查 Image 和 Allocation。
     */
    [[nodiscard]] bool IsValid() const {
        return Image != VK_NULL_HANDLE && Allocation != VK_NULL_HANDLE;
    }

    /**
     * @brief 隐式转换为 VkImage
     * 方便直接传参给 Vulkan 函数，如 vkCmdCopyImage
     */
    operator VkImage() const { return Image; }
};

class VulkanSwapchain {
public:
    VulkanSwapchain() = default;
    ~VulkanSwapchain() { Cleanup(); }

    // 初始化：需要 Context 和窗口大小
    void Init(VulkanContext* context, uint32_t width, uint32_t height);
    void Cleanup();

    // ==========================================
    // 核心流程：每一帧都要调用
    // ==========================================

    // 1. 索取一张空闲的画布 (返回 imageIndex)
    VkResult AcquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t& outImageIndex) const;

    // 2. 把画好的画布推送到屏幕
    VkResult PresentImage(VkSemaphore renderFinishedSemaphore, uint32_t imageIndex) const;

    // ==========================================
    // Getters
    // ==========================================
    [[nodiscard]] VkFormat GetImageFormat() const { return ImageFormat; }
    [[nodiscard]] VkExtent2D GetExtent() const { return Extent; }
    [[nodiscard]] const std::vector<VkImageView>& GetImageViews() const { return ImageViews; }
    [[nodiscard]] size_t GetImageCount() const { return Images.size(); }
    [[nodiscard]] VkFormat GetDepthFormat() const { return DepthFormat; }
    [[nodiscard]] VkImageView GetDepthImageView() const { return DepthImage.ImageView; }

private:
    // 持有 Context 的指针，方便调用 vkDevice
    VulkanContext* ContextRef = nullptr;

    VkSwapchainKHR Swapchain = VK_NULL_HANDLE;

    // 交换链里的图片 (由 Vulkan 自动创建)
    std::vector<VkImage> Images;

    // 图片的“身份证” (我们需要手动创建)
    std::vector<VkImageView> ImageViews;

    VkFormat ImageFormat{};
    VkExtent2D Extent{};

    AllocatedImage DepthImage{};
    VkFormat DepthFormat{};

    void CreateImageViews();
    void CreateDepthResources();

    // 内部辅助函数
    static VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    static VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);

    static VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height);
};