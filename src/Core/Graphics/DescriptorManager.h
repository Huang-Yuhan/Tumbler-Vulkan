#pragma once
#include <vulkan/vulkan.h>
#include "VulkanTypes.h"
#include "DescriptorSetFreeQueue.h"

class RenderDevice;

class DescriptorManager
{
public:
    static constexpr uint32_t kMaxDescriptorSets = 2000;

    void Init(VkDevice device, RenderDevice* renderDevice);
    void Cleanup(VkDevice device, RenderDevice* renderDevice);
    void UpdateShadowBinding(VkDevice device, VkSampler sampler, VkImageView shadowMapView);

    VkDescriptorSet AllocateDescriptorSet(VkDevice device, VkDescriptorSetLayout layout);
    void QueueDescriptorSetFree(VkDescriptorSet descriptorSet);
    void FlushPendingDescriptorSetFrees(VkDevice device);

    [[nodiscard]] VkDescriptorPool GetPool() const { return Pool; }
    [[nodiscard]] VkDescriptorSetLayout GetGlobalSetLayout() const { return GlobalSetLayout; }
    [[nodiscard]] VkDescriptorSet GetGlobalDescriptorSet() const { return GlobalDescriptorSet; }
    [[nodiscard]] const AllocatedBuffer& GetSceneParameterBuffer() const { return SceneParameterBuffer; }
    [[nodiscard]] size_t GetPendingFreeCount() const { return PendingFrees.Size(); }

private:
    VkDescriptorPool Pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout GlobalSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet GlobalDescriptorSet = VK_NULL_HANDLE;
    AllocatedBuffer SceneParameterBuffer{};
    DescriptorSetFreeQueue PendingFrees;
};
