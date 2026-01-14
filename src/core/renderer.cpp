#include <core/renderer.h>

// std
#include <array>
#include <stdexcept>
#include <iostream>

namespace Tumbler {

Renderer::Renderer(VulkanWindow &window, VulkanDevice &device)
    : window{window}, device{device} {
    recreateSwapChain();
    createCommandBuffers();
}

Renderer::~Renderer() {
    freeCommandBuffers();
}

void Renderer::recreateSwapChain() {
    auto extent = window.getExtent();
    // 处理窗口最小化 (Pause)
    while (extent.width == 0 || extent.height == 0) {
        extent = window.getExtent();
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(device.getDevice());

    // 如果 SwapChain 已存在，我们需要创建一个新的
    // 注意：目前的简单实现是直接 reset，更高级的实现会把旧 swapChain 传给新构造函数以复用资源
    if (vulkanSwapChain == nullptr) {
        vulkanSwapChain = std::make_unique<VulkanSwapChain>(device, extent);
    } else {
        // 简单重建：先销毁旧的，再创建新的
        // 注意：如果 RenderPass 格式发生变化，依赖它的 Pipeline 可能会失效
        // 在目前的架构中，我们假设格式不变，或者 App 层会重新构建 Pipeline
        vulkanSwapChain = std::make_unique<VulkanSwapChain>(device, extent);
    }

    // 只有当 SwapChain 图像数量发生变化时才需要重新分配 CommandBuffers
    // 但为了简单，我们这里不重新分配，因为 MAX_FRAMES_IN_FLIGHT 是固定的
}

void Renderer::createCommandBuffers() {
    commandBuffers.resize(VulkanSwapChain::MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = device.getCommandPool();
    allocInfo.commandBufferCount = static_cast<uint32_t>(commandBuffers.size());

    if (vkAllocateCommandBuffers(device.getDevice(), &allocInfo, commandBuffers.data()) !=
        VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

void Renderer::freeCommandBuffers() {
    vkFreeCommandBuffers(
        device.getDevice(),
        device.getCommandPool(),
        static_cast<uint32_t>(commandBuffers.size()),
        commandBuffers.data());
    commandBuffers.clear();
}

VkCommandBuffer Renderer::beginFrame() {
    assert(!isFrameStarted && "Can't call beginFrame while already in progress");

    auto result = vulkanSwapChain->acquireNextImage(&currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return nullptr; // 此时不能渲染，返回 null
    }

    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    isFrameStarted = true;

    auto commandBuffer = getCurrentCommandBuffer();
    
    // 开始录制命令
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    return commandBuffer;
}

void Renderer::endFrame() {
    assert(isFrameStarted && "Can't call endFrame while frame is not in progress");

    auto commandBuffer = getCurrentCommandBuffer();

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }

    auto result = vulkanSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR ||
        window.wasWindowResized()) {
        window.resetWindowResizedFlag();
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    isFrameStarted = false;
    currentFrameIndex = (currentFrameIndex + 1) % VulkanSwapChain::MAX_FRAMES_IN_FLIGHT;
}

void Renderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer) {
    assert(isFrameStarted && "Can't call beginSwapChainRenderPass if frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame");

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = vulkanSwapChain->getRenderPass();
    renderPassInfo.framebuffer = vulkanSwapChain->getFrameBuffer(currentImageIndex);

    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = vulkanSwapChain->getSwapChainExtent();

    // 默认清屏颜色 (未来可以参数化)
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {0.01f, 0.01f, 0.01f, 1.0f}; // 背景色
    // clearValues[1].depthStencil = {1.0f, 0}; // 如果有深度缓冲

    renderPassInfo.clearValueCount = 1; 
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 自动设置 Viewport 和 Scissor
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(vulkanSwapChain->getSwapChainExtent().width);
    viewport.height = static_cast<float>(vulkanSwapChain->getSwapChainExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{{0, 0}, vulkanSwapChain->getSwapChainExtent()};
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}

void Renderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
    assert(isFrameStarted && "Can't call endSwapChainRenderPass if frame is not in progress");
    assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame");

    vkCmdEndRenderPass(commandBuffer);
}

} // namespace Tumbler