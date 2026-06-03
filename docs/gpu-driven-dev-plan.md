# GPU-Driven 引擎开发计划

## 概述

在 `gpu-driven-rewrite` 分支上从零重写，删除所有 `src/` 旧代码，只保留构建配置和资产。新引擎纯 GPU-Driven + Deferred，无 ECS，无 Forward 路径。

---

## 一、架构总览

```
AppLogic
  └─ Scene (CPU 侧直接维护 vector<ObjectData>)
       └─ GPUScene (GPU 侧 SSBO: ObjectData[] + MeshData[] + MaterialData[])

VulkanRenderer::Render()
  ├─ GPUScene::UploadDirtyObjects()      // 同步 CPU → GPU
  ├─ CullingPass::Execute()              // Compute Shader → Indirect Draw Buffer
  ├─ DepthPass::Execute()                // Shadow Map (Indirect Draw)
  ├─ GBufferPass::Execute()              // Albedo + Normal + Depth (Indirect Draw)
  ├─ LightingPass::Execute()             // Fullscreen Lighting + Shadow Sampling
  └─ UIManager::Render()                 // ImGui
```

### 每帧 GPU 管线

```
Compute  │ Frustum Culling → 填充 IndirectDraw + VisibleCount
Barrier  │
Graphics │ Shadow Depth Pass    → vkCmdDrawIndexedIndirect
Graphics │ GBuffer Pass (MRT)   → vkCmdDrawIndexedIndirect
Graphics │ Lighting Pass        → vkCmdDraw(3)  全屏三角形
Graphics │ ImGui Pass
```

### Descriptor Set 绑定模型

```
Set 0 (Global, 每帧绑定一次):
  binding 0: SceneUBO        (ViewProj, CameraPos, LightData, ShadowMatrix)
  binding 1: sampler2DShadow (Shadow Map)

Set 1 (Bindless, 初始化时写入一次):
  binding 0: texture2D[]     (所有纹理的数组, 最多 1024)
  binding 1: MaterialData[]  (SSBO, 材质参数数组)
  binding 2: ObjectData[]    (SSBO, Transform + MeshIndex + MaterialIndex)

Push Constants: 无需 (所有数据通过 SSBO 索引)
```

### GPU 数据布局

```
// ObjectDataSSBO (由 GPUScene 管理, Host-Visible, 持久化映射)
struct ObjectGPUData {
    mat4 modelMatrix;       // 世界变换
    uint meshIndex;         // 索引到 MeshDataSSBO
    uint materialIndex;     // 索引到 MaterialDataSSBO
    uint _pad[2];
};

// MeshDataSSBO (Mesh 上传时填入)
struct MeshGPUData {
    uint64_t vertexAddress;  // VkDeviceAddress
    uint64_t indexAddress;   // VkDeviceAddress
    uint indexCount;
    uint vertexCount;
    vec4 boundingSphere;     // xyz = center, w = radius
};

// MaterialDataSSBO (材质创建时填入)
struct MaterialGPUData {
    uint albedoTexIndex;     // 索引到 Set 1 的 texture2D[]
    uint normalTexIndex;
    uint metallicRoughnessTexIndex;
    float roughness;
    float metallic;
    vec4 baseColorTint;      // + TwoSided flag in .w
};
```

---

## 二、文件结构

