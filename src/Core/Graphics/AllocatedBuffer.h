#pragma once
#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>

/**
 * @brief 托管 Buffer 及其显存分配的组合结构体
 * * 在 Vulkan 中，创建一个 VkBuffer 只是创建了一个"逻辑对象"，它不占用实际显存。
 * 我们需要手动分配内存并绑定。使用 VMA (Vulkan Memory Allocator) 时，
 * 它会同时返回 VkBuffer (逻辑) 和 VmaAllocation (物理)。
 * * 这个结构体将它们打包，确保我们不会弄丢内存句柄，导致内存泄漏。
 */
struct AllocatedBuffer
{
    // ========================================================================
    // 核心资源句柄
    // ========================================================================

    /**
     * @brief Vulkan 缓冲区句柄 (逻辑对象)
     * * 用于所有的 Vulkan 命令记录，例如：
     * - vkCmdBindVertexBuffers (作为顶点缓冲)
     * - vkCmdBindIndexBuffer (作为索引缓冲)
     * - vkCmdCopyBuffer (数据传输)
     * - Descriptor Sets (作为 UBO/SSBO 绑定)
     */
    VkBuffer Buffer = VK_NULL_HANDLE;

    /**
     * @brief VMA 内存分配句柄 (物理存储)
     * * 代表显卡显存中的实际内存块。
     * ⚠️ 重要：销毁 Buffer 时，不能直接调用 vkDestroyBuffer，
     * 必须调用 vmaDestroyBuffer(allocator, Buffer, Allocation) 来同时释放内存。
     */
    VmaAllocation Allocation = nullptr;

    // ========================================================================
    // 元数据与调试信息
    // ========================================================================

    /**
     * @brief 分配详情信息的缓存
     * * VMA 在分配时会返回关于这块内存的详细信息。
     * 常用成员：
     * - size: 实际分配的字节大小
     * - pMappedData: 如果是 CPU 可见内存且已映射，这里直接就是 CPU 指针 (void*)
     * - deviceMemory: 底层对应的 VkDeviceMemory 句柄
     */
    VmaAllocationInfo Info{};

    /**
     * @brief 检查这个 AllocatedBuffer 是否有效
     * @return 如果 Buffer 和 Allocation 都有效，返回 true；否则返回 false
     */
    [[nodiscard]] bool IsValid() const {
        return Buffer != VK_NULL_HANDLE && Allocation != nullptr;
    }

};