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

class AppWindow;

class VulkanContext {
public:
    bool Init();
    bool Init(AppWindow* window);
    void Shutdown();

    VkInstance GetInstance() const { return m_Instance; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice GetDevice() const { return m_Device; }
    VkQueue GetGraphicsQueue() const { return m_GraphicsQueue; }
    VkQueue GetPresentQueue() const { return m_PresentQueue; }
    uint32_t GetGraphicsQueueFamily() const { return m_GraphicsQueueFamily; }
    uint32_t GetPresentQueueFamily() const { return m_PresentQueueFamily; }
    VkSurfaceKHR GetSurface() const { return m_Surface; }

    VulkanContext() = default;
    ~VulkanContext() = default;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;

private:
    void CreateInstance();
    void SelectPhysicalDevice();
    void CreateDevice();
    void CreateSurface(AppWindow* window);

    int ScoreDevice(VkPhysicalDevice device) const;
    bool SupportsRequiredFeatures(VkPhysicalDevice device) const;
    bool SupportsRequiredQueues(VkPhysicalDevice device, uint32_t* graphicsFamily, uint32_t* presentFamily) const;

    VkInstance m_Instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_Surface = VK_NULL_HANDLE;
    VkPhysicalDevice m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice m_Device = VK_NULL_HANDLE;
    VkQueue m_GraphicsQueue = VK_NULL_HANDLE;
    VkQueue m_PresentQueue = VK_NULL_HANDLE;
    uint32_t m_GraphicsQueueFamily = UINT32_MAX;
    uint32_t m_PresentQueueFamily = UINT32_MAX;
    bool m_Windowed = false;

#ifdef NDEBUG
    static constexpr bool kEnableValidationLayers = false;
#else
    static constexpr bool kEnableValidationLayers = true;
#endif
};

} // namespace Tumbler
