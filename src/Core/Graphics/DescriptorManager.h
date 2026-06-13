// DescriptorManager.h — Descriptor Set 布局与分配管理
//
// 职责: 管理全局 DescriptorPool, 创建 Set 0 (Global) 和 Set 1 (Bindless) 的 Layout,
//       提供 Set 分配和更新接口.
//
// 依赖: VulkanContext
// 层级: 图形基础设施 (Phase 3)

#pragma once

#include <vector>
#include <vulkan/vulkan.h>

namespace Tumbler {

class DescriptorManager {
public:
    bool Init(VkDevice device, uint32_t maxTextures);
    void Shutdown();

    VkDescriptorSetLayout GetSet0Layout() const { return m_Set0Layout; }
    VkDescriptorSetLayout GetSet1Layout() const { return m_Set1Layout; }

    VkDescriptorSet AllocateSet(VkDescriptorSetLayout layout);
    void FreeSet(VkDescriptorSet set);

    void UpdateSet0(VkDescriptorSet set, VkBuffer sceneUBO, VkImageView shadowMapView, VkSampler shadowSampler);
    void UpdateSet1Bindless(VkDescriptorSet set, const std::vector<VkImageView>& textureViews, VkBuffer materialSSBO,
                            VkBuffer objectSSBO);

    DescriptorManager() = default;
    ~DescriptorManager() = default;
    DescriptorManager(const DescriptorManager&) = delete;
    DescriptorManager& operator=(const DescriptorManager&) = delete;

private:
    VkDevice m_Device = VK_NULL_HANDLE;
    VkDescriptorPool m_Pool = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_Set0Layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_Set1Layout = VK_NULL_HANDLE;
    uint32_t m_MaxTextures = 0;
};

} // namespace Tumbler
