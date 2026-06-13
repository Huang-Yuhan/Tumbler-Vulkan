#include "GpuVulkanContext.h"
#include "Core/Platform/GpuAppWindow.h"
#include "Core/Utils/Log.h"
#include "VulkanUtils.h"

#include <GLFW/glfw3.h>

#include <set>
#include <vector>

namespace Tumbler {

bool GpuContext::Init() {
    m_Windowed = false;
    CreateInstance();
    SelectPhysicalDevice();
    CreateDevice();
    LOG_INFO("GpuContext initialized (Vulkan 1.4)");
    return m_Instance && m_PhysicalDevice && m_Device;
}

bool GpuContext::Init(GpuAppWindow* window) {
    m_Windowed = true;
    CreateInstance();
    CreateSurface(window);
    SelectPhysicalDevice();
    CreateDevice();
    LOG_INFO("GpuContext initialized with window surface (Vulkan 1.4)");
    return m_Instance && m_Surface && m_PhysicalDevice && m_Device;
}

void GpuContext::Shutdown() {
    if (m_Device) {
        vkDestroyDevice(m_Device, nullptr);
        m_Device = VK_NULL_HANDLE;
    }
    if (m_Surface) {
        vkDestroySurfaceKHR(m_Instance, m_Surface, nullptr);
        m_Surface = VK_NULL_HANDLE;
    }
    if (m_Instance) {
        vkDestroyInstance(m_Instance, nullptr);
        m_Instance = VK_NULL_HANDLE;
    }

    m_PhysicalDevice = VK_NULL_HANDLE;
    m_GraphicsQueue = VK_NULL_HANDLE;
    m_PresentQueue = VK_NULL_HANDLE;
    m_GraphicsQueueFamily = UINT32_MAX;
    m_PresentQueueFamily = UINT32_MAX;
    m_Windowed = false;

    LOG_INFO("GpuContext shutdown");
}

void GpuContext::CreateInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Tumbler";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Tumbler Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_4;

    std::vector<const char*> extensions;
    if (m_Windowed) {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        if (glfwExtensions == nullptr || glfwExtensionCount == 0) {
            LOG_ERROR("GLFW required Vulkan surface extensions are unavailable");
            return;
        }
        extensions.assign(glfwExtensions, glfwExtensions + glfwExtensionCount);
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.empty() ? nullptr : extensions.data();

    if constexpr (kEnableValidationLayers) {
        const char* validationLayer = "VK_LAYER_KHRONOS_validation";
        createInfo.enabledLayerCount = 1;
        createInfo.ppEnabledLayerNames = &validationLayer;
    }

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &m_Instance));
}

void GpuContext::CreateSurface(GpuAppWindow* window) {
    m_Surface = window->CreateSurface(m_Instance);
    if (m_Surface == VK_NULL_HANDLE) {
        LOG_ERROR("Failed to create Vulkan window surface");
    }
}

void GpuContext::SelectPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, nullptr);
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_Instance, &deviceCount, devices.data());

    int bestScore = -1;
    for (auto device : devices) {
        uint32_t graphicsFamily = UINT32_MAX;
        uint32_t presentFamily = UINT32_MAX;
        if (!SupportsRequiredQueues(device, &graphicsFamily, &presentFamily)) {
            continue;
        }

        int score = ScoreDevice(device);
        if (score > bestScore) {
            bestScore = score;
            m_PhysicalDevice = device;
            m_GraphicsQueueFamily = graphicsFamily;
            m_PresentQueueFamily = presentFamily;
        }
    }

    if (m_PhysicalDevice == VK_NULL_HANDLE) {
        LOG_ERROR("Failed to find a suitable Vulkan physical device");
        return;
    }

    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(m_PhysicalDevice, &props);
    LOG_INFO("Selected GPU: {}", props.deviceName);
}

int GpuContext::ScoreDevice(VkPhysicalDevice device) const {
    if (!SupportsRequiredFeatures(device)) {
        return -1000;
    }

    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceMemoryProperties memProps{};
    vkGetPhysicalDeviceProperties(device, &props);
    vkGetPhysicalDeviceMemoryProperties(device, &memProps);

    int score = 0;
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += 100;
    } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
        score += 1;
    } else if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
        return -1000;
    }

    VkDeviceSize totalVram = 0;
    for (uint32_t i = 0; i < memProps.memoryHeapCount; i++) {
        if (memProps.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
            totalVram += memProps.memoryHeaps[i].size;
        }
    }
    score += static_cast<int>(totalVram / (256ULL * 1024 * 1024));

    return score;
}

bool GpuContext::SupportsRequiredFeatures(VkPhysicalDevice device) const {
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

bool GpuContext::SupportsRequiredQueues(VkPhysicalDevice device, uint32_t* graphicsFamily,
                                           uint32_t* presentFamily) const {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            *graphicsFamily = i;
        }

        if (m_Windowed) {
            VkBool32 presentSupport = VK_FALSE;
            vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_Surface, &presentSupport);
            if (presentSupport) {
                *presentFamily = i;
            }
        } else {
            *presentFamily = *graphicsFamily;
        }

        if (*graphicsFamily != UINT32_MAX && *presentFamily != UINT32_MAX) {
            return true;
        }
    }

    return false;
}

void GpuContext::CreateDevice() {
    float queuePriority = 1.0f;
    std::set<uint32_t> uniqueQueueFamilies = {m_GraphicsQueueFamily};
    if (m_Windowed) {
        uniqueQueueFamilies.insert(m_PresentQueueFamily);
    }

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    queueCreateInfos.reserve(uniqueQueueFamilies.size());
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceVulkan12Features features12{};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.drawIndirectCount = VK_TRUE;
    features12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;

    std::vector<const char*> deviceExtensions;
    if (m_Windowed) {
        deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = &features12;
    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();
    deviceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.empty() ? nullptr : deviceExtensions.data();

    VK_CHECK(vkCreateDevice(m_PhysicalDevice, &deviceCreateInfo, nullptr, &m_Device));

    vkGetDeviceQueue(m_Device, m_GraphicsQueueFamily, 0, &m_GraphicsQueue);
    vkGetDeviceQueue(m_Device, m_PresentQueueFamily, 0, &m_PresentQueue);
}

} // namespace Tumbler