```
src/
├── Core/
│   ├── Platform/
│   │   └── AppWindow.h/.cpp           # GLFW 窗口 + Surface 创建
│   │
│   ├── Graphics/
│   │   ├── VulkanContext.h/.cpp        # Instance + Device + VMA + 队列族
│   │   ├── VulkanSwapchain.h/.cpp      # 交换链 + 深度缓冲
│   │   ├── RenderDevice.h/.cpp         # Buffer/Image/Sampler 创建/销毁 (VMA)
│   │   ├── CommandManager.h/.cpp       # CommandPool + 即时提交 + 通用布局转换
│   │   ├── ResourceManager.h/.cpp      # 统一 VB/IB + Mesh 索引 + 纹理索引
│   │   ├── DescriptorManager.h/.cpp    # Set 0 + Set 1 (Bindless) 创建和管理
│   │   ├── GPUScene.h/.cpp             # 三个 SSBO 的分配和 CPU→GPU 同步
│   │   ├── IndirectDraw.h/.cpp         # Indirect Command Buffer + 每帧重置
│   │   ├── CullingPass.h/.cpp          # Frustum Culling Compute 调度
│   │   ├── DepthPass.h/.cpp            # Shadow Map 渲染 (Indirect)
│   │   ├── GBufferPass.h/.cpp          # MRT G-Buffer 渲染 (Indirect)
│   │   ├── LightingPass.h/.cpp         # 全屏 Deferred Lighting
│   │   └── Renderer.h/.cpp             # 帧循环编排 + 初始化/清理
│   │
│   ├── Scene/
│   │   ├── Scene.h/.cpp                # Object 数组管理, 与 GPUScene 同步
│   │   └── Camera.h/.cpp               # FPS 漫游相机
│   │
│   ├── Editor/
│   │   ├── UIManager.h/.cpp            # ImGui 初始化 + 渲染
│   │   └── DebugUI.h/.cpp              # 调试面板 (性能、可见实例数、渲染路径)
│   │
│   └── Utils/
│       ├── Log.h/.cpp                  # spdlog 封装
│       ├── VulkanUtils.h               # VK_CHECK, GetVector, 格式化
│       └── Math.h                      # 视锥体提取、矩阵工具
│
└── Examples/
    └── Tumbler/
        ├── main.cpp                    # 入口
        └── AppLogic.h/.cpp             # 场景搭建 + 帧逻辑
```

### 着色器（保留在 `assets/shaders/engine/`）

```
frustum_cull.comp      # GPU Frustum Culling
depth.vert             # Shadow depth
gbuffer.vert           # G-Buffer 顶点 (从 SSBO 读 Transform)
gbuffer.frag           # G-Buffer 片段 (Write Albedo + Normal)
lighting.vert          # 全屏三角形
lighting.frag          # Deferred Lighting + PBR + PCF 阴影
```

---

## 三、分模块开发顺序

每个模块写完后立即验证，不攒到最后一口气。

### 阶段 0: 分支与清理

- `git checkout -b gpu-driven-rewrite`
- 删除 `src/Core/` 和 `src/Examples/` 所有 `.h/.cpp`
- 删除 `Tumbler_Dev_Plan.md`、`Design_Review.md`
- 提交空壳

### 阶段 1: 平台层（无 Vulkan 依赖）

| 模块 | 关键内容 |
|------|----------|
| `Log.h/.cpp` | spdlog 封装，`LOG_INFO/WARN/ERROR/CRITICAL` 宏 |
| `AppWindow.h/.cpp` | GLFW 窗口创建、Framebuffer 大小查询、Surface 创建、`glfwPollEvents` |
| `Math.h` | `ExtractFrustumPlanes(proj * view)` 返回 6 个平面 |

**验证**: 编译通过，无链接错误。

### 阶段 2: Vulkan 基础设施

| 模块 | 关键内容 |
|------|----------|
| `VulkanContext` | Vulkan 1.3 Instance + Device, 启用 **`bufferDeviceAddress` / `descriptorIndexing` / `drawIndirectCount`** 三大特性, VMA 分配器, Graphics Queue + Present Queue |
| `RenderDevice` | `CreateBuffer()` / `CreateImage()` / `CreateSampler()` / `GetBufferDeviceAddress()` / `DestroyBuffer()` / `DestroyImage()` |
| `CommandManager` | CommandPool, `AllocatePrimaryCB()`, `ImmediateSubmit()`, **通用** `TransitionImageLayout()` (根据 old/new layout 自动推导 access mask 和 stage flags), `CopyBufferToImage()` |
| `VulkanUtils.h` | `VK_CHECK` 宏, `VkUtils::GetVector<T>()` |

**验证**: 启动时 Vulkan 初始化成功（Validation Layer 无报错），能够清屏一帧。

### 阶段 3: 资源管理

| 模块 | 关键内容 |
|------|----------|
| `VulkanSwapchain` | Swapchain 创建/重建, Depth Image, ImageViews, `AcquireNextImage()` / `PresentImage()` |
| `ResourceManager` | 统一大 VB/IB 分配 + sub-allocation, Mesh 上传/索引化, Texture 上传/索引化（stb_image）, Shader Module 加载 |
| `DescriptorManager` | Global DescriptorPool, **Set 0 Layout**: binding 0 = UBO (SceneData), binding 1 = Combined Image Sampler (Shadow Map) — 这两个先做。**Set 1 暂缓**，Phase 5 再上 Bindless |

