#include "VulkanDevice.h"
#include "Core/Utils/Log.h"

#include <GLFW/glfw3.h>

#include <cstring>
#include <set>
#include <vector>

namespace Tumbler {

// ===================================================================
// Init / Shutdown
// ===================================================================

std::expected<void, DeviceError> VulkanDevice::CompleteInit(VkSurfaceKHR surface) {
    auto physicalResult = PickPhysicalDevice(surface);
    if (!physicalResult) return std::unexpected(physicalResult.error());

    auto deviceResult = CreateLogicalDevice();
    if (!deviceResult) return std::unexpected(deviceResult.error());

    return {};
}

void VulkanDevice::Shutdown() {
    if (m_Device) {
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }
    if (m_Instance) {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }
    LOG_INFO("VulkanDevice shutdown");
}

// ===================================================================
// Instance
// ===================================================================

std::expected<void, DeviceError> VulkanDevice::CreateInstance() {
    VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Tumbler",
        .applicationVersion = VK_MAKE_VERSION(0, 2, 0),
        .pEngineName = "Tumbler",
        .engineVersion = VK_MAKE_VERSION(0, 2, 0),
        .apiVersion = VK_API_VERSION_1_4,
    };

    // ---- Layers ----
    std::vector<const char*> layers;

#ifndef NDEBUG
    const char* validationLayer = "VK_LAYER_KHRONOS_validation";

    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    bool found = false;
    for (const auto& layer : availableLayers) {
        if (std::strcmp(layer.layerName, validationLayer) == 0) {
            found = true;
            break;
        }
    }
    if (found) {
        layers.push_back(validationLayer);
        LOG_INFO("Validation layer enabled");
    } else {
        LOG_WARN("Validation layer not available");
    }
#endif

    // ---- Extensions ----
    uint32_t glfwExtCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtCount);

#ifndef NDEBUG
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    VkInstanceCreateInfo instanceInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = static_cast<uint32_t>(layers.size()),
        .ppEnabledLayerNames = layers.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data(),
    };

    if (vkCreateInstance(&instanceInfo, nullptr, &m_Instance) != VK_SUCCESS) {
        LOG_ERROR("vkCreateInstance failed");
        return std::unexpected(DeviceError::InstanceCreationFailed);
    }

    LOG_INFO("Vulkan instance created (1.4)");
    return {};
}

// ===================================================================
// Physical Device
// ===================================================================

static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());

    for (uint32_t i = 0; i < familyCount; i++) {
        if (families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics = i;
        }

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.present = i;
        }

        if (indices.IsComplete()) break;
    }

    return indices;
}

std::expected<void, DeviceError> VulkanDevice::PickPhysicalDevice(VkSurfaceKHR surface) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        LOG_ERROR("No Vulkan-capable physical devices found");
        return std::unexpected(DeviceError::NoSuitablePhysicalDevice);
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    for (const auto& device : devices) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(device, &props);

        if (props.apiVersion < VK_API_VERSION_1_4) {
            LOG_INFO("Skipping {}: Vulkan {}.{}.{} (need 1.4)",
                     props.deviceName,
                     VK_API_VERSION_MAJOR(props.apiVersion),
                     VK_API_VERSION_MINOR(props.apiVersion),
                     VK_API_VERSION_PATCH(props.apiVersion));
            continue;
        }

        auto families = FindQueueFamilies(device, surface);
        if (!families.IsComplete()) continue;

        // Prefer discrete GPU, but accept any
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ||
            m_PhysicalDevice == VK_NULL_HANDLE) {
            m_PhysicalDevice = device;
            m_QueueFamilies = families;

            LOG_INFO("Physical device: {} (type {})", props.deviceName,
                     props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? "discrete" :
                     props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? "integrated" : "other");

            if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) break;
        }
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("No suitable physical device found");
        return std::unexpected(DeviceError::NoSuitablePhysicalDevice);
    }

    return {};
}

// ===================================================================
// Logical Device
// ===================================================================

std::expected<void, DeviceError> VulkanDevice::CreateLogicalDevice() {
    // Deduplicate queue families: graphics and present might be the same
    std::set<uint32_t> uniqueFamilies = {m_QueueFamilies.graphics, m_QueueFamilies.present};

    std::vector<VkDeviceQueueCreateInfo> queueInfos;
    float queuePriority = 1.0f;

    for (uint32_t family : uniqueFamilies) {
        queueInfos.push_back({
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = family,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority,
        });
    }

    // Vulkan 1.1 features
    VkPhysicalDeviceVulkan11Features features11{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES,
        .shaderDrawParameters = VK_TRUE,
    };

    // Vulkan 1.3 features
    VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .pNext = &features11,
        .dynamicRendering = VK_TRUE,
    };

    // Vulkan 1.2 features required by GPU-driven pipeline
    VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .pNext = &features13,
        .drawIndirectCount = VK_TRUE,
        .descriptorIndexing = VK_TRUE,
        .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
        .descriptorBindingSampledImageUpdateAfterBind = VK_TRUE,
        .descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE,
        .descriptorBindingPartiallyBound = VK_TRUE,
        .descriptorBindingVariableDescriptorCount = VK_TRUE,
        .runtimeDescriptorArray = VK_TRUE,
        .timelineSemaphore = VK_TRUE,
        .bufferDeviceAddress = VK_TRUE,
    };

    // Base features (Vulkan 1.0)
    VkPhysicalDeviceFeatures2 features2{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features12,
        .features = {
            .fillModeNonSolid = VK_TRUE,  // VK_POLYGON_MODE_LINE
        },
    };

    // Swapchain extension is still needed even in Vulkan 1.4
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };

    VkDeviceCreateInfo deviceInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &features2,
        .queueCreateInfoCount = static_cast<uint32_t>(queueInfos.size()),
        .pQueueCreateInfos = queueInfos.data(),
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
    };

    if (vkCreateDevice(m_PhysicalDevice, &deviceInfo, nullptr, &m_Device) != VK_SUCCESS) {
        LOG_ERROR("vkCreateDevice failed");
        return std::unexpected(DeviceError::DeviceCreationFailed);
    }

    vkGetDeviceQueue(m_Device, m_QueueFamilies.graphics, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_QueueFamilies.present, 0, &m_PresentQueue);

    LOG_INFO("Logical device created");
    return {};
}

} // namespace Tumbler
