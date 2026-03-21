#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Core/Assets/FMaterial.h"
#include "Core/Assets/FMaterialInstance.h"
#include "Core/Geometry/FMesh.h"
#include "Core/Utils/VulkanUtils.h"
#include "Core/Assets/FTexture.h"
#include "Core/Assets/FAssetManager.h"
#include "Core/Graphics/LightData.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <stdexcept>
#include <array>

VulkanRenderer::VulkanRenderer() = default;

VulkanRenderer::~VulkanRenderer() {
    Cleanup();
}

void VulkanRenderer::Init(AppWindow* window) {
    LOG_INFO("================= VulkanRenderer Initialization Started ================");
    Window = window;

    // 1. 初始化 Vulkan 上下文
    Context.Init(window);

    // 2. 初始化子系统
    TheRenderDevice.Init(&Context);
    TheCommandBufferManager.Init(&Context);
    TheResourceUploadManager.Init(&TheRenderDevice, &TheCommandBufferManager);

    // 3. 初始化交换链
    int width, height;
    window->GetFramebufferSize(width, height);
    SwapChain.Init(&Context, width, height);

    // 4. 初始化渲染状态
    InitRenderPass();
    InitFramebuffers();
    InitSyncStructures();
    InitDescriptors();

    // 5. 初始化主命令缓冲区
    MainCommandBuffer = TheCommandBufferManager.AllocatePrimaryCommandBuffer();

    LOG_INFO("================= VulkanRenderer Initialization Completed ================");
}

void VulkanRenderer::Cleanup() {
    VkDevice device = Context.GetDevice();
    if (device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(device);
    }

    // 1. 清理描述符资源
    DestroyBuffer(SceneParameterBuffer);
    if (GlobalSetLayout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(device, std::exchange(GlobalSetLayout, VK_NULL_HANDLE), nullptr);
    }
    if (DescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(device, std::exchange(DescriptorPool, VK_NULL_HANDLE), nullptr);
    }

    // 2. 清理帧缓冲区和渲染通道
    DestroyFramebuffers();
    if (RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, std::exchange(RenderPass, VK_NULL_HANDLE), nullptr);
    }

    // 3. 清理同步对象
    if (RenderFence != VK_NULL_HANDLE) {
        vkDestroyFence(device, std::exchange(RenderFence, VK_NULL_HANDLE), nullptr);
    }
    if (RenderFinishedSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, std::exchange(RenderFinishedSemaphore, VK_NULL_HANDLE), nullptr);
    }
    if (ImageAvailableSemaphore != VK_NULL_HANDLE) {
        vkDestroySemaphore(device, std::exchange(ImageAvailableSemaphore, VK_NULL_HANDLE), nullptr);
    }

    // 4. 清理子系统（按相反顺序）
    TheResourceUploadManager.Cleanup();
    TheCommandBufferManager.Cleanup();
    TheRenderDevice.Cleanup();

    // 5. 清理交换链和上下文
    SwapChain.Cleanup();
    Context.Cleanup();
    Window = nullptr;
    AssetManager = nullptr;
    MainCommandBuffer = VK_NULL_HANDLE;
}

void VulkanRenderer::InitRenderPass() {
    // 1. 颜色附件
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = SwapChain.GetImageFormat();
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

    // 2. 深度附件
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = SwapChain.GetDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 3. Subpass
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // 4. 依赖
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // 5. 创建 RenderPass
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
    const auto& imageViews = SwapChain.GetImageViews();
    VkExtent2D extent = SwapChain.GetExtent();

    Framebuffers.resize(imageViews.size());

    for (size_t i = 0; i < imageViews.size(); i++) {
        std::array<VkImageView, 2> attachments = {
            imageViews[i],
            SwapChain.GetDepthImageView()
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

void VulkanRenderer::DestroyFramebuffers() {
    VkDevice device = Context.GetDevice();
    for (const auto& framebuffer : Framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    Framebuffers.clear();
}

bool VulkanRenderer::RecreateSwapchain() {
    if (Window == nullptr) {
        return false;
    }

    int width = 0;
    int height = 0;
    Window->GetFramebufferSize(width, height);
    if (width == 0 || height == 0) {
        return false;
    }

    VkDevice device = Context.GetDevice();
    vkDeviceWaitIdle(device);

    DestroyFramebuffers();
    if (RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, std::exchange(RenderPass, VK_NULL_HANDLE), nullptr);
    }

    SwapChain.Cleanup();
    SwapChain.Init(&Context, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    InitRenderPass();
    InitFramebuffers();

    // 重置命令缓冲区以适应新的交换链
    VK_CHECK(vkResetCommandBuffer(MainCommandBuffer, 0));

    return true;
}

void VulkanRenderer::InitSyncStructures() {
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VkDevice device = Context.GetDevice();

    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &ImageAvailableSemaphore));
    VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &RenderFinishedSemaphore));
    VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &RenderFence));
}

