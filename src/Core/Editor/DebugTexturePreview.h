#pragma once

#include <vulkan/vulkan.h>

class VulkanRenderer;

class DebugTexturePreview {
public:
    DebugTexturePreview() = default;
    ~DebugTexturePreview() = default;

    void Init(VulkanRenderer* renderer);
    void Cleanup(VkDevice device);

    void SetImage(int slot, VkImageView imageView);
    [[nodiscard]] VkDescriptorSet GetTextureID(int slot) const;

    static constexpr int kMaxSlots = 2;

private:
    VulkanRenderer* Renderer = nullptr;
    VkSampler Sampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout SetLayout = VK_NULL_HANDLE;
    VkDescriptorSet DescriptorSets[kMaxSlots] = {VK_NULL_HANDLE, VK_NULL_HANDLE};
};
