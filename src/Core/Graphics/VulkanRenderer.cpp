#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <stdexcept>
#include <array>
#include <fstream>

#include "VulkanPipelineBuilder.h"
#include "Core/Geometry/FMesh.h"

void VulkanRenderer::Init(AppWindow* window) {
    Context.Init(window);

    int width, height;
    window->GetFramebufferSize(width, height);
    Swapchain.Init(&Context, width, height);

    InitRenderPass();
    InitFramebuffers();

    // 管线依赖 RenderPass，必须在之后初始化
    InitGraphicsPipeline();

    InitCommands();
    InitSyncStructures();
    InitUploadSync();

    LOG_INFO("VulkanRenderer Initialized Successfully");
}

void VulkanRenderer::Cleanup() {
    VkDevice device = Context.GetDevice();
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    // 1. 清理 Mesh
    LOG_INFO("Cleaning up Mesh Cache: {} meshes", MeshCache.size());
    for (auto& [cpuMesh, gpuMesh] : MeshCache) {
        DestroyBuffer(gpuMesh.VertexBuffer);
        DestroyBuffer(gpuMesh.IndexBuffer);
    }
    MeshCache.clear();

    // 2. 清理管线 (新增)
    if (GraphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, GraphicsPipeline, nullptr);
    if (PipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, PipelineLayout, nullptr);

    // 3. 清理同步对象
    if (UploadFence != VK_NULL_HANDLE) vkDestroyFence(device, UploadFence, nullptr);
    if (RenderFence != VK_NULL_HANDLE) vkDestroyFence(device, RenderFence, nullptr);
    if (RenderFinishedSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, RenderFinishedSemaphore, nullptr);
    if (ImageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, ImageAvailableSemaphore, nullptr);

    // 4. 清理命令与缓冲
    if (CommandPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, CommandPool, nullptr);
    for (auto framebuffer : Framebuffers) vkDestroyFramebuffer(device, framebuffer, nullptr);
    Framebuffers.clear();

    if (RenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, RenderPass, nullptr);

    Swapchain.Cleanup();
    Context.Cleanup();
}

void VulkanRenderer::InitRenderPass() {
    // 1. 颜色附件 (保持不变)
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = Swapchain.GetImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // ===========================================
    // 【新增】2. 深度附件
    // ===========================================
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = Swapchain.GetDepthFormat(); // D32_SFLOAT
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // 每一帧都要清除深度！
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 渲染完我们不需要保存深度图
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1; // 索引 1
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 3. Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    // 【新增】绑定深度附件
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // 4. 依赖 (Dependencies)
    // 深度缓冲需要特殊的同步，防止还在读的时候就写入
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // 5. 创建 RenderPass
    // 数组里现在有两个 Attachment
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(Context.GetDevice(), &renderPassInfo, nullptr, &RenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create render pass!");
    }
}

