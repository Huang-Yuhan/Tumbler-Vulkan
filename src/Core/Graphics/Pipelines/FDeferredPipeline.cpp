#include "FDeferredPipeline.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Graphics/RenderDevice.h"
#include "Core/Assets/FMaterial.h"
#include "Core/Assets/FMaterialInstance.h"
#include "Core/Utils/Log.h"
#include "Core/Graphics/VulkanPipelineBuilder.h"
#include <array>
#include <fstream>
#include <stdexcept>
#include <glm/glm.hpp>

void FDeferredPipeline::Init(VulkanRenderer* renderer)
{
    InitRenderPass(renderer);
    InitGBuffers(renderer);
    InitFramebuffers(renderer);
    InitLightingPipeline(renderer);
    UpdateLightingDescriptorSet(renderer);
}

void FDeferredPipeline::Cleanup(VulkanRenderer* renderer)
{
    VkDevice device = renderer->GetDevice();

    if (LightingPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, LightingPipeline, nullptr);
    if (LightingPipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, LightingPipelineLayout, nullptr);
    if (LightingSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, LightingSetLayout, nullptr);
    if (LightingDescriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, LightingDescriptorPool, nullptr);

    for (const auto& framebuffer : Framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    Framebuffers.clear();

    DestroyGBuffers(renderer);

    if (RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, RenderPass, nullptr);
        RenderPass = VK_NULL_HANDLE;
    }
}

