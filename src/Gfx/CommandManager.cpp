#include "CommandManager.h"
#include "Core/Utils/Log.h"

namespace Tumbler {

std::expected<void, CommandError> CommandManager::Init(VkDevice device, uint32_t graphicsQueueFamily) {
    m_Device = device;

    VkCommandPoolCreateInfo poolInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = graphicsQueueFamily,
    };

    if (vkCreateCommandPool(m_Device, &poolInfo, nullptr, &m_Pool) != VK_SUCCESS) {
        LOG_ERROR("vkCreateCommandPool failed");
        return std::unexpected(CommandError::PoolCreationFailed);
    }

    vkGetDeviceQueue(m_Device, graphicsQueueFamily, 0, &m_Queue);
    LOG_INFO("CommandManager initialized");
    return {};
}

void CommandManager::Shutdown() {
    if (m_Pool) {
        vkDestroyCommandPool(m_Device, m_Pool, nullptr);
        m_Pool = VK_NULL_HANDLE;
    }
    LOG_INFO("CommandManager shutdown");
}

VkCommandBuffer CommandManager::Allocate() {
    VkCommandBufferAllocateInfo allocInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = m_Pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1,
    };

    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(m_Device, &allocInfo, &cmd);
    return cmd;
}

void CommandManager::ImmediateSubmit(CommandFn&& fn) {
    VkCommandBuffer cmd = Allocate();

    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };

    vkBeginCommandBuffer(cmd, &beginInfo);
    fn(cmd);
    vkEndCommandBuffer(cmd);

    VkSubmitInfo submitInfo{
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
    };

    vkQueueSubmit(m_Queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_Queue);

    vkFreeCommandBuffers(m_Device, m_Pool, 1, &cmd);
}

void CommandManager::TransitionLayout(VkCommandBuffer cmd, VkImage image,
                                       VkImageLayout oldLayout, VkImageLayout newLayout,
                                       VkImageAspectFlags aspectMask) {
    auto accessAndStage = [](VkImageLayout layout, VkAccessFlags& access, VkPipelineStageFlags& stage) {
        switch (layout) {
        case VK_IMAGE_LAYOUT_UNDEFINED:
            access = 0; stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; break;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            access = VK_ACCESS_TRANSFER_WRITE_BIT; stage = VK_PIPELINE_STAGE_TRANSFER_BIT; break;
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            access = VK_ACCESS_TRANSFER_READ_BIT; stage = VK_PIPELINE_STAGE_TRANSFER_BIT; break;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            access = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; break;
        case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
            access = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; break;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            access = VK_ACCESS_SHADER_READ_BIT; stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; break;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            access = 0; stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; break;
        case VK_IMAGE_LAYOUT_GENERAL:
            access = VK_ACCESS_SHADER_WRITE_BIT; stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT; break;
        default:
            access = 0; stage = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT; break;
        }
    };

    VkAccessFlags srcAccess, dstAccess;
    VkPipelineStageFlags srcStage, dstStage;
    accessAndStage(oldLayout, srcAccess, srcStage);
    accessAndStage(newLayout, dstAccess, dstStage);

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = srcAccess,
        .dstAccessMask = dstAccess,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange = {
            .aspectMask = aspectMask,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(cmd, srcStage, dstStage, 0,
                         0, nullptr, 0, nullptr, 1, &barrier);
}

} // namespace Tumbler