void VulkanRenderer::InitFramebuffers() {
    const auto& imageViews = Swapchain.GetImageViews();
    VkExtent2D extent = Swapchain.GetExtent();

    Framebuffers.resize(imageViews.size());

    for (size_t i = 0; i < imageViews.size(); i++) {
        // 【修改】附件列表：颜色 + 深度
        std::array<VkImageView, 2> attachments = {
            imageViews[i],
            Swapchain.GetDepthImageView() // 所有的 Framebuffer 共用这一张深度图
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = RenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(Context.GetDevice(), &framebufferInfo, nullptr, &Framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}

void VulkanRenderer::InitCommands() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // 允许每帧重置
    poolInfo.queueFamilyIndex = Context.GetGraphicsQueueFamily();

    if (vkCreateCommandPool(Context.GetDevice(), &poolInfo, nullptr, &CommandPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create command pool!");
    }

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
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 初始状态为绿灯，防止第一帧死锁

    VkDevice device = Context.GetDevice();
    if (vkCreateSemaphore(device, &semaphoreInfo, nullptr, &ImageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateSemaphore(device, &semaphoreInfo, nullptr, &RenderFinishedSemaphore) != VK_SUCCESS ||
        vkCreateFence(device, &fenceInfo, nullptr, &RenderFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create synchronization objects!");
    }
}

void VulkanRenderer::Render(FVulkanMesh* mesh,const glm::mat4& transformMatrix) {
    VkDevice device = Context.GetDevice();
    vkWaitForFences(device, 1, &RenderFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = Swapchain.AcquireNextImage(ImageAvailableSemaphore, imageIndex);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swap chain image!");
    }

    vkResetFences(device, 1, &RenderFence);
    vkResetCommandBuffer(MainCommandBuffer, 0);

    // 传递 Mesh 给录制函数
    RecordCommandBuffer(MainCommandBuffer, imageIndex, mesh,transformMatrix);

    VkSubmitInfo submitInfo{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkSemaphore waitSemaphores[] = {ImageAvailableSemaphore};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &MainCommandBuffer;
    VkSemaphore signalSemaphores[] = {RenderFinishedSemaphore};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(Context.GetGraphicsQueue(), 1, &submitInfo, RenderFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to submit draw command buffer!");
    }

    Swapchain.PresentImage(RenderFinishedSemaphore, imageIndex);
}
void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer cmdBuffer, uint32_t imageIndex, FVulkanMesh* mesh, const glm::mat4& matrix) {
     // 更新 Push Constants
    VkCommandBufferBeginInfo beginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    VkRenderPassBeginInfo renderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    renderPassInfo.renderPass = RenderPass;
    renderPassInfo.framebuffer = Framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = Swapchain.GetExtent();

    // 【修改】清除值数组
    std::array<VkClearValue, 2> clearValues{};
    // 0: 颜色 (深青色)
    clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    // 1: 深度 (1.0 代表最远，所以我们清除为 1.0)
    clearValues[1].depthStencil = {1.0f, 0};

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // 1. 绑定管线
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, GraphicsPipeline);

    // 2. 绘制 Mesh
    if (mesh) {
        vkCmdPushConstants(
            cmdBuffer,
            PipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0,
            sizeof(glm::mat4),
            &matrix // 数据指针
        );
        VkBuffer vertexBuffers[] = {mesh->VertexBuffer.Buffer};
        VkDeviceSize offsets[] = {0};

        // 绑定 Vertex Buffer (Slot 0)
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);

        // 绑定 Index Buffer
        vkCmdBindIndexBuffer(cmdBuffer, mesh->IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

        // 绘制
        vkCmdDrawIndexed(cmdBuffer, mesh->IndexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmdBuffer);
    vkEndCommandBuffer(cmdBuffer);
}

void VulkanRenderer::InitUploadSync() {
    VkFenceCreateInfo uploadFenceCreateInfo{};
    uploadFenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    // 不需要 SIGNALED，因为我们是先 Submit 后 Wait

    if (vkCreateFence(Context.GetDevice(), &uploadFenceCreateInfo, nullptr, &UploadFence) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create upload fence!");
    }
}

void VulkanRenderer::CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, AllocatedBuffer& outBuffer) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaAllocInfo{};
    vmaAllocInfo.usage = memoryUsage;

    // 如果是 Staging Buffer (CPU可写)，我们需要 Map 权限
    if (memoryUsage == VMA_MEMORY_USAGE_AUTO_PREFER_HOST) {
        vmaAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    if (vmaCreateBuffer(Context.GetAllocator(), &bufferInfo, &vmaAllocInfo,
        &outBuffer.Buffer, &outBuffer.Allocation, &outBuffer.Info) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create buffer!");
        }

    // 【新增】打印日志，追踪分配
    LOG_INFO("+++ VMA ALLOC Buffer: Size = {}, Handle = {}", size, (void*)outBuffer.Buffer);
}

void VulkanRenderer::DestroyBuffer(AllocatedBuffer& buffer) {
    if (buffer.Buffer != VK_NULL_HANDLE && buffer.Allocation != VK_NULL_HANDLE) {
        vmaDestroyBuffer(Context.GetAllocator(), buffer.Buffer, buffer.Allocation);

        // 打印日志，追踪销毁
        LOG_INFO("--- VMA FREE Buffer: Handle = {}", (void*)buffer.Buffer);

        buffer.Buffer = VK_NULL_HANDLE;
        buffer.Allocation = VK_NULL_HANDLE;
    } else {
        LOG_WARN("--- VMA FREE Warning: Attempting to free null buffer");
    }
}

bool VulkanRenderer::LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule) {
    // 二进制模式读取，从文件末尾开始(ate)以便获取文件大小
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);

    if (!file.is_open()) {
        LOG_ERROR("Failed to load shader file: {}", filePath);
        return false;
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size();
    createInfo.pCode = reinterpret_cast<const uint32_t*>(buffer.data());

    if (vkCreateShaderModule(Context.GetDevice(), &createInfo, nullptr, outShaderModule) != VK_SUCCESS) {
        return false;
    }
    return true;
}

void VulkanRenderer::ImmediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function) {
    VkDevice device = Context.GetDevice();

    // 1. 临时分配 CommandBuffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = CommandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    // 2. 开始录制 (一次性使用)
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    // 3. 执行传入的逻辑
    function(commandBuffer);

    vkEndCommandBuffer(commandBuffer);

    // 4. 提交并等待
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(Context.GetGraphicsQueue(), 1, &submitInfo, UploadFence);

    vkWaitForFences(device, 1, &UploadFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &UploadFence);

    // 5. 释放
    vkFreeCommandBuffers(device, CommandPool, 1, &commandBuffer);
}

FVulkanMesh& VulkanRenderer::UploadMesh(FMesh* cpuMesh) {
    if (MeshCache.find(cpuMesh) != MeshCache.end()) return MeshCache[cpuMesh];

    FVulkanMesh gpuMesh;
    const auto& indices = cpuMesh->GetIndices();
    gpuMesh.IndexCount = static_cast<uint32_t>(indices.size());

    const auto& rawVertexData = cpuMesh->GetRawVertexData();
    size_t vSize = rawVertexData.size();
    AllocatedBuffer vStaging;
    CreateBuffer(vSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, vStaging);
    memcpy(vStaging.Info.pMappedData, rawVertexData.data(), vSize);
    CreateBuffer(vSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, gpuMesh.VertexBuffer);

    size_t iSize = indices.size() * sizeof(uint32_t);
    AllocatedBuffer iStaging;
    CreateBuffer(iSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, iStaging);
    memcpy(iStaging.Info.pMappedData, indices.data(), iSize);
    CreateBuffer(iSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE, gpuMesh.IndexBuffer);

    ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy copyRegion{};
        copyRegion.size = vSize;
        vkCmdCopyBuffer(cmd, vStaging.Buffer, gpuMesh.VertexBuffer.Buffer, 1, &copyRegion);
        copyRegion.size = iSize;
        vkCmdCopyBuffer(cmd, iStaging.Buffer, gpuMesh.IndexBuffer.Buffer, 1, &copyRegion);
    });

    DestroyBuffer(vStaging);
    DestroyBuffer(iStaging);

    MeshCache[cpuMesh] = gpuMesh;
    LOG_INFO("Uploaded Mesh. RawBytes: {}, Indices: {}", vSize, gpuMesh.IndexCount);
    return MeshCache[cpuMesh];
}

