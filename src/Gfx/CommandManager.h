#pragma once

#include <expected>
#include <functional>
#include <vulkan/vulkan.h>

namespace Tumbler {

enum class CommandError {
    PoolCreationFailed,
};

class CommandManager {
public:
    std::expected<void, CommandError> Init(VkDevice device, uint32_t graphicsQueueFamily);
    void Shutdown();

    // Allocate one primary command buffer (short-lived, freed after submit)
    VkCommandBuffer Allocate();

    // Synchronous submit: record → submit → wait → free
    using CommandFn = std::function<void(VkCommandBuffer cmd)>;
    void ImmediateSubmit(CommandFn&& fn);

    // Insert an image layout transition barrier
    void TransitionLayout(VkCommandBuffer cmd, VkImage image,
                          VkImageLayout oldLayout, VkImageLayout newLayout,
                          VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT);

    VkQueue GetQueue() const { return m_Queue; }
    VkCommandPool GetPool() const { return m_Pool; }

private:
    VkDevice      m_Device   = VK_NULL_HANDLE;
    VkCommandPool  m_Pool     = VK_NULL_HANDLE;
    VkQueue        m_Queue    = VK_NULL_HANDLE;
};

} // namespace Tumbler
