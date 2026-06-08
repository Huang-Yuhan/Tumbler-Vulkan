// VulkanContext.h — Vulkan 实例与设备管理
//
// 职责: 创建 Vulkan 1.4 Instance + Device, 选择物理设备和 Queue 族,
//       启用 GPU-Driven 所需特性 (bufferDeviceAddress, descriptorIndexing, drawIndirectCount).
//       不持有 VMA, 不持有 Surface.
//
// 依赖: VulkanUtils.h
// 层级: 图形基础设施 (Phase 2)

#pragma once

#include <vulkan/vulkan.h>

namespace Tumbler {

class VulkanContext {
public:
    bool Init();
    void Shutdown();

    VkInstance GetInstance() const { return m_Instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice GetDevice() const { return m_Device; }
    VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
    uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }

    VulkanContext() = default;
    ~VulkanContext() = default;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

private:
    void CreateInstance();
    void SelectPhysicalDevice();
    void CreateDevice();

    int ScoreDevice(VkPhysicalDevice device) const;
    bool SupportsRequiredFeatures(VkPhysicalDevice device) const;

    VkInstance m_Instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    uint32_t m_GraphicsQueueFamily = 0;

#ifdef NDEBUG
    static constexpr bool kEnableValidationLayers = false;
#else
    static constexpr bool kEnableValidationLayers = true;
#endif
};

} // namespace Tumbler
