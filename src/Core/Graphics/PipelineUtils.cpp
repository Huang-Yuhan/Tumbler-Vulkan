#include "Core/Graphics/IRenderPipeline.h"
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/Assets/FMaterial.h"
#include "Core/Assets/FMaterialInstance.h"
#include "Core/Graphics/FVulkanMesh.h"
#include <stdexcept>

void IRenderPipeline::DrawMeshPackets(VkCommandBuffer cmd, VulkanRenderer* renderer,
    ERenderPath renderPath, const std::vector<RenderPacket>& renderPackets)
{
    for (const auto& packet : renderPackets) {
        auto parentMaterial = packet.Material->GetParent();

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            parentMaterial->GetPipeline(renderPath));

        VkDescriptorSet descSet[] = {
            renderer->GetGlobalDescriptorSet(),
            packet.Material->GetDescriptorSet()
        };
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS,
            parentMaterial->PipelineLayout, 0, 2, descSet, 0, nullptr);

        vkCmdPushConstants(cmd, parentMaterial->PipelineLayout,
            VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4),
            &packet.TransformMatrix);

        FVulkanMesh& gpuMesh = renderer->UploadMesh(packet.Mesh.get());
        VkBuffer vertexBuffers[] = {gpuMesh.VertexBuffer.Buffer};
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(cmd, gpuMesh.IndexBuffer.Buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(cmd, gpuMesh.IndexCount, 1, 0, 0, 0);
    }
}

void IRenderPipeline::CreateFramebuffers(VkDevice device, VkRenderPass renderPass,
    VkExtent2D extent, const std::vector<VkImageView>& swapchainImageViews,
    const std::vector<VkImageView>& sharedAttachments,
    std::vector<VkFramebuffer>& outFramebuffers)
{
    const uint32_t imageCount = static_cast<uint32_t>(swapchainImageViews.size());
    const uint32_t totalAttachments = 1 + static_cast<uint32_t>(sharedAttachments.size());
    outFramebuffers.resize(imageCount);

    std::vector<VkImageView> fbAttachments(totalAttachments);
    // Copy shared attachments into positions [1..N-1] once
    for (size_t j = 0; j < sharedAttachments.size(); ++j) {
        fbAttachments[1 + j] = sharedAttachments[j];
    }

    for (uint32_t i = 0; i < imageCount; ++i) {
        fbAttachments[0] = swapchainImageViews[i]; // Per-framebuffer swapchain image

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = totalAttachments;
        framebufferInfo.pAttachments = fbAttachments.data();
        framebufferInfo.width = extent.width;
        framebufferInfo.height = extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &outFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create framebuffer!");
        }
    }
}
