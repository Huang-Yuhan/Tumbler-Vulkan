#include <iostream>
#include <stdexcept>
#include <core/VulkanDevice.h>

namespace Tumbler {
    VulkanDevice::VulkanDevice(VkInstance instance, VkSurfaceKHR surface)
            : surface(surface) {
        pickPhysicalDevice(instance);
        createLogicalDevice();
        createCommandPool();
    }

    VulkanDevice::~VulkanDevice() {
        //先销毁commandpool,然后销毁设备
        vkDestroyCommandPool(logicalDevice, commandPool, nullptr);
        vkDestroyDevice(logicalDevice, nullptr);
    }

    void VulkanDevice::pickPhysicalDevice(VkInstance instance) {
        //寻找电脑上所有可用的物理设备
        uint32_t physicalDeviceCount=0;
        vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);
        if (physicalDeviceCount==0) {
            throw std::runtime_error("No physical device found");
        }
        std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
        vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

        //找到一个合适的物理设备
        {
            // 使用 lambda 表达式作为查找条件
            // 注意：必须捕获 [this] 才能调用成员函数 isDeviceSuitable
            const auto it = std::ranges::find_if(physicalDevices, [this](const auto& device) {
                return isDeviceSuitable(device);
            });

            // 检查是否找到了
            if (it != physicalDevices.end()) {
                physicalDevice = *it; // 解引用迭代器获取值
            } else {
                throw std::runtime_error("failed to find a suitable GPU!");
            }
        }

        // 打印选中的设备名称（可选，用于调试）
        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(physicalDevice, &properties);
        std::cout << "Selected GPU: " << properties.deviceName << std::endl;

        // 缓存队列索引
        indices = findQueueFamilies(physicalDevice);
    }
    bool VulkanDevice::isDeviceSuitable(VkPhysicalDevice device) {
        QueueFamilyIndices queue_family_indices = findQueueFamilies(device);
        bool extensionsSupported = checkDeviceExtensionSupport(device);

        bool swapChainAdequate = false;
        if (extensionsSupported) {
            // 检查 Swapchain 格式和呈现模式是否支持
            // 这里简化处理，实际需要调用 vkGetPhysicalDeviceSurfaceFormatsKHR 等
            // 通常只要 extensionsSupported 为 true 且 Surface 创建成功，大部分现代 GPU 都支持
            swapChainAdequate = true;
        }

        // 获取特性检查各向异性过滤支持
        VkPhysicalDeviceFeatures supportedFeatures;
        vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

        return queue_family_indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
    }

    QueueFamilyIndices VulkanDevice::findQueueFamilies(VkPhysicalDevice device) const {
        QueueFamilyIndices queue_family_indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

        int i = 0;
        for (const auto& queueFamily : queueFamilies) {
            // 检查图形支持
            if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                queue_family_indices.graphicsFamily = i;
            }

            // 检查显示支持 (Surface)
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
            if (presentSupport) {
                queue_family_indices.presentFamily = i;
            }

            if (queue_family_indices.isComplete()) {
                break;
            }
            i++;
        }

        return queue_family_indices;
    }
}
