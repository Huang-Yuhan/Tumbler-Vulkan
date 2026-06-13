#include "VulkanContext.h"
#include <Core/Platform/AppWindow.h>
#include <Core/Utils/Log.h>

#include <stdexcept>
#include <set>
#include <iostream>

#include "Core/Utils/VulkanUtils.h"

// ==========================================
// 全局配置：Validation Layers
// ==========================================

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif



const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation" // Vulkan 的标准验证层
};

// ==========================================
// 辅助函数：由于 CreateDebugUtilsMessengerEXT 是扩展函数，需要动态加载
// ==========================================
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

// 调试回调函数：Vulkan 报错时会调用这个函数
static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData) {

    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        LOG_ERROR("[Vulkan Validation] {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_WARN("[Vulkan Validation] {}", pCallbackData->pMessage);
    }
    // else { LOG_INFO("[Vulkan] {}", pCallbackData->pMessage); } // 除非调试否则太吵了

    return VK_FALSE;
}

// ==========================================
// 类实现
// ==========================================

void VulkanContext::Init(AppWindow* window) {

    LOG_INFO("================= VulkanContext Initialization Started ================");

    LOG_INFO("Ready to create Vulkan Instance");

    // 1. 创建 Instance
    CreateInstance();

    if constexpr (enableValidationLayers)
    {
        // 2. 设置调试信息
        LOG_INFO("Setting up Debug Messenger");
        SetupDebugMessenger();
    }

    // 3. 创建 Surface (连接窗口)
    // 必须在 PickPhysicalDevice 之前，因为我们需要检查显卡是否支持画到这个窗口上
    LOG_INFO("Creating Vulkan Surface");
    Surface = window->CreateSurface(Instance);

    // 4. 选择物理显卡
    LOG_INFO("Picking Physical Device");
    PickPhysicalDevice();

    // 5. 创建逻辑设备
    LOG_INFO("Creating Logical Device");
    CreateLogicalDevice();

    // 6. 初始化 VMA
    LOG_INFO("Initializing VMA Allocator");
    InitVMA();

    LOG_INFO("================= VulkanContext Initialized Successfully ================");
}

void VulkanContext::Cleanup() {

    if (Device!= VK_NULL_HANDLE) {
        vkDeviceWaitIdle(Device); // 等待设备空闲，防止资源正在使用时销毁
    }

    LOG_INFO("================= VulkanContext Cleanup Started ================");

    if (Allocator != VK_NULL_HANDLE) {
        vmaDestroyAllocator(std::exchange(Allocator, VK_NULL_HANDLE));
        LOG_INFO("VMA Allocator Destroyed");
    }


    if (Device != VK_NULL_HANDLE) {
        vkDestroyDevice(std::exchange(Device, VK_NULL_HANDLE), nullptr);
        LOG_INFO("Logical Device Destroyed");

    }

    if (Surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(Instance, std::exchange(Surface, VK_NULL_HANDLE), nullptr);
        LOG_INFO("Surface Destroyed");
    }

    if constexpr (enableValidationLayers)
    {
        if (DebugMessenger != VK_NULL_HANDLE) {
            DestroyDebugUtilsMessengerEXT(Instance, std::exchange(DebugMessenger, VK_NULL_HANDLE), nullptr);
            LOG_INFO("Debug Messenger Destroyed");
        }
    }

    if (Instance != VK_NULL_HANDLE) {
        vkDestroyInstance(std::exchange(Instance, VK_NULL_HANDLE), nullptr);
        LOG_INFO("Vulkan Instance Destroyed");
    }

    LOG_INFO("================= VulkanContext Cleaned Up Successfully ================");

}

void VulkanContext::CreateInstance()
{

    constexpr VkApplicationInfo appInfo{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Tumbler Engine",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Tumbler",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3
    };


    auto debugCreateInfo = PopulateDebugMessengerCreateInfo();
    void* pNextChain = nullptr;
    uint32_t layerCount = 0;
    const char* const* layerNames = nullptr;
    auto extensions = GetRequiredExtensions();
    if constexpr (enableValidationLayers)
    {
        if (!CheckValidationLayerSupport())
            throw std::runtime_error("Validation layers requested, but not available!");

        layerCount = static_cast<uint32_t>(validationLayers.size());
        layerNames = validationLayers.data();
        pNextChain = &debugCreateInfo;
    }

    const VkInstanceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = pNextChain,  // 指向 debugCreateInfo 或者 nullptr
        .pApplicationInfo = &appInfo,
        .enabledLayerCount = layerCount,
        .ppEnabledLayerNames = layerNames,
        .enabledExtensionCount = static_cast<uint32_t>(extensions.size()),
        .ppEnabledExtensionNames = extensions.data()
    };

    VK_CHECK(vkCreateInstance(&createInfo, nullptr, &Instance));
    LOG_INFO("Vulkan Instance Created");
}

