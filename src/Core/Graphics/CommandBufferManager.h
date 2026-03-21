#pragma once

#include <vulkan/vulkan.h>
#include <functional>

// 前置声明
class VulkanContext;
class RenderDevice;

/**
 * @brief 命令缓冲区管理器
 * @details 负责 CommandPool 管理和即时提交功能
 * @note 这是 VulkanRenderer 拆分后的子系统，职责单一
 */
class CommandBufferManager {
public:
    CommandBufferManager();
    ~CommandBufferManager();

    // 禁止拷贝
    CommandBufferManager(const CommandBufferManager&) = delete;
    CommandBufferManager& operator=(const CommandBufferManager&) = delete;

    // 允许移动
    CommandBufferManager(CommandBufferManager&& other) noexcept;
    CommandBufferManager& operator=(CommandBufferManager&& other) noexcept;

    /**
     * @brief 初始化命令缓冲区管理器
     * @param context Vulkan 上下文
     */
    void Init(VulkanContext* context);

    /**
     * @brief 清理所有资源
     */
    void Cleanup();

    // ==========================================
    // CommandBuffer 管理
    // ==========================================

    /**
     * @brief 分配一个主命令缓冲区
     * @return 分配的命令缓冲区
     */
    VkCommandBuffer AllocatePrimaryCommandBuffer();

    /**
     * @brief 释放命令缓冲区
     * @param commandBuffer 要释放的命令缓冲区
     */
    void FreeCommandBuffer(VkCommandBuffer commandBuffer);

    /**
     * @brief 重置命令池
     * @param flags 重置标志
     */
    void ResetCommandPool(VkCommandPoolResetFlags flags = 0);

    // ==========================================
    // 即时提交
    // ==========================================

    /**
     * @brief 立即提交命令缓冲区并等待完成
     * @param recordFunction 录制命令的回调函数
     * @note 这是一个同步操作，会阻塞直到 GPU 完成
     */
    void ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& recordFunction);

    // ==========================================
    // 图像布局转换
    // ==========================================

    /**
     * @brief 转换图像布局
     * @param image 目标图像
     * @param format 图像格式
     * @param oldLayout 旧布局
     * @param newLayout 新布局
     */
    void TransitionImageLayout(
        VkImage image,
        VkFormat format,
        VkImageLayout oldLayout,
        VkImageLayout newLayout
    );

    /**
     * @brief 复制缓冲区到图像
     * @param buffer 源缓冲区
     * @param image 目标图像
     * @param width 图像宽度
     * @param height 图像高度
     */
    void CopyBufferToImage(
        VkBuffer buffer,
        VkImage image,
        uint32_t width,
        uint32_t height
    );

    // ==========================================
    // Getter
    // ==========================================

    [[nodiscard]] VkCommandPool GetCommandPool() const { return CommandPool; }
    [[nodiscard]] VkDevice GetDevice() const { return Device; }
    [[nodiscard]] VkQueue GetGraphicsQueue() const { return GraphicsQueue; }

private:
    VulkanContext* ContextRef = nullptr;
    VkDevice Device = VK_NULL_HANDLE;
    VkQueue GraphicsQueue = VK_NULL_HANDLE;
    VkCommandPool CommandPool = VK_NULL_HANDLE;

    // 用于 ImmediateSubmit 的专用 Fence
    VkFence UploadFence = VK_NULL_HANDLE;
};
