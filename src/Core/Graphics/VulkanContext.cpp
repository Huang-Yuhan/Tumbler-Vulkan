#include "VulkanContext.h"
#include "Core/Utils/Log.h"
#include "VulkanUtils.h"

#include <vector>

namespace Tumbler {

bool VulkanContext::Init() {
    CreateInstance();
    SelectPhysicalDevice();
    CreateDevice();
    LOG_INFO("VulkanContext initialized (Vulkan 1.4)");
    return true;
}

void VulkanContext::Shutdown() {
    if (m_Device) {
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }
    if (m_Instance) {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }
    LOG_INFO("VulkanContext shutdown");
}

void VulkanContext::CreateInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Tumbler";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Tumbler Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    if constexpr (kEnableValidationLayers) {
        const char* validationLayer = "VK_LAYER_KHRONOS_validation";
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &validationLayer;
    }

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_Instance));
}

void VulkanContext::SelectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    int bestScore = -1;
    for (auto device : devices) {
        int score = ScoreDevice(device);
        if (score > bestScore) {
            bestScore = score;
            m_PhysicalDevice = device;
        }
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
    LOG_INFO("Selected GPU: {}", props.deviceName);
}

int VulkanContext::ScoreDevice(VkPhysicalDevice device) const {
    if (!SupportsRequiredFeatures(device)) {
        return -1000;
    }

    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceProperties(device, &props);
    vkGetPhysicalDeviceMemoryProperties(device, &memProps);

    int score = 0;

    // Discrete GPU 优先
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 100;
    } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 1;
    } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        return -1000;
    }

    // VRAM 越大分越高
    VkDeviceSize totalVram = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalVram += memProps.memoryHeaps[i].size;
        }
    }
    score += static_cast<int>(totalVram / (256ULL * 1024 * 1024));

    return score;
}

bool VulkanContext::SupportsRequiredFeatures(VkPhysicalDevice device) const {
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(device, &props);
    if (props.apiVersion < VK_API_VERSION_1_4) {
        return false;
    }

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.pNext = &features12;
    vkGetPhysicalDeviceFeatures2(device, &features2);

    return features12.bufferDeviceAddress && features12.descriptorIndexing && features12.drawIndirectCount &&
           features12.shaderSampledImageArrayNonUniformIndexing &&
           features12.descriptorBindingSampledImageUpdateAfterBind && features12.descriptorBindingPartiallyBound &&
           features12.runtimeDescriptorArray;
}

void VulkanContext::CreateDevice() {
    // 找到 Graphics Queue 族
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(m_PhysicalDevice, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            m_GraphicsQueueFamily = i;
            break;
        }
    }

    float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = m_GraphicsQueueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    // Vulkan 1.2 特性
    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.drawIndirectCount = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features12;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;

    VK_CHECK(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device));

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
}

} // namespace Tumbler
