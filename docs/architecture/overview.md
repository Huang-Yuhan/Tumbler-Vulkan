# 引擎架构概览

本项目作为 Vulkan 与现代游戏引擎概念的实践沙盒，遵循"逻辑与渲染分离"的核心理念。

## 1. 实体-组件系统（ECS 变体）

所有场景物体由 `FActor` 表达。`FActor` 持有 `CTransform` 作为值成员，其他能力通过挂载 `Component` 子类实现：

| 组件 | 能力 |
|------|------|
| `CMeshRenderer` | 外观呈现（Mesh + MaterialInstance） |
| `CPointLight` | 点光源 |
| `CDirectionalLight` | 方向光 |
| `CCamera` / `CFirstPersonCamera` | 相机 / 第一人称漫游相机 |

组件通过 `GetComponent<T>()` 查找，内部使用 `std::type_index` 映射实现 O(1) 快速查找。

## 2. 逻辑与渲染的物理隔离

渲染器 (`VulkanRenderer`) 不直接访问 `FActor` 或 `FScene`。数据流：

1. **`FScene::ExtractRenderPackets()`** — 从所有 `CMeshRenderer` 中提取 `RenderPacket` 列表（`shared_ptr<FMesh>`、`shared_ptr<FMaterialInstance>`、`mat4` 变换矩阵）
2. **`FScene::GenerateSceneView()`** — 构建 `SceneViewData`（相机矩阵 + 可见光源列表 + `ERenderPath`）
3. **`VulkanRenderer::Render()`** — 消费上述两种数据结构，对 Actor 和 Scene 一无所知

`SceneViewData::RenderPath` 每帧决定使用 Forward 还是 Deferred 管线。渲染路径由 `RenderSettings::CurrentRenderPath` 统一管理，在 main.cpp 中注入到 `viewData`。

## 3. VulkanRenderer 子系统

```
VulkanRenderer (协调者)
├── VulkanContext          — Instance、Device、队列族
├── VulkanSwapchain        — 交换链图像、深度缓冲、重建
├── RenderDevice           — GPU 资源创建/销毁（VMA 封装）
├── CommandBufferManager   — 命令池分配、即时提交、布局转换
├── ResourceUploadManager  — Mesh 上传（staging→device-local）、纹理加载、Mesh 去重
├── DescriptorSetFreeQueue — 描述符集延迟回收队列
├── SceneParameterBuffer   — 全局 SceneDataUBO 持久映射
└── 同步对象               — VkSemaphore（图像就绪/渲染完成）+ VkFence（帧栅栏）
```

### 3.1 RenderDevice

封装 VMA 资源创建/销毁。`CreateImage()` 支持 `requiredFlags` 参数（如 `VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT`）。

```cpp
void CreateBuffer(size_t size, VkBufferUsageFlags usage, VmaMemoryUsage mem, AllocatedBuffer& out);
void CreateImage(uint32_t w, uint32_t h, VkFormat fmt, VkImageTiling tiling,
    VkImageUsageFlags usage, AllocatedImage& out, VkMemoryPropertyFlags requiredFlags = 0);
void DestroyBuffer(AllocatedBuffer&);
void DestroyImage(AllocatedImage&);
VkImageView CreateImageView(VkImage, VkFormat, VkImageAspectFlags);
VkSampler CreateSampler(VkFilter mag, VkFilter min, VkSamplerAddressMode, bool anisotropy);
```

### 3.2 CommandBufferManager

```cpp
VkCommandBuffer AllocatePrimaryCommandBuffer();
void ImmediateSubmit(std::function<void(VkCommandBuffer)>&& fn);
void TransitionImageLayout(VkImage, VkFormat, VkImageLayout old, VkImageLayout new);
void CopyBufferToImage(VkBuffer, VkImage, uint32_t w, uint32_t h);
void ResetCommandPool();
```

### 3.3 ResourceUploadManager

```cpp
FVulkanMesh& UploadMesh(std::shared_ptr<FMesh> cpuMesh);      // 接受 shared_ptr 保证生命周期
std::shared_ptr<FTexture> LoadTexture(const std::string& path);
bool LoadShaderModule(const char* path, VkShaderModule* out);
bool IsMeshUploaded(FMesh*) const;
void ClearMeshCache();
```

### 3.4 子系统协作流程

