#include "Core/Graphics/VulkanSwapchain.h"
#include "Core/Graphics/VulkanContext.h"
#include "Core/Utils/Log.h"

#include <algorithm> // for std::clamp

#include "Core/Utils/VulkanUtils.h"

void VulkanSwapchain::Init(VulkanContext* context, uint32_t width, uint32_t height) {

    LOG_INFO("================= VulkanSwapchain Initialization Started ================");

    // 初始化一些引用
    ContextRef = context;
    VkDevice device = context->GetDevice();
    VkPhysicalDevice physicalDevice = context->GetPhysicalDevice();
    VkSurfaceKHR surface = context->GetSurface();

    // 1. 查询 Swapchain 支持细节
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);


    auto formats = VkUtils::GetVector<VkSurfaceFormatKHR>(vkGetPhysicalDeviceSurfaceFormatsKHR, physicalDevice, surface);

    auto presentModes = VkUtils::GetVector<VkPresentModeKHR>(vkGetPhysicalDeviceSurfacePresentModesKHR, physicalDevice, surface);

    // 2. 选择最佳配置
    VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(formats);
    VkPresentModeKHR presentMode = ChooseSwapPresentMode(presentModes);
    VkExtent2D extent = ChooseSwapExtent(capabilities, width, height);

    LOG_INFO("Chosen Swapchain Format: {} , ColorSpace: {}",
             VkUtils::VkFormatToString(surfaceFormat.format),
             VkUtils::VkColorSpaceToString(surfaceFormat.colorSpace));
    LOG_INFO("Chosen Present Mode: {}", VkUtils::VkPresentModeToString(presentMode));
    LOG_INFO("Chosen Swapchain Extent: {}x{}", extent.width, extent.height);


    // 3. 决定图片数量 (通常是 Min + 1 实现双重/三重缓冲)
    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    // 4. 创建 Swapchain

    auto graphicsFamilyIndex = context->GetGraphicsQueueFamily();
    auto presentFamilyIndex = context->GetPresentQueueFamily();

    uint32_t queueFamilyIndices[] = {graphicsFamilyIndex, presentFamilyIndex};

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1; // 除非你是 VR，否则总是 1
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 我们直接往上画

    if (graphicsFamilyIndex!=presentFamilyIndex)
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT; // 不用显式转换队列
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    }
    else
    {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // 性能更好
        createInfo.queueFamilyIndexCount = 0; // 可选
        createInfo.pQueueFamilyIndices = nullptr; // 可选
    }

    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // 不透明
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE; // 被遮挡的像素不画
    createInfo.oldSwapchain = VK_NULL_HANDLE; // 暂时不处理窗口 Resize 时的旧链


    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, nullptr, &Swapchain));
    LOG_INFO("Vulkan Swapchain Created");

    // 保存一下选中的格式和大小
    ImageFormat = surfaceFormat.format;
    Extent = extent;

    // 5. 获取 Images (Vulkan 已经创建好了，我们拿句柄)
    // vkGetSwapchainImagesKHR(device, Swapchain, &imageCount, nullptr);
    // Images.resize(imageCount);
    // vkGetSwapchainImagesKHR(device, Swapchain, &imageCount, Images.data());

    Images= VkUtils::GetVector<VkImage>(vkGetSwapchainImagesKHR, device, Swapchain);

    CreateImageViews();
    LOG_INFO("Vulkan Image View Created");
    CreateDepthResources();
    LOG_INFO("Vulkan Depth Resources Created");

    LOG_INFO("================= VulkanSwapchain Initialization Completed ================");
}

