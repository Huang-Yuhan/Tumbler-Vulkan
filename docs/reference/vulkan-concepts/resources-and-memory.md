## 目录
1. [核心数据结构与金字塔](#1-核心数据结构与金字塔)
2. [表面 (VkSurfaceKHR)](#2-表面-vksurfacekhr)
3. [显存与资源的绝对把控 (VMA)](#3-显存与资源的绝对把控-vma)

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

