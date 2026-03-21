# 引擎架构概览 (Architecture Overview)

本项目作为一个 Vulkan 与现代游戏引擎概念的实践沙盒，在结构上遵循了"逻辑与渲染分离"的核心理念。

## 1. 实体-组件系统 (ECS 变体)
场景中的所有客观物体均由 `FActor` 表达。
`FActor` 仅作为一个容器和 Transform（变换）的持有者，所有的特定能力均通过挂载特定的 `Component` 来实现。
- **外观呈现**：挂载 `CMeshRenderer`，指定对应的 Mesh 和 MaterialInstance。
- **环境光照**：挂载 `CPointLight` 或 `CDirectionalLight`。

这种组合优于继承的设计，极大提升了对象设计时的灵活性。

## 2. 逻辑与渲染的极度解耦 (Physical Isolation)

这是现代引擎（如 Unreal Engine）最核心的设计思想之一。
渲染器 (`VulkanRenderer`) **绝对不应该**直接访问场景树或解析 `FActor`。它就像一个纯粹的画图机器，只有人在喂给它数据时，它才开始工作。

### 数据流转流程：
1. **客观世界提取 (`RenderPacket`)**
   在渲染前，引擎从 `FScene` 的所有的 `CMeshRenderer` 中抽取纯净的渲染数据包 `RenderPacket`。包含：
   - 几何体结构 `FMesh*`
   - 材质属性 `FMaterialInstance*`
   - 变换矩阵 `glm::mat4`
   渲染器拿着这堆"包裹"，只管往画面上画，完全不知道它们属于哪个"Actor"。

2. **观察者视图 (`SceneViewData`)**
   游戏视角不仅有相机矩阵（View, Projection），还包含了**能被这个相机看到的**所有光源信息（`std::vector<LightData> Lights`）。
   `SceneViewData` 代表环境上下文。如果做分屏双人游戏或阴影映射，我们只需要生成多份不同的 `SceneViewData`，它们依然可以利用同一批 `RenderPacket`。

这种严格的物理层面隔离，为将来的多线程渲染（逻辑线程打包，渲染线程解包提交）打下了稳固的基础。

## 3. VulkanRenderer 子系统架构

为了遵循单一职责原则（SRP），`VulkanRenderer` 已被拆分为多个专注的子系统：

```
VulkanRenderer (协调者)
├── VulkanContext        - Vulkan 实例、设备、队列管理
├── VulkanSwapchain      - 交换链、图像视图、深度缓冲
├── RenderDevice         - GPU 资源创建/销毁 (Buffer, Image)
├── CommandBufferManager - 命令池、命令缓冲、即时提交
└── ResourceUploadManager - Mesh/Texture 上传管线
```

### 3.1 RenderDevice (渲染设备)
**职责**：封装所有 GPU 资源的创建和销毁操作。

```cpp
class RenderDevice {
    void CreateBuffer(size_t size, VkBufferUsageFlags usage, ...);
    void DestroyBuffer(AllocatedBuffer& buffer);
    void CreateImage(uint32_t width, uint32_t height, ...);
    void DestroyImage(AllocatedImage& image);
    VkSampler CreateSampler(...);
    // ...
};
```

**设计优势**：
- 统一的资源生命周期管理
- VMA 内存分配逻辑集中化
- 便于未来实现资源池和内存回收

### 3.2 CommandBufferManager (命令缓冲管理器)
**职责**：管理命令池和命令缓冲区的分配、提交、同步。

```cpp
class CommandBufferManager {
    VkCommandBuffer AllocatePrimaryCommandBuffer();
    void FreeCommandBuffer(VkCommandBuffer cmd);
    void ImmediateSubmit(std::function<void(VkCommandBuffer)>&& fn);
    void TransitionImageLayout(VkImage, VkImageLayout old, VkImageLayout new);
    // ...
};
```

**设计优势**：
- 命令缓冲区的生命周期独立管理
- 即时提交（Immediate Submit）逻辑复用
- 支持多线程命令录制（未来扩展）

### 3.3 ResourceUploadManager (资源上传管理器)
**职责**：处理 CPU 数据到 GPU 的传输，包括 Mesh 和 Texture。

