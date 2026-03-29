#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"
#include "Core/Assets/FMaterial.h"
#include "Core/Assets/FMaterialInstance.h"
#include "Core/Geometry/FMesh.h"
#include "Core/Utils/VulkanUtils.h"
#include "Core/Assets/FTexture.h"
#include "Core/Assets/FAssetManager.h"
#include "Core/Assets/FAssetManager.h"
#include "Core/Graphics/LightData.h"
#include "Core/Graphics/Pipelines/FForwardPipeline.h"
#include "Core/Graphics/Pipelines/FDeferredPipeline.h"

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

    // 4. 初始化描述符与同步机制
    InitDescriptors();
    InitSyncStructures();

    // 5. 初始化渲染管线策略 (必须在 DescriptorLayout 创建后)
    InitPipelines();

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

    // 2. 清理渲染管线策略 (Pipelines & Framebuffers)
    for (auto& [path, pipeline] : Pipelines) {
        if (pipeline) pipeline->Cleanup(this);
    }
    Pipelines.clear();

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

void VulkanRenderer::InitPipelines() {
    auto forwardPipeline = std::make_unique<FForwardPipeline>();
    forwardPipeline->Init(this);
    Pipelines[ERenderPath::Forward] = std::move(forwardPipeline);
    
    auto deferredPipeline = std::make_unique<FDeferredPipeline>();
    deferredPipeline->Init(this);
    Pipelines[ERenderPath::Deferred] = std::move(deferredPipeline);
}

VkRenderPass VulkanRenderer::GetRenderPass(ERenderPath path) const {
    auto it = Pipelines.find(path);
    if (it != Pipelines.end() && it->second) {
        return it->second->GetRenderPass();
    }
    return VK_NULL_HANDLE;
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

    SwapChain.Cleanup();
    SwapChain.Init(&Context, static_cast<uint32_t>(width), static_cast<uint32_t>(height));
    
    for (auto& [path, pipeline] : Pipelines) {
        pipeline->RecreateResources(this);
    }

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
    std::function<void(VkCommandBuffer, uint32_t)> onUIRender) {

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
    std::function<void(VkCommandBuffer, uint32_t)> onUIRender) {

    // 首先更新全局场景参数 UBO (由于它是共用的，放外部更新比放管道更合理)
    SceneDataUBO sceneData{};
    glm::mat4 viewProj = viewData.ProjectionMatrix * viewData.ViewMatrix;
    sceneData.ViewProjection = viewProj;
    sceneData.InvViewProj = glm::inverse(viewProj);
    sceneData.CameraPosition = glm::vec4(viewData.CameraPosition, 1.0f);
    
    int count = static_cast<int>(viewData.Lights.size());
    sceneData.LightCount = count < MAX_SCENE_LIGHTS ? count : MAX_SCENE_LIGHTS;
    
    for (int i = 0; i < sceneData.LightCount; ++i) {
        sceneData.Lights[i].Position = glm::vec4(viewData.Lights[i].Position, 1.0f);
        sceneData.Lights[i].Color = glm::vec4(viewData.Lights[i].Color, viewData.Lights[i].Intensity);
    }
    for (int i = sceneData.LightCount; i < MAX_SCENE_LIGHTS; ++i) {
        sceneData.Lights[i].Position = glm::vec4(0.0f);
        sceneData.Lights[i].Color = glm::vec4(0.0f);
    }
    
    memcpy(SceneParameterBuffer.Info.pMappedData, &sceneData, sizeof(SceneDataUBO));

    // 执行当前选中管线的具体命令录制
    auto it = Pipelines.find(viewData.RenderPath);
    if (it != Pipelines.end() && it->second) {
        it->second->RecordCommands(cmdBuffer, imageIndex, this, viewData, renderPackets, nullptr);
    } else {
        LOG_CRITICAL("Selected RenderPath mapped to no active pipeline!");
    }

    if (onUIRender) {
        onUIRender(cmdBuffer, imageIndex);
    }

    // 所有人完了，统一在这里关闭 CB
    VK_CHECK(vkEndCommandBuffer(cmdBuffer));
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
