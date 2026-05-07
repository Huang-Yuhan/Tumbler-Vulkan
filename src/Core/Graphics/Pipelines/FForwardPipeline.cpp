#include "FForwardPipeline.h"
#include "Core/Graphics/VulkanRenderer.h"
#include <array>
#include <stdexcept>
#include <glm/glm.hpp>
#include "Core/Utils/Log.h"

void FForwardPipeline::Init(VulkanRenderer* renderer)
{
    InitRenderPass(renderer);
    InitFramebuffers(renderer);
}

void FForwardPipeline::Cleanup(VulkanRenderer* renderer)
{
    VkDevice device = renderer->GetDevice();
    for (const auto& framebuffer : Framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    Framebuffers.clear();

    if (RenderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(device, RenderPass, nullptr);
        RenderPass = VK_NULL_HANDLE;
    }
}

void FForwardPipeline::RecreateResources(VulkanRenderer* renderer)
{
    // Recreate framebuffers when swapchain resizes
    VkDevice device = renderer->GetDevice();
    for (const auto& framebuffer : Framebuffers) {
        vkDestroyFramebuffer(device, framebuffer, nullptr);
    }
    Framebuffers.clear();
    InitFramebuffers(renderer);
}

void FForwardPipeline::InitRenderPass(VulkanRenderer* renderer)
{
    // 1. Color Attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = renderer->GetSwapchainImageFormat();
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

    // 2. Depth Attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = renderer->GetSwapchainDepthFormat();
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

    // 4. Dependencies
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    // 5. Create
    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(renderer->GetDevice(), &renderPassInfo, nullptr, &RenderPass) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create Forward Render Pass");
    }
}

void FForwardPipeline::InitFramebuffers(VulkanRenderer* renderer)
{
    const std::vector<VkImageView> sharedAttachments = { renderer->GetSwapchainDepthImageView() };
    CreateFramebuffers(renderer->GetDevice(), RenderPass,
        renderer->GetSwapchainExtent(), renderer->GetSwapchainImageViews(),
        sharedAttachments, Framebuffers);
}

void FForwardPipeline::RecordCommands(
    VkCommandBuffer cmdBuffer,
    uint32_t imageIndex,
    VulkanRenderer* renderer,
    const SceneViewData& viewData,
    const std::vector<RenderPacket>& renderPackets)
{
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = RenderPass;
    renderPassInfo.framebuffer = Framebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = renderer->GetSwapchainExtent();

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

    // 渲染所有对象
    DrawMeshPackets(cmdBuffer, renderer, ERenderPath::Forward, renderPackets);

    vkCmdEndRenderPass(cmdBuffer);
    // Note: vkEndCommandBuffer is called by VulkanRenderer after the UI pass.
}
