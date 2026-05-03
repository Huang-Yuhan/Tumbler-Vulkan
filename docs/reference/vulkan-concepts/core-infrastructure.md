## 目录
1. [核心数据结构与金字塔](#1-核心数据结构与金字塔)
2. [表面 (VkSurfaceKHR)](#2-表面-vksurfacekhr)
3. [显存与资源的绝对把控 (VMA)](#3-显存与资源的绝对把控-vma)

## 1. 核心数据结构与金字塔

Vulkan 的架构建立在清晰的依赖链条上，**销毁时必须逆序执行**：

```
VkInstance (实例)
    ↓
VkPhysicalDevice (物理设备)
    ↓
VkDevice (逻辑设备)
    ↓
VkQueue (指令队列)
    ↓
VkBuffer / VkImage / VkPipeline / ... (具体资源)
```

### 1.1 VkInstance (实例)
- **作用**：连接应用程序与 Vulkan 驱动的顶层桥梁
- **职责**：
  - 加载 Vulkan 库
  - 启用全局扩展（Extensions）
  - 启用验证层（Validation Layers，仅 Debug 模式）
- **在项目中**：`VulkanContext::CreateInstance()`

```cpp
// 项目中的实现（VulkanContext.cpp）
void VulkanContext::CreateInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Tumbler Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "Tumbler";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    
    // 获取需要的扩展（GLFW + Debug）
    auto extensions = GetRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    
    // 启用验证层
    if (enableValidationLayers) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
        createInfo.ppEnabledLayerNames = validationLayers.data();
    }

    vkCreateInstance(&createInfo, nullptr, &Instance);
}
```

---

### 1.2 VkPhysicalDevice (物理设备)
- **作用**：真实的显卡实体（如 RTX 4090、Intel UHD 等）
- **职责**：
  - 查询显卡支持的特性（Features）
  - 查询显卡支持的扩展
  - 查询队列族（Queue Families）
  - 查询内存类型（Memory Types）
- **在项目中**：`VulkanContext::PickPhysicalDevice()`

**选择物理设备的评分机制**（项目中的实现）：
```cpp
// 优先选择独立显卡（Discrete GPU）
if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 1000;
}

// 显存越大越好
score += deviceProperties.limits.maxImageDimension2D;

// 必须支持图形队列和呈现队列
if (!queueFamilies.isComplete()) {
    score = 0;
}
```

---

### 1.3 VkDevice (逻辑设备)
- **作用**：你与这张显卡交互的具体句柄
- **职责**：
  - 大多数对象的创建和销毁依赖此接口
  - 开启特定的硬件特性（Features）
  - 开启特定的扩展（Extensions）
- **在项目中**：`VulkanContext::CreateLogicalDevice()`

```cpp
void VulkanContext::CreateLogicalDevice() {
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        GraphicsQueueFamilyIndex,
        PresentQueueFamilyIndex
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{};
    // 开启需要的特性，例如：
    // deviceFeatures.samplerAnisotropy = VK_TRUE;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.pEnabledFeatures = &deviceFeatures;

    vkCreateDevice(PhysicalDevice, &createInfo, nullptr, &Device);

    // 获取队列句柄
    vkGetDeviceQueue(Device, GraphicsQueueFamilyIndex, 0, &GraphicsQueue);
    vkGetDeviceQueue(Device, PresentQueueFamilyIndex, 0, &PresentQueue);
}
```

---

### 1.4 VkQueue (指令队列)
- **作用**：GPU 本身是个异步处理器，有 Graphics, Compute, Transfer 等不同类型的队列
- **队列族类型**：
  - `VK_QUEUE_GRAPHICS_BIT`：图形渲染（绘制三角形）
  - `VK_QUEUE_COMPUTE_BIT`：计算着色器
  - `VK_QUEUE_TRANSFER_BIT`：数据传输（拷贝 Buffer/Image）
  - `VK_QUEUE_PRESENT_BIT`：呈现到屏幕
- **在项目中**：
  - `GraphicsQueue`：提交绘制命令
  - `PresentQueue`：呈现图像

---


## 2. 表面 (VkSurfaceKHR)

### 2.1 什么是 VkSurfaceKHR？

**作用**：连接 Vulkan 与操作系统窗口系统的桥梁
- Vulkan 本身是平台无关的，不直接知道如何与窗口交互
- `VkSurfaceKHR` 封装了平台特定的窗口句柄
- 用于创建 Swapchain（交换链）

**项目中的扩展依赖**：
- `VK_KHR_surface`：核心表面扩展
- `VK_KHR_win32_surface`（Windows）、`VK_KHR_xlib_surface`（Linux X11）等平台特定扩展
- GLFW 会自动帮你处理这些平台差异

### 2.2 项目中的 Surface 创建

```cpp
// 在 VulkanContext::Init() 中通过 GLFW 创建
VkSurfaceKHR Surface;
if (glfwCreateWindowSurface(Instance, Window->GetGLFWWindow(), nullptr, &Surface) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create window surface!");
}
```

**GLFW 的便捷之处**：
- 自动检测平台
- 自动加载正确的平台扩展
- 一行代码搞定 Surface 创建

### 2.3 队列族与 Surface 的兼容性

**重要**：物理设备必须有队列族支持**图形渲染**和**呈现到 Surface**！

```cpp
// 项目中的队列族检查
struct QueueFamilyIndices {
    std::optional<uint32_t> GraphicsFamily;
    std::optional<uint32_t> PresentFamily;
    
    bool isComplete() {
        return GraphicsFamily.has_value() && PresentFamily.has_value();
    }
};

// 检查队列族是否支持呈现
VkBool32 presentSupport = false;
vkGetPhysicalDeviceSurfaceSupportKHR(
    PhysicalDevice,
    queueFamily,
    Surface,
    &presentSupport
);
```

---


## 9. 交换链 (Swapchain)

### 9.1 什么是 Swapchain？

**作用**：管理一系列可以呈现到屏幕的 Image（双缓冲/三缓冲）

**为什么需要**：
- 避免屏幕撕裂（Tearing）
- 实现垂直同步（VSync）

**双缓冲工作原理**：
1. **Front Buffer**：当前显示在屏幕上的图像
2. **Back Buffer**：正在绘制的图像
3. 绘制完后，**交换**（Swap）Front 和 Back

---

### 9.2 项目中的 Swapchain 实现

**核心组件**：
- `VkSwapchainKHR`：交换链对象
- `VkImage[]`：交换链中的图像（由 Vulkan 创建）
- `VkImageView[]`：图像视图（我们需要手动创建）
- `AllocatedImage`：深度图像

**创建流程**（在 `VulkanSwapchain::Init()`）：
```cpp
// 1. 选择表面格式
VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(...);
// 通常选择 VK_FORMAT_B8G8R8A8_SRGB

// 2. 选择呈现模式
VkPresentModeKHR presentMode = ChooseSwapPresentMode(...);
// VK_PRESENT_MODE_FIFO_KHR（垂直同步，最可靠）
// VK_PRESENT_MODE_MAILBOX_KHR（低延迟，无撕裂）
// VK_PRESENT_MODE_IMMEDIATE_KHR（无垂直同步，可能撕裂）

// 3. 选择交换范围
VkExtent2D extent = ChooseSwapExtent(...);
// 通常和窗口大小一致

// 4. 创建 Swapchain
VkSwapchainCreateInfoKHR createInfo{};
createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
createInfo.surface = Surface;
createInfo.minImageCount = imageCount;  // 双缓冲=2，三缓冲=3
createInfo.imageFormat = surfaceFormat.format;
createInfo.imageColorSpace = surfaceFormat.colorSpace;
createInfo.imageExtent = extent;
createInfo.imageArrayLayers = 1;
createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
createInfo.preTransform = capabilities.currentTransform;
createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
createInfo.presentMode = presentMode;
createInfo.clipped = VK_TRUE;

vkCreateSwapchainKHR(device, &createInfo, nullptr, &Swapchain);

// 5. 获取 Swapchain Images
vkGetSwapchainImagesKHR(device, Swapchain, &imageCount, nullptr);
Images.resize(imageCount);
vkGetSwapchainImagesKHR(device, Swapchain, &imageCount, Images.data());

// 6. 创建 Image Views
CreateImageViews();

// 7. 创建深度资源
CreateDepthResources();
```

---

### 9.3 窗口重置时重建 Swapchain

**项目中的实现**（在 `VulkanRenderer::RecreateSwapchain()`）：
```cpp
bool VulkanRenderer::RecreateSwapchain() {
    // 1. 等待 GPU 完成所有工作
    vkDeviceWaitIdle(Context.GetDevice());

    // 2. 清理旧的
    DestroyFramebuffers();
    SwapChain.Cleanup();

    // 3. 重新创建
    int width, height;
    glfwGetFramebufferSize(Window->GetGLFWWindow(), &width, &height);
    
    SwapChain.Init(&Context, width, height);
    InitFramebuffers();

    return true;
}
```

---


## 12. 验证层与调试工具 (Validation Layers)

### 12.1 什么是 Validation Layers？

**作用**：
- 检查 API 使用是否正确
- 提供详细的错误信息和警告
- 只在 Debug 模式下启用（Release 模式禁用，提高性能）

**项目中启用的验证层**：
```cpp
const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};
```

---

### 12.2 Debug Utils Messenger（调试信使）

**作用**：接收验证层的回调信息

**项目中的实现**（在 `VulkanContext`）：
```cpp
VKAPI_ATTR VkBool32 VKAPI_CALL VulkanContext::DebugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* pUserData)
{
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        LOG_ERROR("Validation Layer: {}", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

void VulkanContext::SetupDebugMessenger() {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = PopulateDebugMessengerCreateInfo();
    
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)
        vkGetInstanceProcAddr(Instance, "vkCreateDebugUtilsMessengerEXT");
    
    if (func != nullptr) {
        func(Instance, &createInfo, nullptr, &DebugMessenger);
    }
}
```

---

