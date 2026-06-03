// CommandManager.h — 命令池管理与命令录制
//
// 职责: 管理 VkCommandPool, 分配 Primary CommandBuffer,
//       提供同步 ImmediateSubmit 和自动推导的 Image 布局转换.
//
// 依赖: VulkanContext
// 层级: 图形基础设施 (Phase 2)

#pragma once

#include <functional>
#include <vulkan/vulkan.h>

namespace Tumbler {

class CommandManager {
public:
    bool Init(VkDevice device, uint32_t graphicsQueueFamily);
    void Shutdown();

    VkCommandBuffer AllocatePrimaryCB() const;

    using CommandFn = std::function<void(VkCommandBuffer cmd)>;
    void ImmediateSubmit(CommandFn&& fn);

    void TransitionImageLayout(VkCommandBuffer cmd, VkImage image, VkImageLayout oldLayout, VkImageLayout newLayout);

    CommandManager() = default;
    ~CommandManager() = default;
    CommandManager(const CommandManager&) = delete;
    CommandManager& operator=(const CommandManager&) = delete;

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkCommandPool m_CommandPool = VK_NULL_HANDLE;
    VkQueue m_Queue = VK_NULL_HANDLE;
};

} // namespace Tumbler