void VulkanContext::SetupDebugMessenger() {

    /*
     * 这里和创建Instance有所不同
     * 创建Instance的时候也有可能崩溃，所以要把DebugMessenger的CreateInfo链到Instance的CreateInfo里，那个时候还没有DebugMessenger，是一个一次性的DebugMessenger
     * 而这里是在Instance创建好之后，专门创建一个DebugMessenger，所以直接调用CreateDebugUtilsMessengerEXT函数即可
     另外，这个DebugMessenger会一直存在到Context销毁为止
     当然，你也可以选择不创建DebugMessenger，但是这样就收不到Validation Layer的回调了
     这对于调试来说是非常不利的
     *
     */

    auto createInfo = PopulateDebugMessengerCreateInfo();

    VK_CHECK(CreateDebugUtilsMessengerEXT(Instance, &createInfo, nullptr, &DebugMessenger));
}

void VulkanContext::PickPhysicalDevice() {

    auto devices = VkUtils::GetVector<VkPhysicalDevice>(vkEnumeratePhysicalDevices, Instance);

    assert(!devices.empty() && "Failed to find GPUs with Vulkan support!");

    for (const auto& device : devices) {
        QueueFamilyIndices indices = FindQueueFamilies(device, Surface);
        if (indices.isComplete()) {
            PhysicalDevice = device;
            GraphicsQueueFamilyIndex = indices.graphicsFamily.value();
            PresentQueueFamilyIndex = indices.presentFamily.value();
            break;
        }
    }

    if (PhysicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("Failed to find a suitable GPU!");
    }
}

void VulkanContext::CreateLogicalDevice() {
    // 1. 描述队列
    float queuePriority = 1.0f;
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {GraphicsQueueFamilyIndex, PresentQueueFamilyIndex};

    queueCreateInfos.reserve(uniqueQueueFamilies.size());
    for (uint32_t queueFamilyIndex : uniqueQueueFamilies) {
        queueCreateInfos.push_back(VkDeviceQueueCreateInfo{
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queueFamilyIndex,
            .queueCount = 1,
            .pQueuePriorities = &queuePriority
        });
    }

    // 2. 描述逻辑设备特性 (暂时什么都不开启)
    VkPhysicalDeviceFeatures deviceFeatures{
        .samplerAnisotropy = VK_TRUE
    };

    // 3. 创建逻辑设备

    uint32_t layerCount = 0;
    const char* const* layerNames = nullptr;
    const std::vector<const char*> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    if constexpr (enableValidationLayers)
    {
        layerCount = static_cast<uint32_t>(validationLayers.size());
        layerNames = validationLayers.data();
    }

    const VkDeviceCreateInfo createInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
        .pQueueCreateInfos = queueCreateInfos.data(),
        .enabledLayerCount = layerCount,
        .ppEnabledLayerNames = layerNames,
        .enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
        .ppEnabledExtensionNames = deviceExtensions.data(),
        .pEnabledFeatures = &deviceFeatures
    };



    VK_CHECK(vkCreateDevice(PhysicalDevice, &createInfo, nullptr, &Device));

    // 4. 获取队列句柄
    vkGetDeviceQueue(Device, GraphicsQueueFamilyIndex, 0, &GraphicsQueue);
    vkGetDeviceQueue(Device, PresentQueueFamilyIndex, 0, &PresentQueue);
    LOG_INFO("Logical Device Created");
}

void VulkanContext::InitVMA() {

    VmaAllocatorCreateInfo allocatorInfo{
        .physicalDevice = PhysicalDevice,
        .device = Device,
        .instance = Instance,
        .vulkanApiVersion = VK_API_VERSION_1_3
    };

    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &Allocator));
    LOG_INFO("VMA Allocator Initialized");
}

bool VulkanContext::CheckValidationLayerSupport() {
    auto availableLayers = VkUtils::GetVector<VkLayerProperties>(vkEnumerateInstanceLayerProperties);

    for (auto &layerName:validationLayers)
    {
        if (std::ranges::find_if(availableLayers, [layerName](const VkLayerProperties& layerProperties) {
            return strcmp(layerName, layerProperties.layerName) == 0;
        }) == availableLayers.end()) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanContext::GetRequiredExtensions() {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // 【新增】严格检查 GLFW 是否成功获取到了平台扩展
    if (glfwExtensions == nullptr) {
        const char* description;
        int code = glfwGetError(&description);
        LOG_CRITICAL("GLFW failed to find required Vulkan surface extensions! Code: {}, Desc: {}",
                     code, description ? description : "Unknown");
        throw std::runtime_error("glfwGetRequiredInstanceExtensions returned NULL");
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // 【新增】打印一下它到底请求了什么扩展，心里有底
    for (uint32_t i = 0; i < glfwExtensionCount; i++) {
        LOG_INFO("GLFW Requested Extension: {}", glfwExtensions[i]);
    }

    if constexpr (enableValidationLayers) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    return extensions;
}

VkDebugUtilsMessengerCreateInfoEXT VulkanContext::PopulateDebugMessengerCreateInfo()
{
    return VkDebugUtilsMessengerCreateInfoEXT{
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = DebugCallback,
        .pUserData = nullptr
    };
}

QueueFamilyIndices VulkanContext::FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface)
{
    QueueFamilyIndices indices;

    auto queueFamilies = VkUtils::GetVector<VkQueueFamilyProperties>(vkGetPhysicalDeviceQueueFamilyProperties, device);

    for (auto i=0; i<queueFamilies.size(); i++) {
        const auto& queueFamily = queueFamilies[i];

        // 检查图形队列
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily=i;
        }

        // 检查呈现支持
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily=i;
        }

        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

