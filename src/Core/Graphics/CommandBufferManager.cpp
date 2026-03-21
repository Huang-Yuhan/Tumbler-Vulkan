#include "Core/Graphics/CommandBufferManager.h"
#include "Core/Graphics/VulkanContext.h"
#include "Core/Utils/Log.h"
#include "Core/Utils/VulkanUtils.h"

CommandBufferManager::CommandBufferManager() = default;

CommandBufferManager::~CommandBufferManager() {
    Cleanup();
}

CommandBufferManager::CommandBufferManager(CommandBufferManager&& other) noexcept
    : ContextRef(other.ContextRef)
    , Device(other.Device)
    , GraphicsQueue(other.GraphicsQueue)
    , CommandPool(other.CommandPool)
    , UploadFence(other.UploadFence) {
    other.ContextRef = nullptr;
    other.Device = VK_NULL_HANDLE;
    other.GraphicsQueue = VK_NULL_HANDLE;
    other.CommandPool = VK_NULL_HANDLE;
    other.UploadFence = VK_NULL_HANDLE;
}

CommandBufferManager& CommandBufferManager::operator=(CommandBufferManager&& other) noexcept {
    if (this != &other) {
        Cleanup();
        ContextRef = other.ContextRef;
        Device = other.Device;
        GraphicsQueue = other.GraphicsQueue;
        CommandPool = other.CommandPool;
        UploadFence = other.UploadFence;
        other.ContextRef = nullptr;
        other.Device = VK_NULL_HANDLE;
        other.GraphicsQueue = VK_NULL_HANDLE;
        other.CommandPool = VK_NULL_HANDLE;
        other.UploadFence = VK_NULL_HANDLE;
    }
    return *this;
}

void CommandBufferManager::Init(VulkanContext* context) {
    ContextRef = context;
    Device = context->GetDevice();
    GraphicsQueue = context->GetGraphicsQueue();

    // 创建 CommandPool
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = context->GetGraphicsQueueFamily();

    VK_CHECK(vkCreateCommandPool(Device, &poolInfo, nullptr, &CommandPool));

    // 创建上传用的 Fence
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VK_CHECK(vkCreateFence(Device, &fenceInfo, nullptr, &UploadFence));

    LOG_INFO("CommandBufferManager initialized");
}

void CommandBufferManager::Cleanup() {
    if (Device != VK_NULL_HANDLE) {
        if (UploadFence != VK_NULL_HANDLE) {
            vkDestroyFence(Device, std::exchange(UploadFence, VK_NULL_HANDLE), nullptr);
        }
        if (CommandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(Device, std::exchange(CommandPool, VK_NULL_HANDLE), nullptr);
        }
    }
    ContextRef = nullptr;
    Device = VK_NULL_HANDLE;
    GraphicsQueue = VK_NULL_HANDLE;
}

VkCommandBuffer CommandBufferManager::AllocatePrimaryCommandBuffer() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    VK_CHECK(vkAllocateCommandBuffers(Device, &allocInfo, &commandBuffer));
    return commandBuffer;
}

void CommandBufferManager::FreeCommandBuffer(VkCommandBuffer commandBuffer) {
    if (commandBuffer != VK_NULL_HANDLE) {
        vkFreeCommandBuffers(Device, CommandPool, 1, &commandBuffer);
    }
}

void CommandBufferManager::ResetCommandPool(VkCommandPoolResetFlags flags) {
    VK_CHECK(vkResetCommandPool(Device, CommandPool, flags));
}

void CommandBufferManager::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& recordFunction) {
    // 1. 临时分配 CommandBuffer
    VkCommandBuffer commandBuffer = AllocatePrimaryCommandBuffer();

    // 2. 开始录制 (一次性使用)
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // 3. 执行传入的逻辑
    recordFunction(commandBuffer);

    vkEndCommandBuffer(commandBuffer);

    // 4. 提交并等待
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VK_CHECK(vkQueueSubmit(GraphicsQueue, 1, &submitInfo, UploadFence));
    VK_CHECK(vkWaitForFences(Device, 1, &UploadFence, VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(Device, 1, &UploadFence));

    // 5. 释放
    FreeCommandBuffer(commandBuffer);
}

void CommandBufferManager::TransitionImageLayout(
    VkImage image,
    VkFormat format,
    VkImageLayout oldLayout,
    VkImageLayout newLayout) {

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        // 场景 1: 刚创建 -> 准备拷贝
        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        // 场景 2: 拷贝完 -> 准备给 Shader 读
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            throw std::invalid_argument("Unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            cmd,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    });
}

void CommandBufferManager::CopyBufferToImage(
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height) {

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(
            cmd,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );
    });
}
