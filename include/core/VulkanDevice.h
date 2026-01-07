#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace Tumbler {

    // 辅助结构：存储队列族的索引
    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        [[nodiscard]]
        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

    //
    //  封装了Vulkan中的Device部分
    //  physical device:                         物理设备：实体显卡，就是电脑上的独立显卡或者集成显卡，read only
    //  logical device:                                  逻辑设备：操控物理设备的接口，用于创建资源和执行操作，
    //  理论上一个物理设备可以创建多个逻辑设备，但是在Vulkan中通常是11对应的关系，这是出于性能的考虑，因为一个Device中创建的资源给另一个Device并不方便
    //
    class VulkanDevice {
    public:
        // 需要传入 Instance (用于枚举物理设备) 和 Surface (用于检查显示支持)
        VulkanDevice(VkInstance instance, VkSurfaceKHR surface);
        ~VulkanDevice() noexcept;

        // 禁止拷贝 (RAII)
        VulkanDevice(const VulkanDevice&) = delete;
        VulkanDevice& operator=(const VulkanDevice&) = delete;

        // Getters
        [[nodiscard]]
        VkDevice getDevice() const { return logicalDevice; }
        [[nodiscard]]
        VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
        [[nodiscard]]
        VkQueue getGraphicsQueue() const { return graphicsQueue; }
        [[nodiscard]]
        VkQueue getPresentQueue() const { return presentQueue; }
        [[nodiscard]]
        VkCommandPool getCommandPool() const { return commandPool; }
        [[nodiscard]]
        QueueFamilyIndices getQueueFamilies() const { return indices; }

        // 核心工具：查找适合的内存类型 (Buffer/Image创建时使用)
        uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    private:
        void pickPhysicalDevice(VkInstance instance);
        void createLogicalDevice();
        void createCommandPool();

        // 内部检查函数
        bool isDeviceSuitable(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);

        // 成员变量
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // 物理设备 (不需销毁)
        VkDevice logicalDevice = VK_NULL_HANDLE;                 // 逻辑设备 (需销毁)
        VkSurfaceKHR surface;                             // 引用 (不拥有)

        VkQueue graphicsQueue;
        VkQueue presentQueue;
        VkCommandPool commandPool; // 默认的主绘图命令池

        QueueFamilyIndices indices;

        // 设备所需的扩展 (主要是 Swapchain)
        const std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };
    };

}