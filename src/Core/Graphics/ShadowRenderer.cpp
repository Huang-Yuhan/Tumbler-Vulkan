#include "ShadowRenderer.h"
#include "VulkanRenderer.h"
#include "RenderDevice.h"
#include "SceneViewData.h"
#include "RenderPacket.h"
#include "FVulkanMesh.h"
#include "VulkanPipelineBuilder.h"
#include "Core/Utils/VulkanUtils.h"
#include <array>
#include <stdexcept>

void ShadowRenderer::Init(VulkanRenderer* renderer)
{
    VkDevice device = renderer->GetDevice();
    RenderDevice* renderDev = renderer->GetRenderDevice();
    VkExtent2D extent = {kShadowMapSize, kShadowMapSize};

    // 1. Shadow map depth image
    renderDev->CreateImage(
        kShadowMapSize, kShadowMapSize,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_TILING_OPTIMAL,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        ShadowMap,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    ShadowMapView = renderDev->CreateImageView(ShadowMap.Image,
        VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);

    // 2. Shadow sampler (comparison mode for PCF)
    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    samplerInfo.compareEnable = VK_TRUE;
    samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    samplerInfo.maxLod = 1.0f;
    if (vkCreateSampler(device, &samplerInfo, nullptr, &ShadowSampler) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow sampler");

    // 3. Shadow render pass (depth-only)
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = VK_FORMAT_D32_SFLOAT;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;

    VkAttachmentReference depthRef{};
    depthRef.attachment = 0;
    depthRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 0;
    subpass.pDepthStencilAttachment = &depthRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = 1;
    rpInfo.pAttachments = &depthAttachment;
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;
    rpInfo.dependencyCount = 1;
    rpInfo.pDependencies = &dependency;
    if (vkCreateRenderPass(device, &rpInfo, nullptr, &ShadowRenderPass) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow render pass");

    // 4. Shadow framebuffer
    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = ShadowRenderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &ShadowMapView;
    fbInfo.width = kShadowMapSize;
    fbInfo.height = kShadowMapSize;
    fbInfo.layers = 1;
    if (vkCreateFramebuffer(device, &fbInfo, nullptr, &ShadowFramebuffer) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow framebuffer");

    // 5. Shadow uniform buffer (LightViewProj)
    renderDev->CreateBuffer(sizeof(glm::mat4),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        ShadowUniformBuffer);

    // 6. Descriptor set layout (UBO for LightViewProj)
    VkDescriptorSetLayoutBinding uboBinding{};
    uboBinding.binding = 0;
    uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    uboBinding.descriptorCount = 1;
    uboBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo setLayoutInfo{};
    setLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    setLayoutInfo.bindingCount = 1;
    setLayoutInfo.pBindings = &uboBinding;
    if (vkCreateDescriptorSetLayout(device, &setLayoutInfo, nullptr, &ShadowSetLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow descriptor set layout");

    // 7. Descriptor pool + allocate set
    VkDescriptorPoolSize poolSize{};
    poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSize.descriptorCount = 1;

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = 1;
    poolInfo.pPoolSizes = &poolSize;
    poolInfo.maxSets = 1;
    if (vkCreateDescriptorPool(device, &poolInfo, nullptr, &ShadowDescriptorPool) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow descriptor pool");

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = ShadowDescriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &ShadowSetLayout;
    if (vkAllocateDescriptorSets(device, &allocInfo, &ShadowDescriptorSet) != VK_SUCCESS)
        throw std::runtime_error("Failed to allocate shadow descriptor set");

    VkDescriptorBufferInfo bufferInfo{};
    bufferInfo.buffer = ShadowUniformBuffer.Buffer;
    bufferInfo.offset = 0;
    bufferInfo.range = sizeof(glm::mat4);

    VkWriteDescriptorSet write{};
    write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    write.dstSet = ShadowDescriptorSet;
    write.dstBinding = 0;
    write.descriptorCount = 1;
    write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);

    // 8. Pipeline layout
    VkPipelineLayoutCreateInfo plInfo{};
    plInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plInfo.setLayoutCount = 1;
    plInfo.pSetLayouts = &ShadowSetLayout;

    // Push constant for model matrix
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    pushRange.offset = 0;
    pushRange.size = sizeof(glm::mat4);
    plInfo.pushConstantRangeCount = 1;
    plInfo.pPushConstantRanges = &pushRange;

    if (vkCreatePipelineLayout(device, &plInfo, nullptr, &ShadowPipelineLayout) != VK_SUCCESS)
        throw std::runtime_error("Failed to create shadow pipeline layout");

    // 9. Shadow pipeline
    VkShaderModule vertShader, fragShader;
    renderer->LoadShaderModule("assets/shaders/engine/shadow_depth.vert.spv", &vertShader);
    renderer->LoadShaderModule("assets/shaders/engine/shadow_depth.frag.spv", &fragShader);

    auto builder = VulkanPipelineBuilder::Begin(ShadowPipelineLayout);

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = 11 * sizeof(float);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attrDescs = {
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
    };

    builder.VertexInputInfo.vertexBindingDescriptionCount = 1;
    builder.VertexInputInfo.pVertexBindingDescriptions = &bindingDesc;
    builder.VertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attrDescs.size());
    builder.VertexInputInfo.pVertexAttributeDescriptions = attrDescs.data();

    ShadowPipeline = builder
        .SetShaders(vertShader, fragShader)
        .SetViewport(kShadowMapSize, kShadowMapSize)
        .SetDynamicViewportScissor()
        .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
        .SetPolygonMode(VK_POLYGON_MODE_FILL)
        .SetCullMode(VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_CLOCKWISE)
        .SetMultisamplingNone()
        .EnableDepthTest(VK_TRUE, VK_COMPARE_OP_LESS)
        .SetColorBlending(false, 0)
        .Build(device, ShadowRenderPass, 0);

    vkDestroyShaderModule(device, vertShader, nullptr);
    vkDestroyShaderModule(device, fragShader, nullptr);
}

void ShadowRenderer::Cleanup(VulkanRenderer* renderer)
{
    VkDevice device = renderer->GetDevice();
    RenderDevice* renderDev = renderer->GetRenderDevice();

    if (ShadowPipeline != VK_NULL_HANDLE) vkDestroyPipeline(device, ShadowPipeline, nullptr);
    if (ShadowPipelineLayout != VK_NULL_HANDLE) vkDestroyPipelineLayout(device, ShadowPipelineLayout, nullptr);
    if (ShadowSetLayout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(device, ShadowSetLayout, nullptr);
    if (ShadowDescriptorPool != VK_NULL_HANDLE) vkDestroyDescriptorPool(device, ShadowDescriptorPool, nullptr);

    if (ShadowFramebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(device, ShadowFramebuffer, nullptr);
    if (ShadowRenderPass != VK_NULL_HANDLE) vkDestroyRenderPass(device, ShadowRenderPass, nullptr);

    if (ShadowUniformBuffer.Buffer != VK_NULL_HANDLE) renderDev->DestroyBuffer(ShadowUniformBuffer);

    if (ShadowSampler != VK_NULL_HANDLE) vkDestroySampler(device, ShadowSampler, nullptr);

    if (ShadowMapView != VK_NULL_HANDLE) renderDev->DestroyImageView(std::exchange(ShadowMapView, VK_NULL_HANDLE));
    renderDev->DestroyImage(ShadowMap);

    ShadowPipeline = VK_NULL_HANDLE;
    ShadowPipelineLayout = VK_NULL_HANDLE;
    ShadowSetLayout = VK_NULL_HANDLE;
    ShadowDescriptorPool = VK_NULL_HANDLE;
    ShadowFramebuffer = VK_NULL_HANDLE;
    ShadowRenderPass = VK_NULL_HANDLE;
    ShadowSampler = VK_NULL_HANDLE;
}

void ShadowRenderer::RecordDepthPass(VkCommandBuffer cmd, VulkanRenderer* renderer,
    const SceneViewData& viewData, const std::vector<RenderPacket>& renderPackets)
{
    // Upload LightViewProj to shadow UBO
    if (ShadowUniformBuffer.Info.pMappedData) {
        memcpy(ShadowUniformBuffer.Info.pMappedData, &viewData.LightViewProj, sizeof(glm::mat4));
    }

    // Begin shadow render pass
    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = ShadowRenderPass;
    rpBegin.framebuffer = ShadowFramebuffer;
    rpBegin.renderArea.extent = {kShadowMapSize, kShadowMapSize};

    VkClearValue clearVal{};
    clearVal.depthStencil = {1.0f, 0};
    rpBegin.clearValueCount = 1;
    rpBegin.pClearValues = &clearVal;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ShadowPipeline);

    VkViewport viewport{};
    viewport.width = static_cast<float>(kShadowMapSize);
    viewport.height = static_cast<float>(kShadowMapSize);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.extent = {kShadowMapSize, kShadowMapSize};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
        ShadowPipelineLayout, 0, 1, &ShadowDescriptorSet, 0, nullptr);

    for (const auto& packet : renderPackets) {
        FVulkanMesh& gpuMesh = renderer->UploadMesh(packet.Mesh);

        vkCmdPushConstants(cmd, ShadowPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT,
            0, sizeof(glm::mat4), &packet.TransformMatrix);

        VkBuffer vertexBuffers[] = {gpuMesh.VertexBuffer.Buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, gpuMesh.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd, gpuMesh.IndexCount, 1, 0, 0, 0);
    }

    vkCmdEndRenderPass(cmd);
}