```cpp
class ResourceUploadManager {
    FVulkanMesh& UploadMesh(FMesh* cpuMesh);
    std::shared_ptr<FTexture> LoadTexture(const std::string& path);
    bool LoadShaderModule(const char* path, VkShaderModule* out);
    // ...
};
```

**设计优势**：
- Staging Buffer 创建和销毁逻辑封装
- Mesh 缓存管理（避免重复上传）
- 纹理加载管线统一入口

### 3.4 子系统协作流程

```
初始化阶段:
VulkanRenderer::Init()
    ├── Context.Init(window)           // 创建 Instance、Device
    ├── RenderDevice.Init(&Context)    // 初始化资源管理器
    ├── CommandBufferManager.Init(&Context)
    ├── ResourceUploadManager.Init(&RenderDevice, &CommandBufferManager)
    └── SwapChain.Init(&Context)

运行时渲染:
VulkanRenderer::Render()
    ├── 等待上一帧完成 (vkWaitForFences)
    ├── 获取交换链图像
    ├── 重置命令缓冲
    ├── 录制渲染命令
    │   └── ResourceUploadManager::UploadMesh() // 按需上传
    ├── 提交到 GPU
    └── 呈现图像

清理阶段:
VulkanRenderer::Cleanup()
    ├── 等待 GPU 空闲
    ├── ResourceUploadManager.Cleanup()  // 清理 Mesh 缓存
    ├── CommandBufferManager.Cleanup()   // 销毁命令池
    ├── RenderDevice.Cleanup()           // 清理 VMA
    └── Context.Cleanup()
```

## 4. 材质系统架构

材质系统采用"母体-实例"模式，支持高效的资源复用：

```
FMaterial (母体材质)
├── Pipeline              // 烘焙的渲染管线
├── PipelineLayout        // 管线布局
├── DescriptorSetLayout   // 描述符布局
└── FMaterialInstance[]   // 材质实例列表

FMaterialInstance (材质实例)
├── DescriptorSet         // 描述符集
├── UBOBuffer            // 材质参数缓冲
└── FMaterialUBO         // CPU 端参数镜像
    ├── BaseColorTint
    ├── Roughness
    └── Metallic
```

### 4.1 依赖关系

```
FMaterialInstance
    └── FMaterial
            └── RenderDevice (创建 Pipeline)
            └── VulkanRenderer (分配 DescriptorSet)
```

### 4.2 使用流程

```cpp
// 1. 创建母体材质（由 FAssetManager 管理）
auto pbrMaterial = AssetMgr->GetOrLoadMaterial(
    "PBR_Base",
    "shaders/pbr.vert.spv",
    "shaders/pbr.frag.spv"
);

// 2. 创建材质实例
auto matRed = pbrMaterial->CreateInstance();

// 3. 设置材质参数
matRed->SetVector("BaseColorTint", glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
matRed->SetFloat("Roughness", 0.5f);
matRed->SetFloat("Metallic", 0.0f);

// 4. 提交到 GPU
matRed->ApplyChanges();

// 5. 挂载到渲染组件
meshRenderer->SetMaterial(matRed);
```

## 5. 资产管理系统

`FAssetManager` 提供统一的资产加载和缓存：

```cpp
class FAssetManager {
    std::shared_ptr<FMesh> GetOrLoadMesh(const std::string& name, const std::string& path);
    std::shared_ptr<FTexture> GetOrLoadTexture(const std::string& name, const std::string& path);
    std::shared_ptr<FMaterial> GetOrLoadMaterial(const std::string& name, ...);
};
```

**特性**：
- 自动缓存，避免重复加载
- 线程安全（`std::mutex` 保护）
- 自动 GPU 上传（Mesh 通过 `ResourceUploadManager`）

## 6. 设计原则总结

| 原则 | 实践 |
|------|------|
| 单一职责 | 每个子系统只负责一个领域 |
| 依赖倒置 | 高层模块不依赖低层模块实现细节 |
| 开闭原则 | 新增渲染特性无需修改核心渲染器 |
| 接口隔离 | 子系统暴露最小必要接口 |
| 迪米特法则 | 模块间通信通过明确的接口 |
