# Vulkan 核心概念与工作流机制

与其说 Vulkan 是一个图形 API，不如说它是一个"跨平台的 GPU 控制台"。与 OpenGL 这种高度封装的、基于隐式状态机的古老 API 不同，Vulkan 没有隐式状态、没有全局上下文、更不替你管理内存。你必须极度显式地通过数百行代码声明每一个细节，换来的是极致的多线程能力和最小开销。

## 目录
1. [核心数据结构与金字塔](#1-核心数据结构与金字塔)
2. [表面 (VkSurfaceKHR)](#2-表面-vksurfacekhr)
3. [显存与资源的绝对把控 (VMA)](#3-显存与资源的绝对把控-vma)
4. [Shader 变量：Descriptors & Push Constants](#4-shader-变量descriptors--push-constants)
5. [渲染管线对象 (VkPipeline)](#5-渲染管线对象-vkpipeline)
6. [命令池与命令缓冲 (CommandPool & CommandBuffer)](#6-命令池与命令缓冲-commandpool--commandbuffer)
7. [同步原语 (Synchronization Primitives)](#7-同步原语-synchronization-primitives)
8. [渲染通道与帧缓冲 (RenderPass & Framebuffer)](#8-渲染通道与帧缓冲-renderpass--framebuffer)
9. [交换链 (Swapchain)](#9-交换链-swapchain)
10. [图像与采样器 (Image & Sampler)](#10-图像与采样器-image--sampler)
11. [着色器模块 (Shader Module)](#11-着色器模块-shader-module)
12. [验证层与调试工具 (Validation Layers)](#12-验证层与调试工具-validation-layers)
13. [数据传输与资源上传](#13-数据传输与资源上传)
14. [ImGui 集成](#14-imgui-集成)
15. [错误处理与工具函数](#15-错误处理与工具函数)
16. [渲染流程详解](#16-渲染流程详解)
17. [常见陷阱与解决方案](#17-常见陷阱与解决方案)

---

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

## 3. 显存与资源的绝对把控 (VMA)

在 OpenGL 中，你调用 `glGenBuffers`；在 Vulkan 中，这极其复杂：

### 3.1 原生 Vulkan 内存管理流程（不使用 VMA）

```cpp
// 步骤 1: 创建 Buffer（只产生了一个句柄，没有内存）
VkBufferCreateInfo bufferInfo{};
bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
bufferInfo.size = size;
bufferInfo.usage = usage;
bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

VkBuffer buffer;
vkCreateBuffer(device, &bufferInfo, nullptr, &buffer);

// 步骤 2: 查询这块 Buffer 需要多少内存结构与对齐字节数要求
VkMemoryRequirements memRequirements;
vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

// 步骤 3: 枚举物理显卡的内存堆，寻找合适的内存类型
VkPhysicalDeviceMemoryProperties memProperties;
vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

uint32_t memoryTypeIndex = 0;
for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
    if ((memRequirements.memoryTypeBits & (1 << i)) &&
        (memProperties.memoryTypes[i].propertyFlags & requiredProperties)) {
        memoryTypeIndex = i;
        break;
    }
}

// 步骤 4: 向系统申请真正的内存
VkMemoryAllocateInfo allocInfo{};
allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
allocInfo.allocationSize = memRequirements.size;
allocInfo.memoryTypeIndex = memoryTypeIndex;

VkDeviceMemory memory;
vkAllocateMemory(device, &allocInfo, nullptr, &memory);

// 步骤 5: 将句柄和这块生内存绑定
vkBindBufferMemory(device, buffer, memory, 0);
```

这还只是创建一个 Buffer！创建 Image 更复杂...

---

### 3.2 VMA (Vulkan Memory Allocator) - 项目中的实现

**VMA**：为了不被这套反人类逻辑搞死，业界（AMD 提供）标准是使用 VMA 库统一接管。

### 3.2.1 VMA 初始化

```cpp
// 在 VulkanContext::Init() 中
VmaAllocatorCreateInfo allocatorInfo{};
allocatorInfo.physicalDevice = PhysicalDevice;
allocatorInfo.device = Device;
allocatorInfo.instance = Instance;
allocatorInfo.flags = 0;

VmaAllocator Allocator;
vmaCreateAllocator(&allocatorInfo, &Allocator);
```

**项目中的 `AllocatedBuffer` 结构**：
```cpp
struct AllocatedBuffer {
    VkBuffer Buffer = VK_NULL_HANDLE;
    VmaAllocation Allocation = VK_NULL_HANDLE;
    VmaAllocationInfo Info{};
};
```

**项目中的 `CreateBuffer` 实现**（在 `RenderDevice` 中）：
```cpp
void RenderDevice::CreateBuffer(
    size_t size,
    VkBufferUsageFlags usage,
    VmaMemoryUsage memoryUsage,
    AllocatedBuffer& outBuffer)
{
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    vmaCreateBuffer(Allocator, &bufferInfo, &allocInfo,
        &outBuffer.Buffer, &outBuffer.Allocation, &outBuffer.Info);
}
```

**VMA 内存使用类型**（项目中常用）：
- `VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE`：GPU 专用内存（最快，CPU 不可直接访问）
- `VMA_MEMORY_USAGE_AUTO_PREFER_HOST`：CPU 可访问内存（用于上传数据）
- `VMA_MEMORY_USAGE_CPU_TO_GPU`：CPU → GPU 传输（ staging buffer）
- `VMA_MEMORY_USAGE_GPU_TO_CPU`：GPU → CPU 传输（读取回数据）

---

### 3.3 Staging Buffer（暂存缓冲）- 项目中的实现

**问题**：GPU 专用内存（Device Local）CPU 不能直接访问，怎么上传数据？

**解决方案**：使用 Staging Buffer（暂存缓冲）

```cpp
// 项目中的 ResourceUploadManager 实现
void ResourceUploadManager::UploadMeshData(...) {
    // 1. 创建 Staging Buffer（CPU 可写）
    AllocatedBuffer stagingBuffer;
    CreateBuffer(vertexSize,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU,
        stagingBuffer);

    // 2. 映射内存并拷贝数据
    void* data;
    vmaMapMemory(Allocator, stagingBuffer.Allocation, &data);
    memcpy(data, vertices, vertexSize);
    vmaUnmapMemory(Allocator, stagingBuffer.Allocation);

    // 3. 创建 GPU Buffer（GPU 专用）
    AllocatedBuffer vertexBuffer;
    CreateBuffer(vertexSize,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        vertexBuffer);

    // 4. 从 Staging Buffer 拷贝到 GPU Buffer
    CopyBuffer(stagingBuffer.Buffer, vertexBuffer.Buffer, vertexSize);

    // 5. 销毁 Staging Buffer（可选，也可以留着重用）
    DestroyBuffer(stagingBuffer);
}
```

---

## 4. Shader 变量：Descriptors & Push Constants

Vulkan 怎么把 C++ 的矩阵、贴图、参数传进 GPU 里的 Shader？

Vulkan 中不存在 `glGetUniformLocation`。必须高度显式！

---

### 3.1 Push Constants (推送常量)

**特点**：
- **极限高速**：它不像普通内存变量，它其实用的是显卡硬件命令流上极其宝贵的 **128/256 字节**的寄存器空间
- **更新极快**：不需要 Descriptor Set，直接在命令流里写入
- **空间极小**：通常只有 128 或 256 字节（取决于硬件）

**用途**：专供每一帧、每个物体都会频繁剧烈变动的心脏级别小数据。

**项目中的使用**：存放每个物体的 **Model 矩阵**

```glsl
// Vertex Shader 中的定义
layout(push_constant) uniform PushConstants {
    mat4 Model;
} push;
```

```cpp
// C++ 中的使用（在 RecordCommandBuffer 中）
vkCmdPushConstants(
    cmd,
    PipelineLayout,
    VK_SHADER_STAGE_VERTEX_BIT,
    0, sizeof(glm::mat4),
    &modelMatrix
);
```

---

### 3.2 Descriptor Sets (描述符集)

**什么是 Descriptor Set**？
- 对应 Shader 中类似 `layout(set = 0, binding = 1) uniform(...)` 的区域
- 也就是给"哪块内存/什么图片绑定到哪个槽位"写下一纸证明
- 需要三个东西：
  1. `DescriptorPool`：提供内存池
  2. `DescriptorSetLayout`：定义规范（"我要一个 UBO 在 binding 0，一个 Texture 在 binding 1"）
  3. `DescriptorSet`：实际的实例

---

#### 3.2.1 Descriptor Set Layout (描述符集布局)

**项目中的全局 Set Layout（Set 0）**：
```cpp
// 在 VulkanRenderer::InitDescriptors() 中
VkDescriptorSetLayoutBinding sceneBindings[] = {
    {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
    }
};

VkDescriptorSetLayoutCreateInfo layoutInfo{};
layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
layoutInfo.bindingCount = 1;
layoutInfo.pBindings = sceneBindings;

vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &GlobalSetLayout);
```

**项目中的材质 Set Layout（Set 1）**：
```cpp
// 在 FMaterial::BuildPipeline() 中
VkDescriptorSetLayoutBinding baseColorBinding{};
baseColorBinding.binding = 0;
baseColorBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
baseColorBinding.descriptorCount = 1;
baseColorBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

VkDescriptorSetLayoutBinding normalMapBinding{};
normalMapBinding.binding = 1;
normalMapBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
normalMapBinding.descriptorCount = 1;
normalMapBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

VkDescriptorSetLayoutBinding uboBinding{};
uboBinding.binding = 2;
uboBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
uboBinding.descriptorCount = 1;
uboBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

// ... 还有 IBL 的 Binding 3, 4, 5

VkDescriptorSetLayoutBinding bindings[] = {
    baseColorBinding, normalMapBinding, uboBinding,
    irradianceMapBinding, prefilterMapBinding, brdfLUTBinding
};
```

---

#### 3.2.2 Descriptor Pool (描述符池)

**作用**：分配 Descriptor Set 的内存池

**项目中的实现**：
```cpp
// 在 VulkanRenderer::InitDescriptors() 中
VkDescriptorPoolSize poolSizes[] = {
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
    { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 }
};

VkDescriptorPoolCreateInfo poolInfo{};
poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
poolInfo.poolSizeCount = 2;
poolInfo.pPoolSizes = poolSizes;
poolInfo.maxSets = 1000;  // 最多分配 1000 个 Set

vkCreateDescriptorPool(device, &poolInfo, nullptr, &DescriptorPool);
```

---

#### 3.2.3 Descriptor Set (描述符集) - 分配与更新

**分配 Descriptor Set**：
```cpp
// 在 VulkanRenderer::AllocateDescriptorSet() 中
VkDescriptorSetAllocateInfo allocInfo{};
allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
allocInfo.descriptorPool = DescriptorPool;
allocInfo.descriptorSetCount = 1;
allocInfo.pSetLayouts = &layout;

VkDescriptorSet descriptorSet;
vkAllocateDescriptorSets(device, &allocInfo, &descriptorSet);
```

**更新 Descriptor Set**（项目中 `FMaterialInstance::ApplyChanges()`）：
```cpp
// 1. 准备 Buffer Info（UBO）
VkDescriptorBufferInfo bufferInfo{};
bufferInfo.buffer = UBOBuffer.Buffer;
bufferInfo.offset = 0;
bufferInfo.range = sizeof(FMaterialUBO);

// 2. 准备 Image Info（Texture）
VkDescriptorImageInfo imageInfo{};
imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
imageInfo.imageView = texture->GetImageView();
imageInfo.sampler = texture->GetSampler();

// 3. 准备 Write 结构
VkWriteDescriptorSet write{};
write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
write.dstSet = DescriptorSet;
write.dstBinding = bindingIndex;
write.dstArrayElement = 0;
write.descriptorType = type;
write.descriptorCount = 1;
write.pBufferInfo = &bufferInfo;  // 或者写 pImageInfo

// 4. 提交更新
vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
```

---

#### 3.2.4 优化设计：按频率分组

**Vulkan 鼓励按频率将资源捆绑**：

| Set 编号 | 内容 | 更新频率 | 项目中的用途 |
|---------|------|---------|-------------|
| **Set 0** | 全局 Set | **每帧 1 次** | Camera 矩阵、光源数据 `SceneDataUBO` |
| **Set 1** | 材质 Set | **每材质 1 次** | Albedo Texture、Normal Map、PBR 参数 |
| Push Constant | 模型矩阵 | **每物体 1 次** | Model 矩阵 |

**项目中的绑定顺序**（在 `RecordCommandBuffer` 中）：
```cpp
// 1. 绑定全局 Set（每帧一次）
vkCmdBindDescriptorSets(
    cmd,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    0, 1, &GlobalDescriptorSet,
    0, nullptr
);

// 2. 绑定材质 Set（每材质一次）
vkCmdBindDescriptorSets(
    cmd,
    VK_PIPELINE_BIND_POINT_GRAPHICS,
    pipelineLayout,
    1, 1, &materialDescriptorSet,
    0, nullptr
);

// 3. Push Constant（每物体一次）
vkCmdPushConstants(cmd, ...);
```

---

## 5. 渲染管线对象 (VkPipeline)

Vulkan 将 Shader 代码、图元拓扑方式（三角形）、深度测试开关、混合开关、多重采样参数、颜色格式等一系列状态在启动时就**完全烘焙死 (Bake)** 成了不可变的全局常量状态汇总：`VkPipeline`。

---

### 4.1 为什么 Pipeline 是不可变的？

**原因**：
- 显卡驱动可以在游戏主线加载时对它进行**终极硬件优化**
- 运行时切换 Pipeline 的开销很小（只是切换状态）
- 但创建 Pipeline 很慢！（需要编译、优化）

**在 OpenGL 中**：
```cpp
glDisable(GL_DEPTH_TEST);  // 随便改，立即生效
```

**在 Vulkan 中**：
- 你要改深度测试？很抱歉，请重新走流程创建一条新的完全烘焙完毕的 `VkPipeline` 并在运行时切换过去。

---

### 4.2 项目中的 Pipeline 构建 - VulkanPipelineBuilder

Vulkan 的管线创建非常繁琐，需要填充大量的结构体。项目使用 **Builder 模式**封装了这些配置过程。

**项目中的使用示例**（在 `FMaterial::BuildPipeline()`）：
```cpp
auto builder = VulkanPipelineBuilder::Begin(PipelineLayout);

Pipeline = builder
    .SetShaders(vertModule, fragModule)
    .SetViewport(SwapchainExtent.width, SwapchainExtent.height)
    .SetInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
    .SetPolygonMode(VK_POLYGON_MODE_FILL)
    .SetCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE)
    .SetMultisamplingNone()
    .EnableDepthTest(VK_TRUE, VK_COMPARE_OP_LESS)
    .SetColorBlending(true)
    .Build(device, RenderPass);
```

---

### 4.3 Pipeline 的各个阶段详解

#### 4.3.1 Shader Stages（着色器阶段）
```cpp
VkPipelineShaderStageCreateInfo vertStage{};
vertStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
vertStage.stage = VK_SHADER_STAGE_VERTEX_BIT;
vertStage.module = vertexShader;
vertStage.pName = "main";  // Shader 入口函数名

VkPipelineShaderStageCreateInfo fragStage{};
fragStage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
fragStage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
fragStage.module = fragmentShader;
fragStage.pName = "main";
```

#### 4.3.2 Vertex Input（顶点输入）
```cpp
// 绑定描述（一个 Binding 对应一个顶点流）
VkVertexInputBindingDescription bindingDescription{};
bindingDescription.binding = 0;
bindingDescription.stride = 11 * sizeof(float);  // Position(3) + Normal(3) + Tangent(3) + UV(2)
bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

// 属性描述（每个属性对应 Shader 中的一个 location）
std::vector<VkVertexInputAttributeDescription> attributeDescriptions = {
    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},                    // Position
    {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)},    // Normal
    {2, 0, VK_FORMAT_R32G32B32_SFLOAT, 6 * sizeof(float)},    // Tangent
    {3, 0, VK_FORMAT_R32G32_SFLOAT, 9 * sizeof(float)}         // UV
};
```

#### 4.3.3 Input Assembly（输入装配）
```cpp
VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;  // 三角形列表
inputAssembly.primitiveRestartEnable = VK_FALSE;
```

**拓扑类型**：
- `VK_PRIMITIVE_TOPOLOGY_POINT_LIST`：点
- `VK_PRIMITIVE_TOPOLOGY_LINE_LIST`：线段
- `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST`：三角形（最常用）

#### 4.3.4 Viewport & Scissor（视口与裁剪）
```cpp
VkViewport viewport{};
viewport.x = 0.0f;
viewport.y = 0.0f;
viewport.width = (float)width;
viewport.height = (float)height;
viewport.minDepth = 0.0f;
viewport.maxDepth = 1.0f;

VkRect2D scissor{};
scissor.offset = {0, 0};
scissor.extent = {width, height};
```

**Dynamic State（动态状态）**：
- 如果设为动态，就可以在录制命令缓冲时更改，不用重建 Pipeline
- 项目中用于窗口大小变化时，不用重建 Pipeline

```cpp
// 使用动态视口和裁剪
std::vector<VkDynamicState> dynamicStates = {
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_SCISSOR
};
```

#### 4.3.5 Rasterization（光栅化）
```cpp
VkPipelineRasterizationStateCreateInfo rasterizer{};
rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
rasterizer.depthClampEnable = VK_FALSE;
rasterizer.rasterizerDiscardEnable = VK_FALSE;
rasterizer.polygonMode = VK_POLYGON_MODE_FILL;  // 填充模式
rasterizer.lineWidth = 1.0f;
rasterizer.cullMode = VK_CULL_MODE_NONE;  // 不剔除
rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;  // 顺时针为正面
rasterizer.depthBiasEnable = VK_FALSE;
```

**Polygon Mode**：
- `VK_POLYGON_MODE_FILL`：填充（默认）
- `VK_POLYGON_MODE_LINE`：线框模式
- `VK_POLYGON_MODE_POINT`：点模式

**Cull Mode**：
- `VK_CULL_MODE_NONE`：不剔除
- `VK_CULL_MODE_FRONT_BIT`：剔除正面
- `VK_CULL_MODE_BACK_BIT`：剔除背面（最常用）

#### 4.3.6 Multisampling（多重采样 - 抗锯齿）
```cpp
VkPipelineMultisampleStateCreateInfo multisampling{};
multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
multisampling.sampleShadingEnable = VK_FALSE;
multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;  // 1x 采样（无抗锯齿）
```

**采样数**：
- `VK_SAMPLE_COUNT_1_BIT`：无抗锯齿
- `VK_SAMPLE_COUNT_2_BIT`：2x MSAA
- `VK_SAMPLE_COUNT_4_BIT`：4x MSAA
- `VK_SAMPLE_COUNT_8_BIT`：8x MSAA

#### 4.3.7 Depth & Stencil（深度与模板测试）
```cpp
VkPipelineDepthStencilStateCreateInfo depthStencil{};
depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
depthStencil.depthTestEnable = VK_TRUE;           // 启用深度测试
depthStencil.depthWriteEnable = VK_TRUE;          // 启用深度写入
depthStencil.depthCompareOp = VK_COMPARE_OP_LESS;  // 深度小的通过（近的物体覆盖远的）
depthStencil.depthBoundsTestEnable = VK_FALSE;
depthStencil.stencilTestEnable = VK_FALSE;
```

**深度比较操作**：
- `VK_COMPARE_OP_LESS`：小于（常用，近的覆盖远的）
- `VK_COMPARE_OP_LESS_OR_EQUAL`：小于等于
- `VK_COMPARE_OP_GREATER`：大于
- `VK_COMPARE_OP_ALWAYS`：总是通过（禁用深度测试）

#### 4.3.8 Color Blending（颜色混合）
```cpp
VkPipelineColorBlendAttachmentState colorBlendAttachment{};
colorBlendAttachment.colorWriteMask = 
    VK_COLOR_COMPONENT_R_BIT | 
    VK_COLOR_COMPONENT_G_BIT | 
    VK_COLOR_COMPONENT_B_BIT | 
    VK_COLOR_COMPONENT_A_BIT;

if (enableBlend) {
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
} else {
    colorBlendAttachment.blendEnable = VK_FALSE;
}
```

**Alpha 混合公式**（开启混合时）：
```
最终颜色 = 源颜色 * 源 Alpha + 目标颜色 * (1 - 源 Alpha)
```

#### 4.3.9 Pipeline Layout（管线布局）
```cpp
VkPushConstantRange pushConstantRange{};
pushConstantRange.offset = 0;
pushConstantRange.size = sizeof(glm::mat4);
pushConstantRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

VkDescriptorSetLayout layouts[] = {GlobalSetLayout, DescriptorSetLayout};

VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
pipelineLayoutInfo.pushConstantRangeCount = 1;
pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;
pipelineLayoutInfo.setLayoutCount = 2;
pipelineLayoutInfo.pSetLayouts = layouts;

vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &PipelineLayout);
```

---

## 6. 命令池与命令缓冲 (CommandPool & CommandBuffer)

Vulkan 要求一切指令在 **CommandBuffer (命令缓冲)** 里被录制后，作为一个大包裹提交。

---

### 5.1 CommandPool（命令池）

**作用**：分配 CommandBuffer 的内存池

**项目中的实现**：
```cpp
// 在 CommandBufferManager 中
VkCommandPoolCreateInfo poolInfo{};
poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
poolInfo.queueFamilyIndex = graphicsQueueFamily;
poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;  // 允许重置

vkCreateCommandPool(device, &poolInfo, nullptr, &CommandPool);
```

**重要 Flag**：
- `VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT`：允许单独重置每个 CommandBuffer
- `VK_COMMAND_POOL_CREATE_TRANSIENT_BIT`：CommandBuffer 是短暂的，经常被重置

---

### 5.2 CommandBuffer（命令缓冲）

**项目中的录制流程**（在 `VulkanRenderer::RecordCommandBuffer()`）：

```cpp
// 1. 开始录制
VkCommandBufferBeginInfo beginInfo{};
beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
beginInfo.flags = 0;  // 可选：VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT

vkBeginCommandBuffer(cmd, &beginInfo);

// 2. 开始 Render Pass
VkRenderPassBeginInfo renderPassInfo{};
renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
renderPassInfo.renderPass = RenderPass;
renderPassInfo.framebuffer = Framebuffers[imageIndex];
renderPassInfo.renderArea.offset = {0, 0};
renderPassInfo.renderArea.extent = SwapchainExtent;

VkClearValue clearValues[2];
clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};  // 黑色背景
clearValues[1].depthStencil = {1.0f, 0};                // 深度 1.0

renderPassInfo.clearValueCount = 2;
renderPassInfo.pClearValues = clearValues;

vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

// 3. 绑定 Pipeline
vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline);

// 4. 绑定 Descriptor Sets
vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, ...);

// 5. Push Constants
vkCmdPushConstants(cmd, ...);

// 6. 绑定 Vertex/Index Buffer
vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &offset);
vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

// 7. 绘制！
vkCmdDrawIndexed(cmd, indexCount, 1, 0, 0, 0);

// 8. 结束 Render Pass
vkCmdEndRenderPass(cmd);

// 9. 结束录制
vkEndCommandBuffer(cmd);
```

---

### 5.3 命令缓冲区重用模式

**重要**：命令缓冲区不应每帧分配/释放，而应重用！

```cpp
// ❌ 错误做法：每帧分配/释放
VkCommandBuffer cmd = AllocateCommandBuffer();
RecordAndSubmit(cmd);
FreeCommandBuffer(cmd);  // 错误！GPU 可能还在使用

// ✅ 正确做法：重用命令缓冲区
VkCommandBuffer mainCmdBuffer = AllocateCommandBuffer();  // 初始化时分配

// 每帧渲染
vkWaitForFences(..., renderFence, ...);  // 等待 GPU 完成
vkResetCommandBuffer(mainCmdBuffer, 0);  // 重置内容
RecordAndSubmit(mainCmdBuffer);
// 不释放，下一帧继续使用
```

**原因**：
- GPU 执行是异步的，提交后立即释放会导致 "command buffer in use" 验证错误
- 每帧分配/释放有性能开销
- 使用 Fence 确保命令缓冲区不再被 GPU 使用后再重置

---

## 7. 同步原语 (Synchronization Primitives)

Vulkan 提供三种同步机制：

| 类型 | 作用域 | 用途 |
|------|--------|------|
| **Fence** | CPU-GPU | 等待 GPU 完成工作 |
| **Semaphore** | GPU-GPU | 队列间/操作间同步 |
| **Barrier** | 命令缓冲内 | 资源状态转换 |

---

### 6.1 Fence（围栏）- CPU ↔ GPU 同步

**作用**：让 CPU 等待 GPU 完成某项工作

**项目中的使用**：
```cpp
// 创建 Fence（初始状态：未信号）
VkFenceCreateInfo fenceInfo{};
fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
fenceInfo.flags = 0;  // 或者 VK_FENCE_CREATE_SIGNALED_BIT

vkCreateFence(device, &fenceInfo, nullptr, &RenderFence);

// 提交命令时，传入 Fence
vkQueueSubmit(GraphicsQueue, 1, &submitInfo, RenderFence);

// CPU 等待 GPU 完成
vkWaitForFences(device, 1, &RenderFence, VK_TRUE, UINT64_MAX);

// 重置 Fence（可以再次使用）
vkResetFences(device, 1, &RenderFence);
```

---

### 6.2 Semaphore（信号量）- GPU ↔ GPU 同步

**作用**：让 GPU 的一个操作等待另一个操作完成

**项目中的使用**（在 `VulkanRenderer::Render()`）：
```cpp
// 创建 Semaphore
VkSemaphoreCreateInfo semaphoreInfo{};
semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

vkCreateSemaphore(device, &semaphoreInfo, nullptr, &ImageAvailableSemaphore);
vkCreateSemaphore(device, &semaphoreInfo, nullptr, &RenderFinishedSemaphore);

// 1. 获取下一张交换链图像（发出 imageAvailableSemaphore 信号）
uint32_t imageIndex;
vkAcquireNextImageKHR(
    device,
    Swapchain,
    UINT64_MAX,
    ImageAvailableSemaphore,  // 图像可用时发出信号
    VK_NULL_HANDLE,
    &imageIndex
);

// 2. 提交命令缓冲（等待 imageAvailableSemaphore，发出 renderFinishedSemaphore）
VkSubmitInfo submitInfo{};
submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

VkSemaphore waitSemaphores[] = {ImageAvailableSemaphore};
VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
submitInfo.waitSemaphoreCount = 1;
submitInfo.pWaitSemaphores = waitSemaphores;
submitInfo.pWaitDstStageMask = waitStages;

submitInfo.commandBufferCount = 1;
submitInfo.pCommandBuffers = &MainCommandBuffer;

VkSemaphore signalSemaphores[] = {RenderFinishedSemaphore};
submitInfo.signalSemaphoreCount = 1;
submitInfo.pSignalSemaphores = signalSemaphores;

vkQueueSubmit(GraphicsQueue, 1, &submitInfo, RenderFence);

// 3. 呈现图像（等待 renderFinishedSemaphore）
VkPresentInfoKHR presentInfo{};
presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

presentInfo.waitSemaphoreCount = 1;
presentInfo.pWaitSemaphores = signalSemaphores;

presentInfo.swapchainCount = 1;
presentInfo.pSwapchains = &Swapchain;
presentInfo.pImageIndices = &imageIndex;

vkQueuePresentKHR(PresentQueue, &presentInfo);
```

**时序图**：
```
CPU:  AcquireNextImage ──→ 录制命令 ──→ Submit ──→ Present
        │                     │             │          │
        │                     │             │          │
GPU:    ○ ImageAvailable      ○ 绘制       ○ RenderFinished
       (Semaphore)                       (Semaphore)
```

---

### 6.3 Pipeline Barrier（管线屏障）- 命令缓冲内同步

**作用**：
- 资源状态转换（例如：从"传输目标"到"着色器读取"）
- 内存访问同步（确保写入完成后再读取）

**项目中的使用**（在 `ResourceUploadManager` 中上传纹理时）：
```cpp
// 1. 转换 Image 布局：undefined → transfer_dst
VkImageMemoryBarrier barrier{};
barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
barrier.image = image;
barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
barrier.subresourceRange.baseMipLevel = 0;
barrier.subresourceRange.levelCount = 1;
barrier.subresourceRange.baseArrayLayer = 0;
barrier.subresourceRange.layerCount = 1;
barrier.srcAccessMask = 0;
barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

vkCmdPipelineBarrier(
    cmd,
    VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    0, 0, nullptr, 0, nullptr, 1, &barrier
);

// 2. 拷贝数据到 Image（省略）

// 3. 转换 Image 布局：transfer_dst → shader_read_only_optimal
barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

vkCmdPipelineBarrier(
    cmd,
    VK_PIPELINE_STAGE_TRANSFER_BIT,
    VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    0, 0, nullptr, 0, nullptr, 1, &barrier
);
```

**常见 Image Layout**：
- `VK_IMAGE_LAYOUT_UNDEFINED`：初始状态，什么都不保证
- `VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL`：作为传输目标（拷贝数据进来）
- `VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL`：作为传输源（拷贝数据出去）
- `VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL`：着色器只读（采样纹理）
- `VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL`：颜色附件（渲染到）
- `VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL`：深度/模板附件
- `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`：呈现源（显示到屏幕）

---

## 8. 渲染通道与帧缓冲 (RenderPass & Framebuffer)

### 8.1 RenderPass（渲染通道）

**作用**：描述渲染操作的结构（有哪些附件、哪些子通道、附件如何转换）

**项目中的 RenderPass 创建**：
```cpp
void VulkanRenderer::InitRenderPass() {
    // 1. 颜色附件描述
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = SwapChain.GetImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;    // 加载时清除
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;    // 存储时保存
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    // 2. 深度附件描述
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = SwapChain.GetDepthFormat();
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 3. 颜色附件引用
    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // 4. 深度附件引用
    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // 5. 子通道描述
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // 6. 子通道依赖
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // 7. 创建 RenderPass
    VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};
    
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 2;
    renderPassInfo.pAttachments = attachments;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    vkCreateRenderPass(Context.GetDevice(), &renderPassInfo, nullptr, &RenderPass);
}
```

**LoadOp / StoreOp**：
- `VK_ATTACHMENT_LOAD_OP_LOAD`：加载之前的内容
- `VK_ATTACHMENT_LOAD_OP_CLEAR`：清除为某个值
- `VK_ATTACHMENT_LOAD_OP_DONT_CARE`：不关心，内容是未定义的

- `VK_ATTACHMENT_STORE_OP_STORE`：保存内容
- `VK_ATTACHMENT_STORE_OP_DONT_CARE`：不保存

---

### 8.2 Framebuffer（帧缓冲）

**作用**：把具体的 Image View 绑定到 RenderPass 的附件槽位

**项目中的 Framebuffer 创建**：
```cpp
void VulkanRenderer::InitFramebuffers() {
    const auto& imageViews = SwapChain.GetImageViews();
    Framebuffers.resize(imageViews.size());

    for (size_t i = 0; i < imageViews.size(); i++) {
        VkImageView attachments[] = {
            imageViews[i],                    // 颜色附件
            SwapChain.GetDepthImageView()     // 深度附件
        };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = RenderPass;
        framebufferInfo.attachmentCount = 2;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = SwapChain.GetExtent().width;
        framebufferInfo.height = SwapChain.GetExtent().height;
        framebufferInfo.layers = 1;

        vkCreateFramebuffer(
            Context.GetDevice(),
            &framebufferInfo,
            nullptr,
            &Framebuffers[i]
        );
    }
}
```

**关系图**：
```
RenderPass (描述结构)
    ↓
Framebuffer (绑定具体资源)
    ├── Color Image View
    └── Depth Image View
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

## 10. 图像与采样器 (Image & Sampler)

### 10.1 VkImage（图像）

**项目中的 `AllocatedImage` 结构**：
```cpp
struct AllocatedImage {
    VkImage Image = VK_NULL_HANDLE;
    VkImageView ImageView = VK_NULL_HANDLE;
    VmaAllocation Allocation = VK_NULL_HANDLE;
};
```

**创建 Image**（在 `RenderDevice` 中）：
```cpp
void RenderDevice::CreateImage(
    uint32_t width, uint32_t height,
    VkFormat format,
    VkImageTiling tiling,
    VkImageUsageFlags usage,
    VmaMemoryUsage memoryUsage,
    AllocatedImage& outImage)
{
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = memoryUsage;

    vmaCreateImage(Allocator, &imageInfo, &allocInfo,
        &outImage.Image, &outImage.Allocation, nullptr);
}
```

---

### 10.2 VkImageView（图像视图）

**作用**：描述如何访问 Image 的一部分（例如：作为颜色附件、作为采样纹理）

**创建 ImageView**：
```cpp
VkImageViewCreateInfo viewInfo{};
viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
viewInfo.image = image;
viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
viewInfo.format = format;
viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
viewInfo.subresourceRange.baseMipLevel = 0;
viewInfo.subresourceRange.levelCount = 1;
viewInfo.subresourceRange.baseArrayLayer = 0;
viewInfo.subresourceRange.layerCount = 1;

vkCreateImageView(device, &viewInfo, nullptr, &outImage.ImageView);
```

---

### 10.3 VkSampler（采样器）

**作用**：描述如何采样纹理（过滤、寻址、各向异性等）

**项目中的 Sampler 创建**（在 `FTexture` 中）：
```cpp
VkSamplerCreateInfo samplerInfo{};
samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
samplerInfo.magFilter = VK_FILTER_LINEAR;          // 放大：线性过滤
samplerInfo.minFilter = VK_FILTER_LINEAR;          // 缩小：线性过滤
samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // U 方向：重复
samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // V 方向：重复
samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;  // W 方向：重复
samplerInfo.anisotropyEnable = VK_TRUE;            // 启用各向异性过滤
samplerInfo.maxAnisotropy = 16.0f;                // 最大各向异性级别
samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
samplerInfo.unnormalizedCoordinates = VK_FALSE;
samplerInfo.compareEnable = VK_FALSE;
samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

vkCreateSampler(device, &samplerInfo, nullptr, &Sampler);
```

**过滤模式**：
- `VK_FILTER_NEAREST`：最近邻（像素风）
- `VK_FILTER_LINEAR`：线性（平滑）

**寻址模式**：
- `VK_SAMPLER_ADDRESS_MODE_REPEAT`：重复（平铺）
- `VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT`：镜像重复
- `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE`：钳制到边缘
- `VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER`：钳制到边框颜色

---

## 11. 着色器模块 (Shader Module)

### 11.1 编译 Shader

Vulkan 使用 **SPIR-V** 二进制格式，不是 GLSL 文本！

**项目中的 Shader 编译**（使用 glslangValidator）：
```python
# CMakeLists.txt 中的 Shader 编译
find_program(GLSLANG_VALIDATOR glslangValidator)
if(GLSLANG_VALIDATOR)
    add_custom_command(
        OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/pbr.vert.spv
        COMMAND ${GLSLANG_VALIDATOR} -V ${CMAKE_CURRENT_SOURCE_DIR}/pbr.vert -o ${CMAKE_CURRENT_BINARY_DIR}/pbr.vert.spv
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/pbr.vert
    )
endif()
```

或者手动编译：
```bash
glslangValidator -V shader.vert -o shader.vert.spv
glslangValidator -V shader.frag -o shader.frag.spv
```

---

### 11.2 创建 Shader Module

**项目中的实现**（在 `RenderDevice` 中）：
```cpp
bool RenderDevice::LoadShaderModule(const char* filePath, VkShaderModule* outShaderModule) {
    std::ifstream file(filePath, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    size_t fileSize = (size_t)file.tellg();
    std::vector<uint32_t> buffer(fileSize / sizeof(uint32_t));

    file.seekg(0);
    file.read((char*)buffer.data(), fileSize);
    file.close();

    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = buffer.size() * sizeof(uint32_t);
    createInfo.pCode = buffer.data();

    if (vkCreateShaderModule(Device, &createInfo, nullptr, outShaderModule) != VK_SUCCESS) {
        return false;
    }

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

## 13. 数据传输与资源上传

### 13.1 Buffer 到 Buffer 拷贝 (vkCmdCopyBuffer)

**作用**：在两个 Buffer 之间复制数据，常用于 Staging Buffer → GPU Buffer

**项目中的使用**（在 ResourceUploadManager 中）：
```cpp
// 立即提交命令的 lambda
CommandBufferManagerRef->ImmediateSubmit([&](VkCommandBuffer cmd) {
    VkBufferCopy copyRegion{};
    copyRegion.size = vSize;  // 要复制的字节数
    copyRegion.srcOffset = 0;  // 源偏移
    copyRegion.dstOffset = 0;  // 目标偏移
    
    // 执行拷贝
    vkCmdCopyBuffer(cmd, vStaging.Buffer, gpuMesh.VertexBuffer.Buffer, 1, &copyRegion);
});
```

**关键参数**：
- `srcBuffer`：源 Buffer（Staging Buffer）
- `dstBuffer`：目标 Buffer（GPU Buffer）
- `regionCount`：拷贝区域数量
- `pRegions`：拷贝区域数组

---

### 13.2 Buffer 到 Image 拷贝 (vkCmdCopyBufferToImage)

**作用**：将 Buffer 数据复制到 Image，用于纹理上传

**项目中的使用**（在 CommandBufferManager 中）：
```cpp
void CommandBufferManager::CopyBufferToImage(
    VkBuffer buffer,
    VkImage image,
    uint32_t width,
    uint32_t height)
{
    ImmediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {width, height, 1};
        
        vkCmdCopyBufferToImage(
            cmd,
            buffer,
            image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            1,
            &region
        );
    });
}
```

---

### 13.3 立即提交模式 (Immediate Submit)

**什么是立即提交**：临时分配一个 CommandBuffer，录制命令，立即提交并等待完成

**项目中的实现**（在 CommandBufferManager 中）：
```cpp
template<typename F>
void CommandBufferManager::ImmediateSubmit(F&& func) {
    // 1. 分配一次性 CommandBuffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = CommandPool;
    allocInfo.commandBufferCount = 1;
    
    VkCommandBuffer cmd;
    vkAllocateCommandBuffers(Device, &allocInfo, &cmd);
    
    // 2. 开始录制
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(cmd, &beginInfo);
    
    // 3. 执行用户传入的 lambda
    func(cmd);
    
    // 4. 结束录制
    vkEndCommandBuffer(cmd);
    
    // 5. 提交并等待完成
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &cmd;
    
    vkQueueSubmit(GraphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(GraphicsQueue);  // 等待完成
    
    // 6. 释放 CommandBuffer
    vkFreeCommandBuffers(Device, CommandPool, 1, &cmd);
}
```

**适用场景**：
- 初始化时上传资源（纹理、网格）
- 一次性数据传输
- 资源布局转换

---

## 14. ImGui 集成

### 14.1 ImGui 与 Vulkan 的集成

**ImGui**：一个轻量级的即时模式 GUI 库，用于调试和工具界面

**项目中的集成步骤**（在 UIManager 中）：

```cpp
void UIManager::Init(AppWindow* window, VulkanRenderer* renderer) {
    VkDevice device = renderer->GetDevice();
    
    // 1. 创建 ImGui 专属的 Descriptor Pool
    std::array<VkDescriptorPoolSize, 11> pool_sizes = {
        {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
         {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
         {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
         {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
         {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
         {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}}
    };
    
    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = 1000;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &ImGuiPool));
    
    // 2. 初始化 ImGui 上下文
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    
    // 3. 初始化 GLFW 和 Vulkan 后端
    ImGui_ImplGlfw_InitForVulkan(window->GetNativeWindow(), true);
    
    ImGui_ImplVulkan_InitInfo init_info{};
    init_info.Instance = renderer->GetContext().GetInstance();
    init_info.PhysicalDevice = renderer->GetContext().GetPhysicalDevice();
    init_info.Device = device;
    init_info.QueueFamily = renderer->GetContext().GetGraphicsQueueFamily();
    init_info.Queue = renderer->GetContext().GetGraphicsQueue();
    init_info.DescriptorPool = ImGuiPool;
    init_info.RenderPass = renderer->GetRenderPass();
    init_info.MinImageCount = renderer->GetSwapchainImageCount();
    init_info.ImageCount = renderer->GetSwapchainImageCount();
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    
    ImGui_ImplVulkan_Init(&init_info);
    
    // 4. 上传字体纹理
    ImGui_ImplVulkan_CreateFontsTexture();
}
```

---

### 14.2 ImGui 渲染流程

```cpp
// 每帧调用
void UIManager::BeginFrame() {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void UIManager::EndFrame() {
    ImGui::Render();
}

// 在 CommandBuffer 中录制
void UIManager::RecordDrawCommands(VkCommandBuffer cmdBuffer) {
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmdBuffer);
}
```

**在主渲染循环中的调用顺序**：
```cpp
// 1. 开始 ImGui 帧
uiManager.BeginFrame();

// 2. 绘制 ImGui 控件
ImGui::Begin("Debug Window");
ImGui::Text("FPS: %.1f", fps);
ImGui::End();

// 3. 结束 ImGui 帧
uiManager.EndFrame();

// 4. 在录制 CommandBuffer 时调用
uiManager.RecordDrawCommands(cmdBuffer);
```

---

## 15. 错误处理与工具函数

### 15.1 VK_CHECK 宏

**项目中的错误检查机制**：
```cpp
#define VK_CHECK(x)                                                                 \
    do {                                                                            \
        VkResult err = x;                                                           \
        if (err) {                                                                  \
            LOG_CRITICAL("[Vulkan Error] Detected: {} ({})\n \tFile: {}\n \tLine: {}", \
                         VkUtils::VkResultToString(err), (int)err, __FILE__, __LINE__);      \
            abort();                                                                \
        }                                                                           \
    } while (0)
```

**使用示例**：
```cpp
VK_CHECK(vkCreateInstance(&createInfo, nullptr, &Instance));
VK_CHECK(vkCreateDevice(PhysicalDevice, &createInfo, nullptr, &Device));
```

---

### 15.2 VkResult 转字符串

```cpp
inline const char* VkUtils::VkResultToString(const VkResult result) {
    switch (result) {
        // 成功状态
        case VK_SUCCESS: return "VK_SUCCESS";
        case VK_NOT_READY: return "VK_NOT_READY";
        case VK_TIMEOUT: return "VK_TIMEOUT";
        
        // 常见错误
        case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
        case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
        case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
        case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
        case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
        case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
        case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
        case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
        // ... 更多
        default: return "UNKNOWN_ERROR";
    }
}
```

---

### 15.3 通用的 "Count then Data" 模式

Vulkan 很多 API 使用"先查询数量，再获取数据"的模式：

```cpp
template <typename T, typename Func, typename... Args>
std::vector<T> VkUtils::GetVector(Func func, Args... args) {
    uint32_t count = 0;
    std::vector<T> result;
    
    // 第一次调用：获取数量
    func(args..., &count, nullptr);
    
    if (count == 0) return {};
    result.resize(count);
    
    // 第二次调用：获取数据
    func(args..., &count, result.data());
    
    return result;
}
```

**使用示例**：
```cpp
// 获取物理设备列表
auto physicalDevices = VkUtils::GetVector<VkPhysicalDevice>(
    vkEnumeratePhysicalDevices, Instance
);

// 获取队列族属性
auto queueFamilies = VkUtils::GetVector<VkQueueFamilyProperties>(
    vkGetPhysicalDeviceQueueFamilyProperties, PhysicalDevice
);
```

---

### 15.4 格式与颜色空间工具

```cpp
// VkFormat 转字符串
const char* VkUtils::VkFormatToString(VkFormat format);

// VkColorSpaceKHR 转字符串
const char* VkUtils::VkColorSpaceToString(VkColorSpaceKHR color_space);

// VkPresentModeKHR 转字符串
const char* VkUtils::VkPresentModeToString(VkPresentModeKHR present_mode);
```

**常见格式**：
- `VK_FORMAT_B8G8R8A8_SRGB`：交换链首选格式
- `VK_FORMAT_R8G8B8A8_SRGB`：纹理格式
- `VK_FORMAT_D32_SFLOAT`：深度缓冲格式

---

## 16. 渲染流程详解

### 16.1 帧生命周期

```
Frame N 开始
    │
    ├── vkWaitForFences (等待 Frame N-1 完成)
    │
    ├── vkAcquireNextImageKHR (获取交换链图像)
    │       └── 信号量: imageAvailableSemaphore
    │
    ├── vkResetCommandBuffer (重置命令缓冲)
    │
    ├── 录制命令缓冲
    │   ├── vkBeginCommandBuffer
    │   ├── vkCmdBeginRenderPass
    │   ├── 绑定 Pipeline / DescriptorSets
    │   ├── vkCmdDrawIndexed (绘制调用)
    │   ├── vkCmdEndRenderPass
    │   └── vkEndCommandBuffer
    │
    ├── vkQueueSubmit
    │       ├── 等待: imageAvailableSemaphore
    │       ├── 信号: renderFinishedSemaphore
    │       └── Fence: renderFence
    │
    └── vkQueuePresentKHR
            └── 等待: renderFinishedSemaphore
```

---

### 16.2 描述符更新流程

```cpp
// 材质参数更新
materialInstance->SetVector("BaseColorTint", color);
materialInstance->SetFloat("Roughness", 0.5f);
materialInstance->ApplyChanges();  // 上传到 GPU

// ApplyChanges 内部实现：
void FMaterialInstance::ApplyChanges() {
    // 1. 将 CPU 数据复制到 UBO 缓冲
    void* data;
    vmaMapMemory(allocator, UBOBuffer.Allocation, &data);
    memcpy(data, &ParameterData, sizeof(FMaterialUBO));
    vmaUnmapMemory(allocator, UBOBuffer.Allocation);

    // 2. 更新描述符集指向新数据
    VkDescriptorBufferInfo bufferInfo{...};
    VkWriteDescriptorSet write{...};
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}
```

---

## 17. 常见陷阱与解决方案

### 17.1 命令缓冲区生命周期

**问题**：`VUID-vkFreeCommandBuffers-pCommandBuffers-00047`
```
vkFreeCommandBuffers(): pCommandBuffers[0] is in use
```

**原因**：GPU 还在使用命令缓冲区时就尝试释放它。

**解决**：使用 Fence 等待 GPU 完成，或重用命令缓冲区而非释放。

---

### 17.2 描述符集布局兼容性

**问题**：Pipeline 创建时使用的布局必须与绑定的 DescriptorSet 布局一致。

**解决**：在 `FMaterial` 中保存 `DescriptorSetLayout`，创建实例时使用相同布局。

---

### 17.3 图像布局转换

**问题**：纹理采样时图像布局不正确。

**解决**：使用 Pipeline Barrier 在正确的时机转换布局。

---

### 17.4 交换链图像索引错误

**问题**：使用了错误的 imageIndex。

**解决**：确保 `vkAcquireNextImageKHR` 返回的索引和 `vkQueuePresentKHR` 使用的索引一致。

---

### 17.5 资源创建顺序与销毁顺序

**问题**：销毁资源时崩溃。

**解决**：
- **创建顺序**：Instance → PhysicalDevice → Device → Queue → Buffer/Image/Pipeline
- **销毁顺序**：Buffer/Image/Pipeline → Queue → Device → PhysicalDevice → Instance
- **逆序销毁！**

---

### 17.6 VK_SUBOPTIMAL_KHR 和 VK_ERROR_OUT_OF_DATE_KHR

**问题**：`vkAcquireNextImageKHR` 或 `vkQueuePresentKHR` 返回这些错误。

**原因**：
- `VK_ERROR_OUT_OF_DATE_KHR`：Swapchain 不再兼容表面（窗口大小改变）
- `VK_SUBOPTIMAL_KHR`：Swapchain 仍然可以工作，但不是最优的

**解决**：重建 Swapchain！

```cpp
VkResult result = vkAcquireNextImageKHR(...);
if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
    RecreateSwapchain();
    return;
}
```

---

### 17.7 内存映射 (Memory Mapping) 问题

**问题**：`vkMapMemory` 失败或崩溃。

**原因**：
- 试图映射设备本地内存（Device Local）
- 内存没有设置 `HOST_VISIBLE` 标志

**解决**：
- 只有 Host Visible 内存才能映射
- 使用 Staging Buffer 传输数据到设备本地内存

```cpp
// ❌ 错误：直接映射设备本地内存
// ✅ 正确：使用 Staging Buffer
AllocatedBuffer stagingBuffer;
CreateBuffer(size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
    VMA_MEMORY_USAGE_AUTO_PREFER_HOST, stagingBuffer);
// 映射 stagingBuffer（CPU 可访问）
memcpy(stagingBuffer.Info.pMappedData, data, size);
// 拷贝到 GPU Buffer
CopyBuffer(stagingBuffer.Buffer, gpuBuffer.Buffer, size);
```

---

## 总结

这就是 Tumbler 引擎中使用的所有 Vulkan 核心概念！

**项目中使用的完整 Vulkan 功能清单**：

**核心初始化**：
- VkInstance 创建（带扩展和验证层）
- VkPhysicalDevice 选择（评分机制）
- VkDevice 创建（带队列）
- VkSurfaceKHR 创建（窗口系统连接）
- VmaAllocator 初始化（内存管理）

**资源管理**：
- VkBuffer 与 VMA 分配（AllocatedBuffer）
- VkImage 与 VMA 分配（AllocatedImage）
- VkImageView 创建
- VkSampler 创建
- Staging Buffer 模式

**数据传输**：
- vkCmdCopyBuffer（Buffer 到 Buffer）
- vkCmdCopyBufferToImage（Buffer 到 Image）
- 立即提交模式（Immediate Submit）
- 图像布局转换（Pipeline Barrier）

**渲染管线**：
- VkShaderModule 加载（SPIR-V）
- VkPipeline 构建（Builder 模式）
- VkPipelineLayout（Descriptor Set + Push Constant）
- 动态状态（Viewport, Scissor）

**描述符系统**：
- VkDescriptorSetLayout（全局、材质）
- VkDescriptorPool（普通、ImGui 专用）
- VkDescriptorSet 分配与更新
- 按频率分组（Set 0 全局、Set 1 材质）

**命令系统**：
- VkCommandPool 创建
- VkCommandBuffer 分配与录制
- 命令缓冲区重用
- vkQueueSubmit 提交

**同步机制**：
- VkFence（CPU-GPU 同步）
- VkSemaphore（GPU-GPU 同步）
- VkPipelineBarrier（资源状态转换）

**渲染通道**：
- VkRenderPass 创建（颜色+深度附件）
- VkFramebuffer 创建
- 子通道依赖

**交换链**：
- VkSwapchainKHR 创建
- Swapchain 重建（窗口大小变化）
- vkAcquireNextImageKHR
- vkQueuePresentKHR

**调试工具**：
- Validation Layers
- Debug Utils Messenger
- VK_CHECK 宏
- VkUtils 工具函数

**UI 集成**：
- ImGui Vulkan 后端
- ImGui 专用 Descriptor Pool
- ImGui 渲染命令录制

**关键点回顾**：
1. Vulkan 是显式的，你要控制一切
2. VMA 帮你管理复杂的内存
3. 按频率分组 Descriptor Set（Set 0 全局，Set 1 材质）
4. Pipeline 是不可变的，创建慢但切换快
5. 重用 Command Buffer，不要每帧分配/释放
6. 使用正确的同步原语（Fence, Semaphore, Barrier）
7. Validation Layers 是你的好朋友！
8. Surface 连接 Vulkan 和窗口系统
9. 立即提交模式适合资源上传
10. ImGui 集成需要专用的 Descriptor Pool

现在你应该能看懂项目中所有的 Vulkan 代码了！🎉