void VulkanRenderer::InitDescriptors() {
    VkDevice device = Context.GetDevice();

    // 创建描述符池
    std::array<VkDescriptorPoolSize, 2> poolSizes{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[0].descriptorCount = 1000;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[1].descriptorCount = 1000;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes = poolSizes.data();
    poolInfo.maxSets = 1000;

    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, nullptr, &DescriptorPool));
    LOG_INFO("Descriptor Pool Initialized (Capacity: 1000 Sets)");

    // 创建全局描述符集布局
    VkDescriptorSetLayoutBinding sceneBind{};
    sceneBind.binding = 0;
    sceneBind.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    sceneBind.descriptorCount = 1;
    sceneBind.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo globalLayoutInfo{};
    globalLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    globalLayoutInfo.bindingCount = 1;
    globalLayoutInfo.pBindings = &sceneBind;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &globalLayoutInfo, nullptr, &GlobalSetLayout));

    // 分配全局描述符集
    GlobalDescriptorSet = AllocateDescriptorSet(GlobalSetLayout);

    // 创建场景参数缓冲区
    TheRenderDevice.CreateBuffer(
        sizeof(SceneDataUBO),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        SceneParameterBuffer
    );

    // 更新描述符集
    VkDescriptorBufferInfo sceneBufferInfo{};
    sceneBufferInfo.buffer = SceneParameterBuffer.Buffer;
    sceneBufferInfo.offset = 0;
    sceneBufferInfo.range = sizeof(SceneDataUBO);

    VkWriteDescriptorSet setWrite{};
    setWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    setWrite.dstSet = GlobalDescriptorSet;
    setWrite.dstBinding = 0;
    setWrite.descriptorCount = 1;
    setWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    setWrite.pBufferInfo = &sceneBufferInfo;

    vkUpdateDescriptorSets(device, 1, &setWrite, 0, nullptr);
}

void VulkanRenderer::Render(
    const SceneViewData& viewData,
    const std::vector<RenderPacket>& renderPackets,
    std::function<void(VkCommandBuffer)> onUIRender) {

    VkDevice device = Context.GetDevice();
    vkWaitForFences(device, 1, &RenderFence, VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = SwapChain.AcquireNextImage(ImageAvailableSemaphore, imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        RecreateSwapchain();
        return;
    }
    VK_CHECK(result);

    vkResetFences(device, 1, &RenderFence);

    // 重用主命令缓冲区
    VK_CHECK(vkResetCommandBuffer(MainCommandBuffer, 0));

    RecordCommandBuffer(MainCommandBuffer, imageIndex, viewData, renderPackets, onUIRender);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

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

    result = SwapChain.PresentImage(RenderFinishedSemaphore, imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        RecreateSwapchain();
    } else {
        VK_CHECK(result);
    }
}

void VulkanRenderer::RecordCommandBuffer(
    VkCommandBuffer cmdBuffer,
    uint32_t imageIndex,
    const SceneViewData& viewData,
    const std::vector<RenderPacket>& renderPackets,
    std::function<void(VkCommandBuffer)> onUIRender) {

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = RenderPass;
    renderPassInfo.framebuffer = Framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = SwapChain.GetExtent();

    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}};
    clearValues[1].depthStencil = {1.0f, 0};
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(renderPassInfo.renderArea.extent.width);
    viewport.height = static_cast<float>(renderPassInfo.renderArea.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = renderPassInfo.renderArea.extent;
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    // 更新场景数据
    SceneDataUBO sceneData;
    sceneData.ViewProjection = viewData.ProjectionMatrix * viewData.ViewMatrix;
    sceneData.CameraPosition = glm::vec4(viewData.CameraPosition, 1.0f);
    
    // 支持多光源，目前取第一个光源
    if (!viewData.Lights.empty()) {
        sceneData.LightPosition = glm::vec4(viewData.Lights[0].Position, 1.0f);
        sceneData.LightColor = glm::vec4(viewData.Lights[0].Color, viewData.Lights[0].Intensity);
    } else {
        sceneData.LightPosition = glm::vec4(0.0f);
        sceneData.LightColor = glm::vec4(0.0f);
    }
    
    memcpy(SceneParameterBuffer.Info.pMappedData, &sceneData, sizeof(SceneDataUBO));

    // 渲染所有对象
    for (const auto& packet : renderPackets) {
        auto parentMaterial = packet.Material->GetParent();

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, parentMaterial->Pipeline);

        VkDescriptorSet descSet[] = {GlobalDescriptorSet, packet.Material->GetDescriptorSet()};
        vkCmdBindDescriptorSets(
            cmdBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS,
            parentMaterial->PipelineLayout,
            0, 2, descSet,
            0, nullptr
        );

        vkCmdPushConstants(
            cmdBuffer,
            parentMaterial->PipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(glm::mat4),
            &packet.TransformMatrix
        );

        FVulkanMesh& gpuMesh = UploadMesh(packet.Mesh);
        VkBuffer vertexBuffers[] = {gpuMesh.VertexBuffer.Buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, gpuMesh.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmdBuffer, gpuMesh.IndexCount, 1, 0, 0, 0);
    }

    if (onUIRender) {
        onUIRender(cmdBuffer);
    }

    vkCmdEndRenderPass(cmdBuffer);
    vkEndCommandBuffer(cmdBuffer);
}

// ==========================================
// 委托方法 - 转发给子系统
// ==========================================

void VulkanRenderer::CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage, AllocatedBuffer& outBuffer) {
    TheRenderDevice.CreateBuffer(size, usage, memoryUsage, outBuffer);
}

void VulkanRenderer::DestroyBuffer(AllocatedBuffer& buffer) {
    TheRenderDevice.DestroyBuffer(buffer);
}

bool VulkanRenderer::LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule) {
    return TheResourceUploadManager.LoadShaderModule(filePath, outShaderModule);
}

FVulkanMesh& VulkanRenderer::UploadMesh(FMesh* cpuMesh) {
    return TheResourceUploadManager.UploadMesh(cpuMesh);
}

std::shared_ptr<FTexture> VulkanRenderer::LoadTexture(const std::string& filePath) {
    return TheResourceUploadManager.LoadTexture(filePath);
}

VkDescriptorSet VulkanRenderer::AllocateDescriptorSet(VkDescriptorSetLayout layout) {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = DescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet descriptorSet;
    VK_CHECK(vkAllocateDescriptorSets(Context.GetDevice(), &allocInfo, &descriptorSet));
    return descriptorSet;
}