**验证**: 能加载一个 OBJ 模型上传到 GPU，能加载纹理，DescriptorSet 创建成功。

### 阶段 4: 场景与 GPU 数据

| 模块 | 关键内容 |
|------|----------|
| `Scene.h/.cpp` | `vector<ObjectData>` (CPU 副本), `AddObject()` / `RemoveObject()` / `UpdateTransform()`, Dirty 标记 |
| `GPUScene.h/.cpp` | 创建 ObjectDataSSBO (Host-Visible, Persistent Mapped), MeshDataSSBO (Device-Local), MaterialDataSSBO (Host-Visible). `UploadDirtyObjects()` — 只同步有变更的 Object |
| `Camera.h/.cpp` | FPS 漫游 (WASD + Mouse), `GetViewMatrix()` / `GetProjectionMatrix()`, Frustum Plane 提取 |

**验证**: 创建几个 Object, 上传到 SSBO, 通过 RenderDoc 查看 GPU Buffer 内容。

### 阶段 5: 渲染 Pass（硬编码管线 + 非 Indirect）

| 模块 | 关键内容 |
|------|----------|
| `DepthPass` | Shadow Map RenderPass (D32), Pipeline, Framebuffer. CPU 循环遍历 Object 发 Draw Call (先用非 Indirect 方式验证) |
| `GBufferPass` | MRT RenderPass: Attachment 0 = Albedo (R8G8B8A8), Attachment 1 = Normal+Roughness (R16G16B16A16), Attachment 2 = Depth (D32). Vertex Shader 从 ObjectDataSSBO 读 Transform, Fragment Shader 写 G-Buffer |
| `LightingPass` | 全屏三角形, Fragment Shader 读取 G-Buffer (Input Attachment 或 Sampled Image), PBR 光照, PCF 阴影采样. 最终输出到 Swapchain |
| `Renderer` | 帧循环编排: Acquire → Update UBO → Shadow Pass → GBuffer Pass → Lighting Pass → ImGui → Submit |

**验证**: Cornell Box 场景正常渲染（前向等价效果，但走 Deferred 路径）。

### 阶段 6: GPU-Driven 核心（Compute Culling + Indirect Draw）

| 模块 | 关键内容 |
|------|----------|
| `IndirectDraw` | `VkBuffer` (INDIRECT_BIT + STORAGE_BIT), 每帧开始前用 Compute 或 vkCmdFillBuffer 清零 count, `VkDrawIndexedIndirectCommand[]` |
| `frustum_cull.comp` | 输入: ObjectDataSSBO, MeshDataSSBO, Frustum Planes (来自 UBO). 对每个 Object 做 Sphere-Frustum 测试, 通过则 `atomicAdd` 写入 Indirect Command |
| `CullingPass` | Dispatch Compute Shader, Barrier |
| `GBufferPass` 改造 | 从 `vkCmdDrawIndexed` 循环改为 `vkCmdDrawIndexedIndirect(countBuffer)` |
| `DepthPass` 改造 | 同上, 使用光源视角的 Frustum Culling |

**验证**: 渲染正确, ImGui 显示可见实例数随相机旋转动态变化, Draw Call 降至 Pipeline Bucket 数量级别。

### 阶段 7: Bindless 纹理 + 多材质

| 模块 | 关键内容 |
|------|----------|
| `DescriptorManager` 扩展 | 创建 Set 1: `texture2D[]` 大数组 + `MaterialData[]` SSBO. 纹理加载时自动分配索引并写数组 |
| Shader 改造 | `gbuffer.frag` 和 `lighting.frag` 从 `MaterialDataSSBO[object.materialIndex]` 读参数, 通过 `albedoTexIndex` 从 `texture2D[]` 采样 |
| Pipeline Bucket | 按材质模板分组 (Opaque, TwoSided, Transparent), 每组一个 Indirect Buffer. Culling 时按 materialIndex 对应的 pipelineBucket 写入不同 Buffer |

**验证**: 不同材质的物体在同一个 vkCmdDrawIndexedIndirect 调用中渲染, 纹理各异。

### 阶段 8: 编辑器和调试

