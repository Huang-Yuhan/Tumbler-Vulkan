#pragma once
#include <vulkan/vulkan.h>
#include "VulkanTypes.h"

class VulkanRenderer;
struct SceneViewData;
struct RenderPacket;

class ShadowRenderer {
public:
    static constexpr uint32_t kShadowMapSize = 2048;

    void Init(VulkanRenderer* renderer);
    void Cleanup(VulkanRenderer* renderer);

    void RecordDepthPass(VkCommandBuffer cmd, VulkanRenderer* renderer,
        const SceneViewData& viewData, const std::vector<RenderPacket>& renderPackets);

    [[nodiscard]] VkSampler GetShadowSampler() const { return ShadowSampler; }
    [[nodiscard]] VkImageView GetShadowMapView() const { return ShadowMapView; }
    [[nodiscard]] VkDescriptorSet GetShadowDescriptorSet() const { return ShadowDescriptorSet; }

private:
    AllocatedImage ShadowMap{};
    VkImageView ShadowMapView = VK_NULL_HANDLE;
    VkSampler ShadowSampler = VK_NULL_HANDLE;

    VkRenderPass ShadowRenderPass = VK_NULL_HANDLE;
    VkFramebuffer ShadowFramebuffer = VK_NULL_HANDLE;

    VkDescriptorSetLayout ShadowSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout ShadowPipelineLayout = VK_NULL_HANDLE;
    VkPipeline ShadowPipeline = VK_NULL_HANDLE;

    VkDescriptorPool ShadowDescriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet ShadowDescriptorSet = VK_NULL_HANDLE;
    AllocatedBuffer ShadowUniformBuffer{};
};
