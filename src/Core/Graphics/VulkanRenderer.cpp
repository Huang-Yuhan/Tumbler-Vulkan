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
#include "Core/Utils/VulkanUtils.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Core/Assets/FTexture.h"
#include "Core/Assets/TextureManager.h"

VulkanRenderer::VulkanRenderer()=default;

VulkanRenderer::~VulkanRenderer()
{
    Cleanup();
}

void VulkanRenderer::Init(AppWindow* window) {

    LOG_INFO("================= VulkanRenderer Initialization Started ================");

    Context.Init(window);

    int width, height;
    window->GetFramebufferSize(width, height);
    Swapchain.Init(&Context, width, height);

    InitRenderPass();
    InitFramebuffers();
    InitCommands();
    InitSyncStructures();
    InitUploadSync();

    TexManager=std::make_unique<TextureManager>(this);

    CurrentDemoTexture= TexManager->GetTexture("assets/textures/1.jpg");

    InitDescriptors();

    // 管线依赖 RenderPass，必须在之后初始化
    InitGraphicsPipeline();



    LOG_INFO("================= VulkanRenderer Initialization Completed ================");
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
    if (GraphicsPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, std::exchange(GraphicsPipeline,VK_NULL_HANDLE), nullptr);
    if (PipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, std::exchange(PipelineLayout,VK_NULL_HANDLE), nullptr);

    // 3. 清理同步对象
    if (UploadFence != VK_NULL_HANDLE) vkDestroyFence(device,std::exchange(UploadFence,VK_NULL_HANDLE), nullptr);
    if (RenderFence != VK_NULL_HANDLE) vkDestroyFence(device, std::exchange(RenderFence,VK_NULL_HANDLE), nullptr);
    if (RenderFinishedSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, std::exchange(RenderFinishedSemaphore,VK_NULL_HANDLE), nullptr);
    if (ImageAvailableSemaphore != VK_NULL_HANDLE) vkDestroySemaphore(device, std::exchange(ImageAvailableSemaphore,VK_NULL_HANDLE), nullptr);

    // 4. 清理命令与缓冲
    if (CommandPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, std::exchange(CommandPool,VK_NULL_HANDLE), nullptr);
    for (const auto& framebuffer : Framebuffers) vkDestroyFramebuffer(device, framebuffer, nullptr);
    Framebuffers.clear();

    if (RenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, std::exchange(RenderPass,VK_NULL_HANDLE), nullptr);

    if (auto descriptorPool = std::exchange(DescriptorPool, VK_NULL_HANDLE); descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
    }
    if (auto descriptorSetLayout = std::exchange(DescriptorSetLayout, VK_NULL_HANDLE); descriptorSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
    }

    CurrentDemoTexture.reset();
    TexManager->Cleanup();
    TexManager.reset();

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

    VK_CHECK(vkCreateRenderPass(Context.GetDevice(), &renderPassInfo, nullptr, &RenderPass));
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

        VK_CHECK(vkCreateFramebuffer(Context.GetDevice(), &framebufferInfo, nullptr, &Framebuffers[i]));
    }
}

void VulkanRenderer::InitCommands() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // 允许每帧重置
    poolInfo.queueFamilyIndex = Context.GetGraphicsQueueFamily();

    VK_CHECK(vkCreateCommandPool(Context.GetDevice(), &poolInfo, nullptr, &CommandPool));

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = CommandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    VK_CHECK(vkAllocateCommandBuffers(Context.GetDevice(), &allocInfo, &MainCommandBuffer));
}

void VulkanRenderer::InitSyncStructures() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 初始状态为绿灯，防止第一帧死锁

    VkDevice device = Context.GetDevice();

    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &ImageAvailableSemaphore));
    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &RenderFinishedSemaphore));
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &RenderFence));
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

    VK_CHECK(vkQueueSubmit(Context.GetGraphicsQueue(), 1, &submitInfo, RenderFence));

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

        vkCmdBindDescriptorSets(
                    cmdBuffer,
                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                    PipelineLayout,
                    0, // First Set
                    1, // DescriptorSetCount
                    &TextureDescriptorSet, // 我们的纹理集
                    0, nullptr // Dynamic Offsets
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

    VK_CHECK(vkCreateFence(Context.GetDevice(), &uploadFenceCreateInfo, nullptr, &UploadFence));
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


    VK_CHECK(vmaCreateBuffer(Context.GetAllocator(), &bufferInfo, &vmaAllocInfo,
        &outBuffer.Buffer, &outBuffer.Allocation, &outBuffer.Info));

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

    pipelineLayoutInfo.setLayoutCount = 1;
    pipelineLayoutInfo.pSetLayouts = &DescriptorSetLayout; // 绑定描述符集布局

    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &PipelineLayout));

    // 3. 配置顶点输入 (Input State)
    auto builder = VulkanPipelineBuilder::Begin(PipelineLayout);

    // [关键] 绑定描述：告诉 GPU 每个顶点跨度多少字节
    // FMesh::CreatePlane 的布局是 Pos(3) + Norm(3) + UV(2) = 8 floats = 32 bytes
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = 8 * sizeof(float); // 32 bytes
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attributeDescriptions;

    // 顶点
    attributeDescriptions.push_back({
        .location = 0,
        .binding = 0,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = 0
    });

    // 法线先跳过

    // UV
    attributeDescriptions.push_back({
        .location = 2,
        .binding = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = 6 * sizeof(float)
    });



    builder.VertexInputInfo.vertexBindingDescriptionCount = 1;
    builder.VertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
    builder.VertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
    builder.VertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();

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


