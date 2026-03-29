#pragma once
#include "Core/Graphics/IRenderPipeline.h"
#include <vector>
#include <vulkan/vulkan.h>

class FForwardPipeline final : public IRenderPipeline {
public:
    FForwardPipeline() = default;
    ~FForwardPipeline() override = default;

    void Init(VulkanRenderer* renderer) override;
    void Cleanup(VulkanRenderer* renderer) override;
    void RecreateResources(VulkanRenderer* renderer) override;

    void RecordCommands(
        VkCommandBuffer cmd,
        uint32_t imageIndex,
        VulkanRenderer* renderer,
        const SceneViewData& viewData,
        const std::vector<RenderPacket>& renderPackets,
        std::function<void(VkCommandBuffer)> onUIRender) override;

    // 让 FMaterial 用来绑定 Forward 版本的 VkPipeline
    [[nodiscard]] VkRenderPass GetRenderPass() const { return RenderPass; }

private:
    void InitRenderPass(VulkanRenderer* renderer);
    void InitFramebuffers(VulkanRenderer* renderer);

    VkRenderPass RenderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> Framebuffers;
};
