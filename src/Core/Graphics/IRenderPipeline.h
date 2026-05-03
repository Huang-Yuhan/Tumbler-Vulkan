#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <functional>
#include "SceneViewData.h"
#include "RenderPacket.h"

class VulkanRenderer;
class FVulkanMesh;

class IRenderPipeline {
public:
    virtual ~IRenderPipeline() = default;

    virtual void Init(VulkanRenderer* renderer) = 0;
    virtual void Cleanup(VulkanRenderer* renderer) = 0;
    virtual void RecreateResources(VulkanRenderer* renderer) = 0;

    virtual void RecordCommands(
        VkCommandBuffer cmd,
        uint32_t imageIndex,
        VulkanRenderer* renderer,
        const SceneViewData& viewData,
        const std::vector<RenderPacket>& renderPackets) = 0;

    [[nodiscard]] virtual VkRenderPass GetRenderPass() const = 0;

    [[nodiscard]] virtual bool SupportsGBuffer() const { return false; }
    [[nodiscard]] virtual VkImageView GetGBufferAlbedoImageView() const { return VK_NULL_HANDLE; }
    [[nodiscard]] virtual VkImageView GetGBufferNormalImageView() const { return VK_NULL_HANDLE; }
    [[nodiscard]] virtual VkImageView GetGBufferDepthImageView() const { return VK_NULL_HANDLE; }

    // Shared helpers for pipeline implementations
    static void DrawMeshPackets(VkCommandBuffer cmd, VulkanRenderer* renderer,
        ERenderPath renderPath, const std::vector<RenderPacket>& renderPackets);

    static void CreateFramebuffers(VkDevice device, VkRenderPass renderPass,
        VkExtent2D extent, const std::vector<VkImageView>& swapchainImageViews,
        const std::vector<VkImageView>& sharedAttachments,
        std::vector<VkFramebuffer>& outFramebuffers);
};
