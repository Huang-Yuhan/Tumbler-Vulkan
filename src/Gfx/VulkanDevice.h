#pragma once

#include <expected>
#include <vulkan/vulkan.h>

namespace Tumbler {

enum class DeviceError {
    InstanceCreationFailed,
    NoSuitablePhysicalDevice,
    DeviceCreationFailed,
};

// 从 Surface 查询到的队列族信息
struct QueueFamilyIndices {
    uint32_t graphics = ~0u;
    uint32_t present  = ~0u;
    bool IsComplete() const { return graphics != ~0u && present != ~0u; }
};

class VulkanDevice {
public:
    // Two-phase init: instance first (for surface creation),
    // then physical + logical device (needs surface).
    std::expected<void, DeviceError> CreateInstance();
    std::expected<void, DeviceError> CompleteInit(VkSurfaceKHR surface);
    void Shutdown();

    VkInstance       GetInstance()       const { return m_Instance; }
    VkPhysicalDevice  GetPhysicalDevice() const { return m_PhysicalDevice; }
    VkDevice          GetDevice()         const { return m_Device; }
    VkQueue           GetGraphicsQueue()  const { return m_GraphicsQueue; }
    VkQueue           GetPresentQueue()   const { return m_PresentQueue; }
    const QueueFamilyIndices& GetQueueFamilies() const { return m_QueueFamilies; }

private:
    std::expected<void, DeviceError> PickPhysicalDevice(VkSurfaceKHR surface);
    std::expected<void, DeviceError> CreateLogicalDevice();

    VkInstance       m_Instance       = VK_NULL_HANDLE;
    VkPhysicalDevice  m_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice          m_Device         = VK_NULL_HANDLE;
    VkQueue           m_GraphicsQueue  = VK_NULL_HANDLE;
    VkQueue           m_PresentQueue   = VK_NULL_HANDLE;
    QueueFamilyIndices m_QueueFamilies;
};

} // namespace Tumbler
