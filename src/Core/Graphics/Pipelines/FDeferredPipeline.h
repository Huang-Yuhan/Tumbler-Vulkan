#pragma once
#include "Core/Graphics/IRenderPipeline.h"
#include <vector>
#include <vulkan/vulkan.h>
#include "Core/Graphics/VulkanTypes.h"  // For AllocatedImage

class FDeferredPipeline final : public IRenderPipeline {
public:
    FDeferredPipeline() = default;
    ~FDeferredPipeline() override = default;

    void Init(VulkanRenderer* renderer) override;
    void Cleanup(VulkanRenderer* renderer) override;
    void RecreateResources(VulkanRenderer* renderer) override;

    void RecordCommands(
        VkCommandBuffer cmdBuffer,
        uint32_t imageIndex,
        VulkanRenderer* renderer,
        const SceneViewData& viewData,
        const std::vector<RenderPacket>& renderPackets,
        std::function<void(VkCommandBuffer)> onUIRender) override;

    [[nodiscard]] VkRenderPass GetRenderPass() const override { return RenderPass; }

private:
    void InitRenderPass(VulkanRenderer* renderer);
    void InitGBuffers(VulkanRenderer* renderer);
    void InitFramebuffers(VulkanRenderer* renderer);
    void DestroyGBuffers(VulkanRenderer* renderer);

    void InitLightingPipeline(VulkanRenderer* renderer);
    void UpdateLightingDescriptorSet(VulkanRenderer* renderer);

    VkRenderPass RenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> Framebuffers;

    // G-Buffer 图像资源 (Albedo, Normal) - 深度复用 Swapchain Depth
    AllocatedImage AlbedoImage;
    VkImageView AlbedoImageView = VK_NULL_HANDLE;

    AllocatedImage NormalImage;
    VkImageView NormalImageView = VK_NULL_HANDLE;
    
    // 全屏光照专属管线和描述符
    VkDescriptorSetLayout LightingSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout LightingPipelineLayout = VK_NULL_HANDLE;
    VkPipeline LightingPipeline = VK_NULL_HANDLE;
    VkDescriptorPool LightingDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet LightingDescriptorSet = VK_NULL_HANDLE;
};
