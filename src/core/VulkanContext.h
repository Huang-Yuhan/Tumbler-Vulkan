#pragma once

#include "VulkanWindow.h"
#include <vulkan/vulkan.h>
#include <vector>
#include <iostream>

namespace Tumbler {

    class VulkanContext {
    public:
#ifdef NDEBUG
        const bool enableValidationLayers = false;
#else
        const bool enableValidationLayers = true;
#endif

        VulkanContext(VulkanWindow& window);
        ~VulkanContext();

        // 禁止拷贝
        VulkanContext(const VulkanContext&) = delete;
        VulkanContext& operator=(const VulkanContext&) = delete;

        VkInstance getInstance() const { return instance; }

        // 静态辅助函数：供 Instance 创建时填充 Debug 信息
        static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    private:
        void createInstance();
        void setupDebugMessenger();
        bool checkValidationLayerSupport();
        std::vector<const char*> getRequiredExtensions();

        // 两个核心句柄
        VkInstance instance;
        VkDebugUtilsMessengerEXT debugMessenger;

        // 依赖窗口获取扩展
        VulkanWindow& window;

        const std::vector<const char*> validationLayers = {
            "VK_LAYER_KHRONOS_validation"
        };
    };

} // namespace Tumbler