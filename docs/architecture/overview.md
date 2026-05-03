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

## 6. 编辑器与运行时控制台架构

为了避免把调试功能继续堆进 `AppLogic`，当前版本把编辑器运行时能力拆成了 3 层：

### 6.1 `UIManager`：ImGui 生命周期与控制台宿主

`UIManager` 负责：
- ImGui 的 GLFW/Vulkan 后端初始化
- 每帧 `BeginFrame()` / `EndFrame()`
- 在 UI 结束前统一绘制 `RuntimeConsole`
- 通过 `TickInput()` 驱动控制台输入逻辑

这意味着控制台不再是示例逻辑的一部分，而是 Core 层的通用工具。

### 6.2 `EditorSessionState`：共享编辑状态桥

`EditorSessionState` 目前至少承载：
- `SelectedActor`
- `CurrentRenderPath`

这样做的目的，是把“当前编辑上下文”从 `AppLogic` 私有状态中抽出来，让：
- Hierarchy / Inspector
- 运行时控制台命令
- 渲染路径切换

共享同一份状态，而不是在多个系统里复制一份选择结果。

### 6.3 `RuntimeConsole`：Core 层命令框架

`RuntimeConsole` 的职责固定为：
- 管理开关状态、输入框、日志、历史记录和滚动
- 注册命令定义
- 执行内建命令
- 提供命令名和参数级别的 `Tab` 补全

Tumbler 专属命令并不写死在 Core 中，而是通过 `TumblerConsoleBindings.cpp` 注册进去。这样 Core 只依赖“命令框架”，不依赖具体场景业务。

## 7. 输入与控制台的耦合边界

`InputManager` 目前显式区分了两类输入：

- **Gameplay 输入**
  由 `GetAxis()`、`IsActionPressed()`、`IsActionJustPressed()`、`GetMouseDelta()` 消费
- **原始按键输入**
  由 `WasKeyJustPressed()` 提供

这样运行时控制台可以使用 `WasKeyJustPressed(EKeyCode::GraveAccent)` 来切换开关，同时在打开控制台的同一帧调用 `SetGameplayInputBlocked(true)`，立即阻断相机移动和鼠标看向。

这个设计比单纯依赖 `ImGuiIO::WantCaptureKeyboard` 更稳，因为它不需要等待 ImGui 下一阶段才告诉输入系统“现在 UI 想接管键盘”。

## 8. Linux / Wayland 启动链路

Linux 下的窗口和 Vulkan 初始化现在有一条更稳的引导链：

1. `AppWindow` 先判断当前是否为 Wayland 会话
2. 如果检测到 Snap Code 注入的 GTK/GIO 环境，会先清理相关环境变量
3. GLFW 初始化优先尝试 `GLFW_PLATFORM_WAYLAND`
4. 如果失败，则退回 `GLFW_ANY_PLATFORM`
5. 最后在 Linux 下再尝试一次 `GLFW_PLATFORM_X11`

同时，`AppWindow` 会在 `glfwGetRequiredInstanceExtensions()` 失败时：
- 打印 GLFW 当前平台
- 打印 Vulkan loader 暴露的实例扩展列表
- 针对 Wayland / X11 分别指出缺的是 `VK_KHR_wayland_surface` 还是 `VK_KHR_xcb_surface` / `VK_KHR_xlib_surface`

这套诊断链路的目标不是“吞掉错误”，而是把问题明确分成：
- 工程依赖配置问题
- 系统 Vulkan WSI 暴露问题
- 桌面环境污染问题

## 9. 设计原则总结

| 原则 | 实践 |
|------|------|
| 单一职责 | 每个子系统只负责一个领域 |
| 依赖倒置 | 高层模块不依赖低层模块实现细节 |
| 开闭原则 | 新增渲染特性无需修改核心渲染器 |
| 接口隔离 | 子系统暴露最小必要接口 |
| 迪米特法则 | 模块间通信通过明确的接口 |