```
初始化:
VulkanRenderer::Init()
    ├── Context.Init(window)
    ├── RenderDevice.Init(&Context)
    ├── CommandBufferManager.Init(&Context)
    ├── ResourceUploadManager.Init(&RenderDevice, &CommandBufferManager)
    ├── SwapChain.Init(&Context)
    └── InitPipelines()  // Forward + Deferred

每帧渲染:
Render()
    ├── FlushPendingDescriptorSetFrees()
    ├── vkWaitForFences(kFenceTimeoutNs)  // 5 秒超时
    ├── AcquireNextImage(kAcquireTimeoutNs)
    ├── vkResetCommandBuffer()
    ├── 更新 SceneDataUBO
    ├── Pipeline->RecordCommands(...)
    ├── onUIRender(cmd, imageIndex)       // ImGui 回调
    └── vkEndCommandBuffer()

清理:
Cleanup()
    ├── vkDeviceWaitIdle()
    ├── FlushPendingDescriptorSetFrees()
    ├── 销毁 DescriptorPool + GlobalSetLayout
    ├── Pipelines[i]->Cleanup(this)
    ├── 销毁同步对象
    ├── ResourceUploadManager.Cleanup()
    ├── CommandBufferManager.Cleanup()
    ├── RenderDevice.Cleanup()
    ├── SwapChain.Cleanup()
    └── Context.Cleanup()
```

## 4. 双管线策略

`IRenderPipeline` 是策略接口，有两个实现：

| 管线 | Subpass 数 | 特点 |
|------|-----------|------|
| `FForwardPipeline` | 1 | 几何 + 光照在 `pbr.frag` 中同时计算 |
| `FDeferredPipeline` | 2 | MRT G-Buffer（Albedo + Normal+Roughness）+ 全屏光照 pass |

两条管线在启动时一起初始化，`SceneViewData::RenderPath` 每帧选择使用哪条。运行时可通过控制台 `render.path` 命令或 ImGui Camera 面板热切换。

详细设计参见 [渲染架构](rendering.md)。

### 4.1 IRenderPipeline 接口

```cpp
class IRenderPipeline {
    virtual void Init(VulkanRenderer*) = 0;
    virtual void Cleanup(VulkanRenderer*) = 0;
    virtual void RecreateResources(VulkanRenderer*) = 0;
    virtual void RecordCommands(VkCommandBuffer, uint32_t imageIndex,
        VulkanRenderer*, const SceneViewData&,
        const std::vector<RenderPacket>&) = 0;
    virtual VkRenderPass GetRenderPass() const = 0;

    virtual bool SupportsGBuffer() const { return false; }
    virtual VkImageView GetGBufferAlbedoImageView() const { return VK_NULL_HANDLE; }
    virtual VkImageView GetGBufferNormalImageView() const { return VK_NULL_HANDLE; }
    virtual VkImageView GetGBufferDepthImageView() const { return VK_NULL_HANDLE; }

    // 共享辅助方法
    static void DrawMeshPackets(VkCommandBuffer, VulkanRenderer*,
        ERenderPath, const std::vector<RenderPacket>&);
    static void CreateFramebuffers(VkDevice, VkRenderPass, VkExtent2D,
        const std::vector<VkImageView>& swapchainImageViews,
        const std::vector<VkImageView>& sharedAttachments,
        std::vector<VkFramebuffer>& out);
};
```

`DrawMeshPackets` 和 `CreateFramebuffers` 是静态方法，Forward 和 Deferred 管线共享，避免代码重复。

## 5. 材质系统（母体-实例模式）

`FMaterial` 持有烘焙后的管线（`VkPipeline`、`VkPipelineLayout`、`VkDescriptorSetLayout`），一个母体对应 Forward + Deferred 两套管线。

`FMaterialInstance` 持有每实例的 `DescriptorSet`、`UBOBuffer`、CPU 端 `FMaterialUBO` 镜像。实例通过 `FMaterial::CreateInstance()` 创建，由调用者持有 `shared_ptr`。

```
FMaterial (母体)                     FMaterialInstance (实例 × N)
├── Pipelines[ERenderPath]           ├── shared_ptr<FMaterial> Parent
├── PipelineLayout                   ├── VkDescriptorSet
└── DescriptorSetLayout              ├── AllocatedBuffer UBOBuffer
                                     └── FMaterialUBO ParameterData
```