void VulkanSwapchain::Cleanup() {
    if (ContextRef) {
        LOG_INFO("================= VulkanSwapchain Cleanup Started ================");
        VkDevice device = ContextRef->GetDevice();
        
        for (const auto& imageView : ImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }
        ImageViews.clear();

        LOG_INFO("Image Views Destroyed");

        if (Swapchain != VK_NULL_HANDLE) {
            vkDestroySwapchainKHR(device, std::exchange(Swapchain, VK_NULL_HANDLE), nullptr);
            LOG_INFO("Swapchain Destroyed");
        }
        if (DepthImage.ImageView != VK_NULL_HANDLE) {
            vkDestroyImageView(device, std::exchange(DepthImage.ImageView, VK_NULL_HANDLE), nullptr);
            LOG_INFO("Depth Image View Destroyed");
        }
        if (DepthImage.Image != VK_NULL_HANDLE) {
            vmaDestroyImage(ContextRef->GetAllocator(), std::exchange(DepthImage.Image,VK_NULL_HANDLE), DepthImage.Allocation);
            LOG_INFO("Depth Image Destroyed");
        }

        LOG_INFO("================= VulkanSwapchain Cleanup Completed ================");
    }
}

VkResult VulkanSwapchain::AcquireNextImage(VkSemaphore imageAvailableSemaphore, uint32_t& outImageIndex) const
{
    // timeout = UINT64_MAX 表示无限等待，直到有一张图可用
    return vkAcquireNextImageKHR(ContextRef->GetDevice(), Swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &outImageIndex);
}

VkResult VulkanSwapchain::PresentImage(VkSemaphore renderFinishedSemaphore, uint32_t imageIndex) const
{
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinishedSemaphore; // 等这盏灯绿了再显示

    VkSwapchainKHR swapChains[] = {Swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    return vkQueuePresentKHR(ContextRef->GetPresentQueue(), &presentInfo);
}

// ==========================================
// 辅助选择函数
// ==========================================

void VulkanSwapchain::CreateImageViews()
{
    ImageViews.resize(Images.size());
    for (size_t i = 0; i < Images.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = Images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = ImageFormat;

        // 默认映射
        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // 描述图像的用途 (作为颜色层，也就是 BaseMipLevel = 0)
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        // if (vkCreateImageView(device, &viewInfo, nullptr, &ImageViews[i]) != VK_SUCCESS) {
        //     throw std::runtime_error("Failed to create image views!");
        // }
        VK_CHECK(vkCreateImageView(ContextRef->GetDevice(), &viewInfo, nullptr, &ImageViews[i]));
    }
}

void VulkanSwapchain::CreateDepthResources()
{// 我们要找一种显卡支持的深度格式 (通常是 D32_SFLOAT)
    DepthFormat = VK_FORMAT_D32_SFLOAT;

    VkExtent2D extent = GetExtent();

    // 1. 创建 Image 也就是显存
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = extent.width;
    imageInfo.extent.height = extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = DepthFormat;
    // Tiling Optimal 意味着让显卡用它最喜欢的内存排列方式（此时 CPU 无法直接读写，但渲染极快）
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // 用途：作为深度/模板附件，并允许作为 Input Attachment 在延迟渲染被读取
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE; // 纯 GPU 显存
    allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    if (vmaCreateImage(ContextRef->GetAllocator(), &imageInfo, &allocInfo,
        &DepthImage.Image, &DepthImage.Allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image!");
    }

    // 2. 创建 ImageView (让 Vulkan 知道怎么看这张图)
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = DepthImage.Image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = DepthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; // 这是一个深度图
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(ContextRef->GetDevice(), &viewInfo, nullptr, &DepthImage.ImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create depth image view!");
    }

    LOG_INFO("Depth Resources Created");
}

/**
 * @brief 1. 选择表面格式 (Color Format & Color Space)
 * 决定了交换链中的图片“长什么样”以及“怎么存颜色”。
 * * @return 选中的 VkSurfaceFormatKHR (通常是 B8G8R8A8_SRGB)
 */
VkSurfaceFormatKHR VulkanSwapchain::ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
    // 遍历所有支持的格式
    for (const auto& availableFormat : availableFormats) {
        // 我们寻找两个条件的完美组合：
        // 1. VK_FORMAT_B8G8R8A8_SRGB:
        //    - B8G8R8A8: 蓝绿红透，每个通道8位。这是最常见的内存布局。
        //    - SRGB: 这是一个非常关键的后缀！
        //      这表示图片在内存中存储的是“非线性”的 Gamma 校正后的颜色。
        //      如果选了它，Vulkan 会在 Shader 写入颜色时自动进行线性 -> sRGB 的转换，
        //      这对于让渲染结果看起来颜色正确（不发灰、不过曝）至关重要。
        //
        // 2. VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:
        //    - 表示这是标准的 sRGB 颜色空间（绝大多数显示器的标准）。
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    // 如果找不到完美的 SRGB 格式（虽然极少发生），
    // 为了保证程序能跑，直接返回列表里的第一个（通常也是个不错的选择，比如 UNORM 格式）。
    return availableFormats[0];
}

/**
 * @brief 2. 选择呈现模式 (Present Mode)
 * 决定了图片画好后，如何“排队”交给显示器显示。这直接影响延迟和画面撕裂。
 * * @return 选中的模式 (首选 Mailbox，保底 FIFO)
 */
VkPresentModeKHR VulkanSwapchain::ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
    // 策略：优先寻找 Mailbox 模式
    for (const auto& availablePresentMode : availablePresentModes) {
        // VK_PRESENT_MODE_MAILBOX_KHR (三重缓冲 / "快照"模式):
        // - 这是一个高性能模式。
        // - 队列里只有一个空位。如果 GPU 画得太快，显示器还没来得及拿走上一帧，
        //   GPU 不会等待（不阻塞），而是直接用最新画好的一帧“覆盖”掉队列里那张旧的。
        // - 优点：延迟极低（显示器拿到的永远是最新的），且没有画面撕裂（VSync 开启）。
        // - 缺点：这是个耗电模式，因为 GPU 会全速渲染，哪怕画出来的帧被覆盖掉了没显示。
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode;
        }
    }

    // 策略：如果不支持 Mailbox，退回 FIFO
    // VK_PRESENT_MODE_FIFO_KHR (先进先出 / 标准 VSync):
    // - Vulkan 规范强制要求所有显卡必须支持这个模式。
    // - 队列满了 GPU 就会停下来等待显示器（阻塞）。
    // - 优点：绝对没有撕裂，省电（帧率被锁在刷新率上限，如 60FPS）。
    // - 缺点：如果 GPU 这一帧画慢了，会直接错过刷新时机，瞬间掉帧；且输入延迟比 Mailbox 高。
    return VK_PRESENT_MODE_FIFO_KHR;
}