| 模块 | 关键内容 |
|------|----------|
| `UIManager` | ImGui GLFW/Vulkan 后端, `Init()` / `Render()` / `Cleanup()`, 管理自己的 RenderPass |
| `DebugUI` | 性能面板 (FPS/FrameTime/DrawCalls/VisibleObjects), 渲染路径信息, Shadow Map 预览 |

**验证**: ImGui 面板正常叠加显示, 统计数据实时更新。

---

## 四、关键技术细节

### VulkanContext 必须开启的特性

```cpp
VkPhysicalDeviceVulkan12Features features12{
    .bufferDeviceAddress = VK_TRUE,
    .descriptorIndexing = VK_TRUE,
    .shaderSampledImageArrayNonUniformIndexing = VK_TRUE,
    .descriptorBindingPartiallyBound = VK_TRUE,
    .runtimeDescriptorArray = VK_TRUE,
    .drawIndirectCount = VK_TRUE,
};
// 链接到 VkDeviceCreateInfo.pNext
```

同时请求设备扩展:
- `VK_KHR_SWAPCHAIN_EXTENSION_NAME`
- `VK_KHR_buffer_device_address`

### Vertex Shader 读取 SSBO 的方式

```glsl
// gbuffer.vert
layout(set = 1, binding = 2) readonly buffer ObjectData {
    ObjectGPUData objects[];
} objectSSBO;

void main() {
    ObjectGPUData obj = objectSSBO.objects[gl_DrawIndex];  // 或 gl_BaseInstance
    gl_Position = scene.ViewProj * obj.modelMatrix * vec4(inPosition, 1.0);
}
```

注意：使用 `gl_DrawIndex` 需要 `VK_KHR_draw_indirect_count` 支持的 `vkCmdDrawIndexedIndirectCount`，或者使用 `firstInstance` 技巧传递 object index。

### Unified Vertex/Index Buffer

- `ResourceManager` 初始化时分配一个大 Buffer（如 64MB Vertex + 16MB Index）
- 每个 Mesh 上传时 sub-allocate 并记录 offset
- GPU Culling Shader 通过 `MeshDataSSBO[meshIndex].vertexAddress` 直接获取地址
- 绘制前 `vkCmdBindVertexBuffers(cmd, 0, 1, &bigVBBuffer, &zeroOffset)` — 绑定整个大 Buffer, Indirect Command 中的 `firstVertex` 和 `firstIndex` 自动定位

### Scene 接口设计

```cpp
class Scene {
public:
    uint32_t AddObject(const std::string& name);
    void RemoveObject(uint32_t objectIndex);
    void SetTransform(uint32_t objectIndex, const glm::mat4& matrix);
    void SetMesh(uint32_t objectIndex, MeshHandle mesh);
    void SetMaterial(uint32_t objectIndex, MaterialHandle material);

    void SyncToGPU(GPUScene& gpuScene);  // 只同步 dirty objects
private:
    std::vector<ObjectData> Objects;
    std::vector<bool> DirtyFlags;
};
```

---

## 五、不做的内容

- **Forward 渲染路径**: 纯 Deferred
- **ECS 系统**: Scene 直接管理 Object 数组
- **Material 模板-实例模式**: 简化为 MaterialData SSBO 索引
- **运行时控制台**: 后续再加
- **SSAO**: Phase 2 后续
- **多线程命令录制**: 保持单线程，先跑稳 GPU-Driven
- **资源异步加载**: 先用同步加载

---

## 六、G-Buffer 格式

| Attachment | Format | 通道分配 |
|------------|--------|----------|
| 0 (Albedo) | `R8G8B8A8_UNORM` | RGB = Albedo, A = (unused or AO future) |
| 1 (Normal+Roughness) | `R16G16B16A16_SFLOAT` | RG = Octahedron Encoded Normal, B = Roughness, A = Metallic |
| 2 (Depth) | Swapchain Depth Format | 复用 Swapchain 的 Depth Image |

Lighting Pass 直接 sample Attachment 0 和 1 作为 `VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER`（不走 Input Attachment，简化 Barrier 管理）。

---

## 七、依赖库（复用 vcpkg.json）

所有依赖已配置好，不需要改:
- `vulkan` + `vulkan-loader` + `vulkan-memory-allocator`
- `glfw3` (with Wayland)
- `glm`
- `imgui` (glfw-binding + vulkan-binding)
- `spdlog`
- `stb`
- `tinyobjloader`
- `gtest` (测试用)