编辑材质参数时应使用 `UpdateUBO()`（直接写入持久映射缓冲），而非 `ApplyChanges()`（仅更新描述符写入，参数值会丢失）。

## 6. 资产管理系统

`FAssetManager` 提供统一的资产加载和缓存：

```cpp
std::shared_ptr<FMesh> GetOrLoadMesh(name, path);     // 自动 GPU 上传
std::shared_ptr<FTexture> GetOrLoadTexture(name, path);
std::shared_ptr<FMaterial> GetOrLoadMaterial(name, vertPath, fragPath);
void RegisterMesh(name, shared_ptr<FMesh>);           // 注册 CPU 端 Mesh
```

特性：自动缓存 + `std::mutex` 线程安全 + Mesh 自动 GPU 上传。

## 7. 编辑器与运行时控制台

### 7.1 UIManager

Core 层 ImGui 宿主。管理 GLFW/Vulkan 后端的 Init/Cleanup、每帧 `BeginFrame()`/`EndFrame()`、`RecordDrawCommands()` 录制 UI 绘制命令。

### 7.2 EditorSessionState / RenderSettings

```cpp
struct EditorSessionState {
    FActor* SelectedActor = nullptr;   // 当前选中 Actor
    bool ShowDebugPanel = true;        // 是否显示调试面板
};

struct RenderSettings {
    ERenderPath CurrentRenderPath = ERenderPath::Forward;  // 当前渲染路径
};
```

两者分离：`EditorSessionState` 管理编辑器选中状态，`RenderSettings` 管理渲染配置。`RenderSettings` 不与 Actor 选中状态耦合，未来扩展渲染配置项（如阴影质量、分辨率缩放）可直接加入。

### 7.3 DebugTexturePreview

Core/Editor 层组件，封装 G-Buffer 调试预览所需的 Vulkan 资源（`VkSampler`、`VkDescriptorSetLayout`、`VkDescriptorSet`）。AppLogic 不直接触碰任何 `vk*` 函数：

```cpp
class DebugTexturePreview {
    void Init(VulkanRenderer*);
    void Cleanup(VkDevice);
    void SetImage(int slot, VkImageView);    // slot 0=Albedo, 1=Normal
    VkDescriptorSet GetTextureID(int slot) const;  // 用于 ImGui::Image()
};
```

预览嵌入在统一的 Render Debug 窗口中，仅在 Deferred 模式下显示。

### 7.4 RuntimeConsole

Core 层命令框架：管理开关状态、输入框、历史记录、日志、Tab 补全。控制台切换键可通过 `SetToggleKey(EKeyCode)` 配置，默认为 `GraveAccent`。Tumbler 专属命令通过 `TumblerConsoleBindings.cpp` 注册。

## 8. 输入系统

`InputManager` 区分两层输入：

- **Gameplay 输入**：`GetAxis()`、`IsActionPressed()`、`GetMouseDelta()`（被 Blocked 时返回零值）
- **原始按键输入**：`WasKeyJustPressed()`、`GetKey()`（不受 Blocked 影响）

UI 焦点状态通过 `SetUIFocused(bool)` 从外部注入（main.cpp 中读取 `ImGui::GetIO().WantCaptureMouse/Keyboard`），`InputManager` 不再直接依赖 ImGui。

## 9. Linux / Wayland 启动链路

1. 检测 Wayland 会话
2. 清理 Snap Code 注入的 GTK/GIO 环境变量
3. GLFW 优先尝试 `GLFW_PLATFORM_WAYLAND`
4. 失败则退回 `GLFW_ANY_PLATFORM`
5. 最终兜底尝试 `GLFW_PLATFORM_X11`

Vulkan 表面扩展失败时打印完整诊断信息（平台、可用扩展列表、缺失的 WSI 扩展）。

## 10. 设计原则总结

| 原则 | 实践 |
|------|------|
| 单一职责 | 每个子系统只负责一个领域 |
| 逻辑渲染分离 | Renderer 不访问 Actor，只消费 RenderPacket + SceneViewData |
| 策略模式 | IRenderPipeline → Forward / Deferred 可热切换 |
| 母体-实例 | FMaterial 管线共享 + FMaterialInstance 参数独立 |
| 依赖倒置 | Core 不依赖 Example；Example 命令通过注册机制挂载 |