void FDeferredPipeline::RecreateResources(VulkanRenderer* renderer)
{
    VkDevice device = renderer->GetDevice();
    for (const auto& framebuffer : Framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    Framebuffers.clear();

    DestroyGBuffers(renderer);

    InitGBuffers(renderer);
    InitFramebuffers(renderer);
}

void FDeferredPipeline::InitRenderPass(VulkanRenderer* renderer)
{
    // === 附件定义 (Attachments) ===
    
    // 0. Final Color (输出到 Swapchain)
    VkAttachmentDescription finalColorAttachment{};
    finalColorAttachment.format = renderer->GetSwapchainImageFormat();
    finalColorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    finalColorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    finalColorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    finalColorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    finalColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    finalColorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    finalColorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // 1. Albedo G-Buffer
    VkAttachmentDescription albedoAttachment{};
    albedoAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    albedoAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    albedoAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    albedoAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 之后不再需要存回物理显存，节约带宽
    albedoAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    albedoAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    albedoAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    albedoAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // 因为要作为 Input 读取

    // 2. Normal G-Buffer
    VkAttachmentDescription normalAttachment{};
    normalAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    normalAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    normalAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    normalAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    normalAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    normalAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    normalAttachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 3. Depth Attachment (复用 Swapchain Depth, 我们需要其重投影获取世界坐标)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = renderer->GetSwapchainDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;


    // === 子通道引用 (Subpass References) ===

    // Subpass 0: 几何通道 (输出到 Albedo, Normal, Depth)
    VkAttachmentReference depthRef = {3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    std::array<VkAttachmentReference, 2> colorRefs = {
        VkAttachmentReference{1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        VkAttachmentReference{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}
    };
    
    VkSubpassDescription subpass0{};
    subpass0.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass0.colorAttachmentCount = static_cast<uint32_t>(colorRefs.size());
    subpass0.pColorAttachments = colorRefs.data();
    subpass0.pDepthStencilAttachment = &depthRef;

    // Subpass 1: 光照通道 (读取 Albedo, Normal, Depth, 然后算出结果并输出到 Final Color)
    VkAttachmentReference finalColorRef = {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    std::array<VkAttachmentReference, 3> inputRefs = {
        VkAttachmentReference{1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        VkAttachmentReference{2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        VkAttachmentReference{3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}
    };

    VkSubpassDescription subpass1{};
    subpass1.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass1.colorAttachmentCount = 1;
    subpass1.pColorAttachments = &finalColorRef;
    subpass1.inputAttachmentCount = static_cast<uint32_t>(inputRefs.size());
    subpass1.pInputAttachments = inputRefs.data();
    // 深度不在需要写入了，所以留空


    // === 子通道依赖 (Dependencies) ===
    
    std::array<VkSubpassDependency, 3> dependencies{};

    // 外部 -> Subpass 0
    dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[0].dstSubpass = 0;
    dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Subpass 0 -> Subpass 1
    // 需要确保 G-Buffer 画完了才能在 Subpass 1 里面当做 Input Attachment 读
    dependencies[1].srcSubpass = 0;
    dependencies[1].dstSubpass = 1;
    dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

    // Subpass 1 -> 外部
    dependencies[2].srcSubpass = 1;
    dependencies[2].dstSubpass = VK_SUBPASS_EXTERNAL;
    dependencies[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependencies[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependencies[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dependencies[2].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
    dependencies[2].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


    // 创建 RenderPass
    std::array<VkAttachmentDescription, 4> attachments = {finalColorAttachment, albedoAttachment, normalAttachment, depthAttachment};
    std::array<VkSubpassDescription, 2> subpasses = {subpass0, subpass1};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = static_cast<uint32_t>(subpasses.size());
    renderPassInfo.pSubpasses = subpasses.data();
    renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
    renderPassInfo.pDependencies = dependencies.data();

    if (vkCreateRenderPass(renderer->GetDevice(), &renderPassInfo, nullptr, &RenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Deferred Render Pass!");
    }
    LOG_INFO("Deferred Render Pass created successfully with 2 subpasses.");
}

void FDeferredPipeline::InitGBuffers(VulkanRenderer* renderer)
{
    VkExtent2D extent = renderer->GetSwapchainExtent();
    auto renderDev = renderer->GetRenderDevice();
    VkDevice device = renderer->GetDevice();

    // 1. Albedo G-Buffer Allocation (VK_FORMAT_R8G8B8A8_UNORM)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        // IMPORTANT: It needs COLOR_ATTACHMENT to be drawn into, and INPUT_ATTACHMENT to be read later!
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // Use VMA via RenderDevice to allocate
        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (vmaCreateImage(renderer->GetContext().GetAllocator(), &imageInfo, &allocInfo,
            &AlbedoImage.Image, &AlbedoImage.Allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate Albedo G-Buffer image.");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = AlbedoImage.Image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &AlbedoImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Albedo G-Buffer image view.");
        }
    }

    // 2. Normal G-Buffer Allocation (VK_FORMAT_R16G16B16A16_SFLOAT)
    {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = extent.width;
        imageInfo.extent.height = extent.height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VmaAllocationCreateInfo allocInfo{};
        allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        allocInfo.requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

        if (vmaCreateImage(renderer->GetContext().GetAllocator(), &imageInfo, &allocInfo,
            &NormalImage.Image, &NormalImage.Allocation, nullptr) != VK_SUCCESS) {
            throw std::runtime_error("Failed to allocate Normal G-Buffer image.");
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = NormalImage.Image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R16G16B16A16_SFLOAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(device, &viewInfo, nullptr, &NormalImageView) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Normal G-Buffer image view.");
        }
    }
}

void FDeferredPipeline::DestroyGBuffers(VulkanRenderer* renderer)
{
    VkDevice device = renderer->GetDevice();
    auto allocator = renderer->GetContext().GetAllocator();

    if (AlbedoImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, std::exchange(AlbedoImageView, VK_NULL_HANDLE), nullptr);
        vmaDestroyImage(allocator, std::exchange(AlbedoImage.Image, VK_NULL_HANDLE), AlbedoImage.Allocation);
    }

    if (NormalImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(device, std::exchange(NormalImageView, VK_NULL_HANDLE), nullptr);
        vmaDestroyImage(allocator, std::exchange(NormalImage.Image, VK_NULL_HANDLE), NormalImage.Allocation);
    }
}

void FDeferredPipeline::InitFramebuffers(VulkanRenderer* renderer)
{
    const auto& imageViews = renderer->GetSwapchainImageViews();
    VkExtent2D extent = renderer->GetSwapchainExtent();

    Framebuffers.resize(imageViews.size());

    for (size_t i = 0; i < imageViews.size(); i++) {
        // [0] Swapchain, [1] Albedo, [2] Normal, [3] Depth
        std::array<VkImageView, 4> attachments = {
            imageViews[i],
            AlbedoImageView,
            NormalImageView,
            renderer->GetSwapchainDepthImageView()
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = RenderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(renderer->GetDevice(), &framebufferInfo, nullptr, &Framebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create Deferred Framebuffer!");
        }
    }
}

void FDeferredPipeline::InitLightingPipeline(VulkanRenderer* renderer) {
    VkDevice device = renderer->GetDevice();

    // 1. 创建 Descriptor Pool
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 3}
    };
    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = poolSizes;
    poolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &LightingDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create descriptor pool for Lighting Pass");
    }

    // 2. 创建 Descriptor Set Layout (3 个 subpassInput)
    VkDescriptorSetLayoutBinding albedoBinding{};
    albedoBinding.binding = 0;
    albedoBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    albedoBinding.descriptorCount = 1;
    albedoBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding normalBinding{};
    normalBinding.binding = 1;
    normalBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    normalBinding.descriptorCount = 1;
    normalBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding depthBinding{};
    depthBinding.binding = 2;
    depthBinding.descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    depthBinding.descriptorCount = 1;
    depthBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutBinding bindings[] = {albedoBinding, normalBinding, depthBinding};
    VkDescriptorSetLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = 3;
    layoutInfo.pBindings = bindings;
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &LightingSetLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Lighting Descriptor Set Layout");
    }

    // 分配 Descriptor Set
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = LightingDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &LightingSetLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &LightingDescriptorSet) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate Lighting Descriptor Set");
    }

    // 3. Pipeline Layout (Global UBO as set=0, Subpass Input as set=1)
    VkDescriptorSetLayout setLayouts[] = {renderer->GetGlobalSetLayout(), LightingSetLayout};
    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 2;
    pipelineLayoutInfo.pSetLayouts = setLayouts;
    if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &LightingPipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Lighting Pipeline Layout");
    }

    // 4. Load Shaders
    auto loadShader = [&](const std::string& path) -> VkShaderModule {
        std::ifstream file(path, std::ios::ate | std::ios::binary);
        if (!file.is_open()) return VK_NULL_HANDLE;
        size_t size = static_cast<size_t>(file.tellg());
        std::vector<char> buf(size);
        file.seekg(0);
        file.read(buf.data(), size);
        file.close();
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = buf.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(buf.data());
        VkShaderModule mod;
        vkCreateShaderModule(device, &createInfo, nullptr, &mod);
        return mod;
    };

    VkShaderModule vertShader = loadShader("assets/shaders/engine/deferred_lighting.vert.spv");
    VkShaderModule fragShader = loadShader("assets/shaders/engine/deferred_lighting.frag.spv");

    // 5. Build Pipeline
    auto builder = VulkanPipelineBuilder::Begin(LightingPipelineLayout);
    // 无 Vertex Buffer! (所以 VertexInputInfo 全部留空，0 count)
    builder.VertexInputInfo.vertexBindingDescriptionCount = 0;
    builder.VertexInputInfo.vertexAttributeDescriptionCount = 0;

    LightingPipeline = builder
        .SetShaders(vertShader, fragShader)
        .SetViewport(renderer->GetSwapchainExtent().width, renderer->GetSwapchainExtent().height)
        .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE) // 后期全屏四边形禁用剔除以免画不出来
        .SetMultisamplingNone()
        .DisableDepthTest() // 极度关键：光照通道不需要且不能深度测试测试！我们要让三角形完全覆盖屏幕
        .SetColorBlending(false, 1) // 混合到 Swapchain
        .Build(device, RenderPass, 1);

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}

void FDeferredPipeline::UpdateLightingDescriptorSet(VulkanRenderer* renderer) {
    auto device = renderer->GetDevice();

    VkDescriptorImageInfo albedoInfo{};
    albedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    albedoInfo.imageView = AlbedoImageView; // 我们动态分配创建的
    albedoInfo.sampler = VK_NULL_HANDLE;

    VkDescriptorImageInfo normalInfo{};
    normalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    normalInfo.imageView = NormalImageView;
    normalInfo.sampler = VK_NULL_HANDLE;

    VkDescriptorImageInfo depthInfo{};
    depthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // 虽然是 Depth，但作为 input attachment 就是 READ_ONLY
    depthInfo.imageView = renderer->GetSwapchainDepthImageView();
    depthInfo.sampler = VK_NULL_HANDLE;

    VkWriteDescriptorSet writes[3] = {};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = LightingDescriptorSet;
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    writes[0].descriptorCount = 1;
    writes[0].pImageInfo = &albedoInfo;

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = LightingDescriptorSet;
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &normalInfo;

    writes[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[2].dstSet = LightingDescriptorSet;
    writes[2].dstBinding = 2;
    writes[2].dstArrayElement = 0;
    writes[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
    writes[2].descriptorCount = 1;
    writes[2].pImageInfo = &depthInfo;

    vkUpdateDescriptorSets(device, 3, writes, 0, nullptr);
}

void FDeferredPipeline::RecordCommands(
    VkCommandBuffer cmdBuffer,
    uint32_t imageIndex,
    VulkanRenderer* renderer,
    const SceneViewData& viewData,
    const std::vector<RenderPacket>& renderPackets,
    std::function<void(VkCommandBuffer)> onUIRender)
{
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    vkBeginCommandBuffer(cmdBuffer, &beginInfo);

    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = RenderPass;
    renderPassInfo.framebuffer = Framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = renderer->GetSwapchainExtent();

    std::array<VkClearValue, 4> clearValues{};
    clearValues[0].color = {{0.1f, 0.1f, 0.1f, 1.0f}}; // Final Output
    clearValues[1].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Albedo Output
    clearValues[2].color = {{0.0f, 0.0f, 0.0f, 0.0f}}; // Normal Output
    clearValues[3].depthStencil = {1.0f, 0};           // Depth (Replaces the regular depth buffer clear)
    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    // =============== SUBPASS 0: GEOMETRY ===============
    vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.width = static_cast<float>(renderPassInfo.renderArea.extent.width);
    viewport.height = static_cast<float>(renderPassInfo.renderArea.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = renderPassInfo.renderArea.extent;
    vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);

    // 绘制全部的拥有 deferred_geometry 材质模型的对象
    for (const auto& packet : renderPackets) {
        auto parentMaterial = packet.Material->GetParent();

        vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, parentMaterial->GetPipeline(ERenderPath::Deferred));

        VkDescriptorSet descSet[] = {renderer->GetGlobalDescriptorSet(), packet.Material->GetDescriptorSet()};
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

        FVulkanMesh& gpuMesh = renderer->UploadMesh(packet.Mesh);
        VkBuffer vertexBuffers[] = {gpuMesh.VertexBuffer.Buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmdBuffer, gpuMesh.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmdBuffer, gpuMesh.IndexCount, 1, 0, 0, 0);
    }


    // =============== SUBPASS 1: LIGHTING ===============
    vkCmdNextSubpass(cmdBuffer, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingPipeline);

    // 绑定 Global Scene Data (set = 0) 以及 LightingDescriptorSet (set = 1)
    VkDescriptorSet descSets[] = {renderer->GetGlobalDescriptorSet(), LightingDescriptorSet};
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, LightingPipelineLayout, 0, 2, descSets, 0, nullptr);

    // 发射一个全屏大三角形，不需要绑定 VertexBuffer
    vkCmdDraw(cmdBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(cmdBuffer);
    // Note: vkEndCommandBuffer is called by VulkanRenderer after the UI pass.
}