/**
 * @brief 3. 选择交换范围 (Swap Extent / Resolution)
 * 决定了 Swapchain 图片的具体像素宽/高。
 * * @param capabilities 显卡对 Surface 的能力描述
 * @param width 窗口当前的像素宽度
 * @param height 窗口当前的像素高度
 * @return 最终确定的分辨率
 */
VkExtent2D VulkanSwapchain::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, uint32_t width, uint32_t height) {
    // 情况 A: 显卡已经规定好了 Swapchain 必须多大
    // 通常在 Windows 上，capabilities.currentExtent 会直接返回窗口的确切大小。
    // 如果它不是 UINT32_MAX，说明我们没得选，必须用这个值。
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }
    // 情况 B: 显卡允许我们自己决定大小 (UINT32_MAX)
    // 这种情况常出现在高 DPI 屏幕（如 Retina 显示器）或 Wayland 等窗口系统上。
    // 此时窗口的“屏幕坐标”和“实际像素”是不一样的，我们需要根据 GLFW 传进来的像素宽高自己设定。
    else {
        VkExtent2D actualExtent = {width, height};

        // 防御性编程：必须把我们想要的大小限制在显卡支持的 Min 和 Max 之间。
        // 比如虽然我想创个 8000x8000 的图，但显卡最大只支持 4096，如果不 clamp 程序就会崩。
        actualExtent.width = std::clamp(actualExtent.width,
                                        capabilities.minImageExtent.width,
                                        capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height,
                                         capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height);

        return actualExtent;
    }
}