void VulkanRenderer::InitGraphicsPipeline() {
    VkDevice device = Context.GetDevice();

    // 1. 加载 Shader 二进制代码
    // 注意：请根据你的运行目录调整路径
    // 如果你在 CLion 运行，通常需要退两层目录找到 assets
    VkShaderModule vertModule, fragModule;
    if (!LoadShaderModule("./assets/shaders/example/triangle/triangle.vert.spv", &vertModule) ||
        !LoadShaderModule("./assets/shaders/example/triangle/triangle.frag.spv", &fragModule))
    {
        throw std::runtime_error("Failed to load shader modules!");
    }
    // ===========================================
    // 【修改】配置 Push Constants 范围
    // ===========================================
    VkPushConstantRange pushConstantRange{};
    pushConstantRange.offset = 0;
    pushConstantRange.size = sizeof(glm::mat4); // 一个矩阵的大小 (64字节)
    pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // 只有 Vertex Shader 用到了


    // 2. 创建 Pipeline Layout
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};

    pipelineLayoutInfo.pushConstantRangeCount = 1;
    pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &PipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create pipeline layout!");
    }

    // 3. 配置顶点输入 (Input State)
    auto builder = VulkanPipelineBuilder::Begin(PipelineLayout);

    // [关键] 绑定描述：告诉 GPU 每个顶点跨度多少字节
    // FMesh::CreatePlane 的布局是 Pos(3) + Norm(3) + UV(2) = 8 floats = 32 bytes
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = 8 * sizeof(float); // 32 bytes
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    // [关键] 属性描述：告诉 GPU Position 在哪里
    // Shader location 0 -> format vec3 -> offset 0
    VkVertexInputAttributeDescription attributeDescription{};
    attributeDescription.binding = 0;
    attributeDescription.location = 0;
    attributeDescription.format = VK_FORMAT_R32G32B32_SFLOAT;
    attributeDescription.offset = 0;

    builder.VertexInputInfo.vertexBindingDescriptionCount = 1;
    builder.VertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    builder.VertexInputInfo.vertexAttributeDescriptionCount = 1;
    builder.VertexInputInfo.pVertexAttributeDescriptions = &attributeDescription;

    // 4. 构建管线
    GraphicsPipeline = builder
        .SetShaders(vertModule, fragModule)
        .SetViewport(Swapchain.GetExtent().width, Swapchain.GetExtent().height)
        .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE) // 暂时不剔除，方便调试
        .SetMultisamplingNone()
        .EnableDepthTest(VK_TRUE,VK_COMPARE_OP_LESS)
        .SetColorBlending(true)
        .Build(device, RenderPass);

    vkDestroyShaderModule(device, vertModule, nullptr);
    vkDestroyShaderModule(device, fragModule, nullptr);
    LOG_INFO("Graphics Pipeline Created");
}