std::shared_ptr<FTexture> VulkanRenderer::LoadTexture(const std::string& filePath) {
    // 1. 使用 stb_image 加载文件
    int texWidth, texHeight, texChannels;
    stbi_uc* pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

    if (!pixels) {
        // 简单的备用路径查找
        std::string fallbackPath = "../../" + filePath;
        pixels = stbi_load(fallbackPath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    }

    if (!pixels) {
        LOG_ERROR("Failed to load texture: {}", filePath);
        throw std::runtime_error("Texture load failed");
    }

    VkDeviceSize imageSize = texWidth * texHeight * 4;

    // 2. 创建 Staging Buffer
    AllocatedBuffer stagingBuffer;
    CreateBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, stagingBuffer);
    memcpy(stagingBuffer.Info.pMappedData, pixels, static_cast<size_t>(imageSize));
    stbi_image_free(pixels);

    // 3. 创建 Image (注意：变量名改为局部变量)
    AllocatedImage newImage;
    VkImageCreateInfo imageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent = { (uint32_t)texWidth, (uint32_t)texHeight, 1 };
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

    if (vmaCreateImage(Context.GetAllocator(), &imageInfo, &allocInfo,
        &newImage.Image, &newImage.Allocation, nullptr) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create texture image!");
    }

    // 4. 上传数据
    TransitionImageLayout(newImage.Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    CopyBufferToImage(stagingBuffer.Buffer, newImage.Image, texWidth, texHeight);
    TransitionImageLayout(newImage.Image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    DestroyBuffer(stagingBuffer);

    // 5. 创建 ImageView
    VkImageViewCreateInfo viewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    viewInfo.image = newImage.Image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_SRGB;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(Context.GetDevice(), &viewInfo, nullptr, &newImage.ImageView) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create image view!");
    }

    // 6. 创建 Sampler
    VkSampler newSampler;
    VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    samplerInfo.magFilter = VK_FILTER_LINEAR; // 改回 Linear，看起来平滑些
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;

    // 获取设备限制的最大各向异性
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(Context.GetPhysicalDevice(), &properties);
    samplerInfo.maxAnisotropy = properties.limits.maxSamplerAnisotropy;

    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(Context.GetDevice(), &samplerInfo, nullptr, &newSampler) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create sampler!");
    }

    LOG_INFO("Texture loaded: {} ({}x{})", filePath, texWidth, texHeight);

    // 7. 返回封装好的 FTexture 对象
    return std::make_shared<FTexture>(&Context, newImage, newSampler);
}


// 辅助函数：布局转换 (使用 Pipeline Barrier)
void VulkanRenderer::TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        // 如果不想转移队列所有权，设为 Ignored
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

// 辅助函数：拷贝 Buffer 到 Image
void VulkanRenderer::CopyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
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

        vkCmdCopyBufferToImage(cmd, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
    });
}

void VulkanRenderer::InitDescriptors() {
    VkDevice device = Context.GetDevice();

    // ---------------------------------------------------------
    // 1. 创建 Layout (描述符的形状)
    // 告诉管线：Binding 0 是一个 Combined Image Sampler，只在 Fragment Shader 使用
    // ---------------------------------------------------------
    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 0;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = 1;
    textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    textureBinding.pImmutableSamplers = nullptr;

    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 1;
    layoutInfo.pBindings = &textureBinding;

    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &DescriptorSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor set layout!");
    }

    // ---------------------------------------------------------
    // 2. 创建 Pool (显存池)
    // 我们只需要 1 个 Set，里面包含 1 个 Sampler
    // ---------------------------------------------------------
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1; // 池子总容量

    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &DescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool!");
    }

    // ---------------------------------------------------------
    // 3. 分配 Set (Allocate)
    // ---------------------------------------------------------
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &DescriptorSetLayout;

    if (vkAllocateDescriptorSets(device, &allocInfo, &TextureDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor set!");
    }

    // ---------------------------------------------------------
    // 4. 写入/更新 Set (Update)
    // 把真正的纹理资源 (Stage 7.1 创建的) 填进去
    // ---------------------------------------------------------
    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // Shader 读取时的布局
    imageInfo.imageView = CurrentDemoTexture->GetImageView();
    imageInfo.sampler = CurrentDemoTexture->GetSampler();

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = TextureDescriptorSet;
    descriptorWrite.dstBinding = 0; // 对应 Layout 中的 Binding 0
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

    LOG_INFO("Descriptor Sets Initialized & Updated");
}