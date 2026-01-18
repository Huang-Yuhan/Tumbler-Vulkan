#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"

#include <stdexcept>

void VulkanRenderer::Init(AppWindow* window) {
    // 1. 初始化 Context (GPU)
    Context.Init(window);

    // 2. 初始化 Swapchain (屏幕)
    int width, height;
    window->GetFramebufferSize(width, height);
    Swapchain.Init(&Context, width, height);

    // 3. 初始化命令系统
    InitCommands();

    // 4. 初始化同步对象
    InitSyncStructures();

    LOG_INFO("VulkanRenderer Initialized Successfully");
}

void VulkanRenderer::Cleanup() {
    VkDevice device = Context.GetDevice();

    // 等待 GPU 停工后再销毁，防止正在用的时候被删了
    vkDeviceWaitIdle(device);

    // 1. 销毁同步对象
    if (RenderFence != VK_NULL_HANDLE) {
        vkDestroyFence(device, RenderFence, nullptr);
        vkDestroySemaphore(device, RenderFinishedSemaphore, nullptr);
        vkDestroySemaphore(device, ImageAvailableSemaphore, nullptr);
    }

    // 2. 销毁命令池 (会自动释放里面的 CommandBuffer)
    if (CommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(device, CommandPool, nullptr);
    }

    // 3. 销毁模块 (顺序很重要：先 Swapchain 后 Context)
    Swapchain.Cleanup();
    Context.CleanUp();
}

void VulkanRenderer::InitCommands() {
    // 创建命令池
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    // 允许重置 CommandBuffer (因为我们要每帧重录)
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = Context.GetGraphicsQueueFamily();

    if (vkCreateCommandPool(Context.GetDevice(), &poolInfo, nullptr, &CommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

    // 分配一个主要的 CommandBuffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(Context.GetDevice(), &allocInfo, &MainCommandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate command buffers!");
    }
}

void VulkanRenderer::InitSyncStructures() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // 【关键】初始状态设为 Signaled。
    // 这样第一帧 Render 时，WaitForFences 不会卡死，而是直接通过。
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkDevice device = Context.GetDevice();
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &ImageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &RenderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &RenderFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create synchronization objects!");
    }
}

void VulkanRenderer::Render(FScene* scene) {
    VkDevice device = Context.GetDevice();

    // 1. 【CPU 等待】等待上一帧 GPU 工作完成
    // timeout 设为最大，表示死等
    vkWaitForFences(device, 1, &RenderFence, VK_TRUE, UINT64_MAX);

    // 2. 【获取画布】从交换链拿一张图
    uint32_t imageIndex;
    VkResult result = Swapchain.AcquireNextImage(ImageAvailableSemaphore, imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // TODO: 处理窗口大小改变 (Recreate Swapchain)
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    // 只有确定要画了，才重置 Fence (关门放狗)
    vkResetFences(device, 1, &RenderFence);

    // 3. 【录制命令】
    vkResetCommandBuffer(MainCommandBuffer, 0);
    RecordCommandBuffer(MainCommandBuffer, imageIndex);

    // 4. 【提交任务】告诉 GPU 开始干活
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    // 等谁？等“画布准备好” (ImageAvailableSemaphore)
    VkSemaphore waitSemaphores[] = {ImageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    // 执行什么？执行我们刚才录的命令
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &MainCommandBuffer;

    // 完事通知谁？通知“渲染结束” (RenderFinishedSemaphore)
    VkSemaphore signalSemaphores[] = {RenderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    // 提交！并且告诉 GPU：做完了把 RenderFence 打开，让 CPU 下一帧能通过
    if (vkQueueSubmit(Context.GetGraphicsQueue(), 1, &submitInfo, RenderFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    // 5. 【显示画面】
    Swapchain.PresentImage(RenderFinishedSemaphore, imageIndex);
}

void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer!");
    }

    // ========================================================
    // TODO: 这里将来会写 BeginRenderPass -> Draw -> EndRenderPass
    // 现在我们什么都不画，只是跑通流程
    // ========================================================

    if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer!");
    }
}