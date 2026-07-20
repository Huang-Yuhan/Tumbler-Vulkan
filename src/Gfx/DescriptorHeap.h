#pragma once

#include <expected>
#include <span>
#include <vulkan/vulkan.h>

namespace Tumbler {

enum class DescriptorError {
    PoolCreationFailed,
    LayoutCreationFailed,
    SetAllocationFailed,
};

// Descriptor binding points
namespace Bindings {
    // Set 0 — Global (per-frame)
    constexpr uint32_t SceneUBO   = 0;
    constexpr uint32_t ShadowMap  = 1;

    // Set 1 — Bindless (init once, rarely updated)
    constexpr uint32_t Textures       = 0;  // SampledImage[]
    constexpr uint32_t MaterialData   = 1;  // SSBO
    constexpr uint32_t ClusterPageData = 2;  // ByteAddressBuffer
    constexpr uint32_t ObjectData     = 3;  // SSBO (future: per-instance transforms)
}

class DescriptorHeap {
public:
    std::expected<void, DescriptorError> Init(VkDevice device, uint32_t maxBindlessTextures);
    void Shutdown();

    VkDescriptorSetLayout GetSet0Layout() const { return m_Set0Layout; }
    VkDescriptorSetLayout GetSet1Layout() const { return m_Set1Layout; }
    VkDescriptorSet       GetSet0()       const { return m_Set0; }
    VkDescriptorSet       GetSet1()       const { return m_Set1; }

    // Pipeline layout combining Set 0 + Set 1
    std::expected<VkPipelineLayout, DescriptorError> CreatePipelineLayout(
        VkDevice device,
        std::span<const VkPushConstantRange> pushConstants = {});

    // Update Set 0 (per-frame: scene UBO, shadow map)
    void WriteSet0(VkBuffer sceneUbo, VkImageView shadowMap = VK_NULL_HANDLE,
                   VkSampler shadowSampler = VK_NULL_HANDLE);

    // Bindless texture registration — returns the index in the texture array
    uint32_t RegisterTexture(VkImageView view, VkSampler sampler);

private:
    VkDevice m_Device = VK_NULL_HANDLE;

    VkDescriptorPool      m_Pool      = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_Set0Layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_Set1Layout = VK_NULL_HANDLE;
    VkDescriptorSet       m_Set0       = VK_NULL_HANDLE;
    VkDescriptorSet       m_Set1       = VK_NULL_HANDLE;

    uint32_t m_MaxTextures = 0;
    uint32_t m_NextTextureIndex = 0;
    VkSampler m_DefaultSampler = VK_NULL_HANDLE;
};

} // namespace Tumbler
