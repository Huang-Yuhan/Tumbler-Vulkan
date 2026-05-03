# 渲染管线深度解析 (Rendering Pipeline Deep Dive)

本文档深入剖析 Tumbler 引擎的 Vulkan 渲染管线，从数据准备到最终像素输出的完整流程。
文档对应当前实现的**双管线策略模式 (Dual-Pipeline Strategy Pattern)** 架构。

---

## 1. 渲染架构概览

Tumbler 引擎的渲染系统采用严格的分层 + 策略模式设计：

```
┌──────────────────────────────────────────────────────────────┐
│                    应用层 (AppLogic)                          │
│  - 场景管理 (FScene)                                          │
│  - 输入处理 / 游戏逻辑                                        │
│  - 选择 ERenderPath (Forward / Deferred)                      │
└───────────────────────────┬──────────────────────────────────┘
                            │ SceneViewData + RenderPackets
                            ▼
┌──────────────────────────────────────────────────────────────┐
│               渲染协调层 (VulkanRenderer)                     │
│  - 管理帧同步 (Fence / Semaphore)                             │
│  - 更新全局 UBO (SceneDataUBO)                                │
│  - 将命令录制工作委派给 IRenderPipeline                       │
│  - 在管线完成后执行 UI 回调 (onUIRender)                      │
│  - 统一调用 vkEndCommandBuffer                                │
└────────────┬──────────────────────────┬──────────────────────┘
             │                          │
             ▼                          ▼
┌────────────────────┐    ┌─────────────────────────────────┐
│  FForwardPipeline  │    │       FDeferredPipeline          │
│  - 单 Subpass      │    │  - 2 Subpasses (MRT G-Buffer)   │
│  - pbr.vert/frag   │    │  - deferred_geometry.vert/frag  │
│                    │    │  - deferred_lighting.vert/frag   │
└────────────────────┘    └─────────────────────────────────┘
             │                          │
             └──────────┬───────────────┘
                        │ Vulkan 命令
                        ▼
         ┌──────────────────────────┐
         │        驱动与 GPU         │
         └──────────────────────────┘
```

---

## 2. 数据准备阶段

### 2.1 RenderPacket (渲染包)

`RenderPacket` 是从场景中提取的纯净渲染数据：

```cpp
struct RenderPacket {
    FMesh* Mesh;                    // 几何体
    FMaterialInstance* Material;    // 材质实例（携带描述符集）
    glm::mat4 TransformMatrix;      // 模型矩阵 (Push Constants)
};
```

### 2.2 SceneViewData (场景视图数据)

`SceneViewData` 代表特定相机视角下的环境信息，并携带**渲染路径选择**：

```cpp
enum class ERenderPath {
    Forward,
    Deferred,
    GPUDriven  // 预留
};

struct SceneViewData {
    glm::mat4 ViewMatrix;
    glm::mat4 ProjectionMatrix;
    glm::vec3 CameraPosition;
    std::vector<LightData> Lights;

    ERenderPath RenderPath = ERenderPath::Forward;  // 控制选用哪条管线
};
```

`ERenderPath` 字段是 VulkanRenderer 在 `RecordCommandBuffer` 内分发到对应 `IRenderPipeline` 的唯一键。

---

## 3. IRenderPipeline — 策略接口

所有渲染管线实现一个统一的 `IRenderPipeline` 接口，实现策略模式 (Strategy Pattern)：

```cpp
class IRenderPipeline {
public:
    virtual void Init(VulkanRenderer* renderer) = 0;
    virtual void Cleanup(VulkanRenderer* renderer) = 0;
    virtual void RecreateResources(VulkanRenderer* renderer) = 0;  // Swapchain 重建触发

    virtual void RecordCommands(
        VkCommandBuffer cmd,
        uint32_t imageIndex,
        VulkanRenderer* renderer,
        const SceneViewData& viewData,
        const std::vector<RenderPacket>& renderPackets,
        std::function<void(VkCommandBuffer)> onUIRender
    ) = 0;

    virtual VkRenderPass GetRenderPass() const = 0;
};
```

`VulkanRenderer` 内部用 `unordered_map` 持有所有管线：

```cpp
std::unordered_map<ERenderPath, std::unique_ptr<IRenderPipeline>> Pipelines;
```

在 `Init()` 时，Forward 和 Deferred 管线**同时初始化**，在运行时通过 `SceneViewData::RenderPath` 动态选择。

---

## 4. 完整帧循环 (VulkanRenderer::Render)

```
① vkWaitForFences(RenderFence)          ← 等待上一帧 GPU 执行完毕
② SwapChain.AcquireNextImage(...)       ← 获取交换链图像索引
③ vkResetFences(RenderFence)
④ vkResetCommandBuffer(MainCB)
⑤ RecordCommandBuffer(...)
   ├── 更新全局 SceneDataUBO (memcpy 到已映射缓冲)
   ├── Pipelines[viewData.RenderPath]->RecordCommands(...) ← 委派给管线
   ├── onUIRender(cmdBuffer, imageIndex)  ← 执行 ImGui UI 回调
   └── vkEndCommandBuffer(MainCB)         ← 统一在这里关闭
⑥ vkQueueSubmit(wait: ImageAvailable, signal: RenderFinished, fence: RenderFence)
⑦ SwapChain.PresentImage(wait: RenderFinished)
```

> **关键设计**：`vkEndCommandBuffer` 始终在 `VulkanRenderer::RecordCommandBuffer` 尾部统一调用，
> 各 `IRenderPipeline::RecordCommands` 实现**不得**自行结束命令缓冲，仅负责 `vkBeginCommandBuffer` 到
> `vkCmdEndRenderPass`。

---

## 5. Forward 渲染管线 (FForwardPipeline)

`FForwardPipeline` 是经典的单 RenderPass / 单 Subpass 正向渲染。

### 5.1 RenderPass 结构

| 附件 | 格式 | finalLayout |
|------|------|-------------|
| Color | Swapchain 格式 | `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR` |
| Depth | Swapchain Depth 格式 | `VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL` |

### 5.2 录制流程

```
vkBeginCommandBuffer
vkCmdBeginRenderPass (Subpass 0)
  vkCmdSetViewport / vkCmdSetScissor
  for each RenderPacket:
    vkCmdBindPipeline (Forward 变体)
    vkCmdBindDescriptorSets (Set0: GlobalUBO, Set1: MaterialUBO)
    vkCmdPushConstants (Model 矩阵)
    vkCmdBindVertexBuffers / vkCmdBindIndexBuffer
    vkCmdDrawIndexed
vkCmdEndRenderPass
← 返回，VulkanRenderer 紧接着调 vkEndCommandBuffer
```

渲染采用 **Cook-Torrance BRDF (PBR)** 直接在 `pbr.frag` 中同时执行光照计算。

---

## 6. Deferred 渲染管线 (FDeferredPipeline)

`FDeferredPipeline` 是本引擎的核心特性，采用精简的 **2 Subpass G-Buffer 方案**。

### 6.1 G-Buffer 设计

仅分配 **2 个** 显式 G-Buffer（+ 复用 Swapchain Depth）：

| 索引 | 资源 | 格式 | 内容 |
|------|------|----|------|
| 0 | Final Color (Swapchain) | Swapchain 格式 | 最终输出 |
| 1 | Albedo G-Buffer | `R8G8B8A8_UNORM` | RGB=BaseColor, A=Metallic |
| 2 | Normal G-Buffer | `R16G16B16A16_SFLOAT` | RGB=世界法线, A=Roughness |
| 3 | Depth | Swapchain Depth (`D32_SFLOAT_S8_UINT`) | 深度（供 Lighting Pass 重投影） |

> **世界坐标不显式存储**，Lighting Pass 通过 `InvViewProj × 深度` 数学重建，节省约 12 字节/像素带宽。

### 6.2 Attachment 布局（RenderPass 视角）

```
Attachment[0]: Final Color   (loadOp=CLEAR, storeOp=STORE, finalLayout=PRESENT_SRC_KHR)
Attachment[1]: Albedo        (loadOp=CLEAR, storeOp=DONT_CARE, finalLayout=COLOR_ATTACHMENT_OPTIMAL)
Attachment[2]: Normal        (loadOp=CLEAR, storeOp=DONT_CARE, finalLayout=COLOR_ATTACHMENT_OPTIMAL)
Attachment[3]: Depth         (loadOp=CLEAR, storeOp=DONT_CARE, finalLayout=DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
```

G-Buffer 的 `storeOp=DONT_CARE` 是刻意设计——在 TBDR GPU（Apple Silicon / Mobile）上，
G-Buffer 可以完全保留在片上 SRAM 中，无需写回主存，极大节省带宽。

### 6.3 Subpass 依赖链

```
EXTERNAL ──[dep0]──► Subpass 0 (Geometry) ──[dep1]──► Subpass 1 (Lighting) ──[dep2]──► EXTERNAL
```

**dep0** (EXTERNAL → Subpass 0):
- src: `BOTTOM_OF_PIPE` / `MEMORY_READ`
- dst: `COLOR_ATTACHMENT_OUTPUT | EARLY_FRAGMENT_TESTS` / `COLOR_ATTACHMENT_WRITE | DEPTH_WRITE`
- flags: `BY_REGION`

**dep1** (Subpass 0 → Subpass 1) ← *核心依赖*:
- src: `COLOR_ATTACHMENT_OUTPUT | LATE_FRAGMENT_TESTS` / `COLOR_WRITE | DEPTH_WRITE`
- dst: `FRAGMENT_SHADER` / `SHADER_READ`
- flags: `BY_REGION` ← 启用 TBDR tile-local 优化

**dep2** (Subpass 1 → EXTERNAL):
- src: `COLOR_ATTACHMENT_OUTPUT` / `COLOR_READ | COLOR_WRITE`
- dst: `BOTTOM_OF_PIPE` / `MEMORY_READ`

### 6.4 Framebuffer 结构

每个 Swapchain Image 对应一个 Framebuffer，共享同一对 G-Buffer Image：

```
Framebuffer[i] = {
    SwapchainImageView[i],  // Attachment 0
    AlbedoImageView,        // Attachment 1 (共享，全帧只有一个)
    NormalImageView,        // Attachment 2 (同上)
    DepthImageView          // Attachment 3 (Swapchain 统一深度)
}
```

### 6.5 Lighting Pipeline 内部描述符布局

Lighting Pass 使用两个描述符集：

```
Set 0 (全局): GlobalSetLayout — SceneDataUBO (ViewProj, InvViewProj, CameraPos, Lights[])
Set 1 (光照): LightingSetLayout — 3x INPUT_ATTACHMENT
  ├── binding 0: subpassInput (Albedo)   → Attachment[1]
  ├── binding 1: subpassInput (Normal)   → Attachment[2]
  └── binding 2: subpassInput (Depth)    → Attachment[3]
```

Lighting Pass 不绑定 VertexBuffer，直接 `vkCmdDraw(3, 1, 0, 0)` 产生一个全屏大三角形，
在顶点着色器 (`deferred_lighting.vert`) 内程序化生成坐标。

### 6.6 录制流程

```
vkBeginCommandBuffer
vkCmdBeginRenderPass (含 4x clearValues)

─── SUBPASS 0: GEOMETRY ───
  vkCmdSetViewport / vkCmdSetScissor
  for each RenderPacket:
    vkCmdBindPipeline (Deferred Geometry 变体)
    vkCmdBindDescriptorSets (Set0: GlobalUBO, Set1: MaterialSet)
    vkCmdPushConstants (TransformMatrix)
    vkCmdBindVertexBuffers / vkCmdBindIndexBuffer
    vkCmdDrawIndexed

─── SUBPASS 1: LIGHTING ───
vkCmdNextSubpass
  vkCmdBindPipeline (LightingPipeline)
  vkCmdBindDescriptorSets (Set0: GlobalUBO, Set1: LightingDescriptorSet)
  vkCmdDraw(3, 1, 0, 0)  ← 全屏三角形

vkCmdEndRenderPass
← 返回，VulkanRenderer 统一调 vkEndCommandBuffer
```

---

## 7. 全局 SceneDataUBO

两条管线共享同一个全局 UBO，由 `VulkanRenderer` 在 `RecordCommandBuffer` 开头更新：

```cpp
struct SceneDataUBO {
    glm::mat4 ViewProjection;       // VP 矩阵
    glm::mat4 InvViewProj;          // 逆 VP（用于深度重投影）
    glm::vec4 CameraPosition;       // w 分量未使用
    LightDataGPU Lights[MAX_SCENE_LIGHTS];
    int LightCount;
    // padding...
};
```

更新方式为 **VMA 持久映射** (Persistently Mapped Buffer via `VMA_MEMORY_USAGE_AUTO_PREFER_HOST`):

```cpp
// 无需 Map/Unmap，直接 memcpy 到已映射地址
memcpy(SceneParameterBuffer.Info.pMappedData, &sceneData, sizeof(SceneDataUBO));
```

---

## 8. UI 渲染回调模式

ImGui 的 UI 渲染通过回调注入，与管线渲染严格解耦：

```
VulkanRenderer::Render(viewData, packets, onUIRender)
  └── RecordCommandBuffer(...)
        ├── Pipelines[path]->RecordCommands(...)   ← vkCmdEndRenderPass 在此结束
        ├── onUIRender(cmdBuffer, imageIndex)       ← UI 在此写入独立 RenderPass
        └── vkEndCommandBuffer(cmdBuffer)           ← 统一结束
```

> **重要**：UI 回调在管线的 `vkCmdEndRenderPass` **之后**、`vkEndCommandBuffer` **之前**执行。
> ImGui 使用自己独立的 `VkRenderPass` 和 `VkFramebuffer`（由 ImGui Vulkan 后端管理），
> 不得混入 Forward/Deferred 的 RenderPass 中。

---

## 9. 描述符集设计总览

| Set | 持有者 | 内容 | 更新频率 |
|-----|--------|------|----------|
| Set 0 | VulkanRenderer (GlobalDescriptorSet) | SceneDataUBO (VP, InvVP, Lights) | 每帧 |
| Set 1 (Forward) | FMaterialInstance | Albedo Tex + MaterialParams UBO | 每材质切换 |
| Set 1 (Deferred Lighting) | FDeferredPipeline (LightingDescriptorSet) | 3x Input Attachment | 初始化 / Swapchain 重建 |

---

## 10. 同步机制

### 10.1 信号量与 Fence 流程

```
CPU:  vkWaitForFences(RenderFence) ──────────────────────────────► 阻塞直到上帧完成
      AcquireNextImage(signal: ImageAvailable)
      RecordCommandBuffer(...)
      vkQueueSubmit(
          wait:   ImageAvailable       ← GPU 等待图像就绪
          signal: RenderFinished
          fence:  RenderFence          ← GPU 完成后通知 CPU
      )
      vkQueuePresent(wait: RenderFinished)
```

### 10.2 Fence vs Semaphore

| 特性 | Fence | Semaphore |
|------|-------|-----------|
| 同步范围 | CPU ↔ GPU | GPU 内部队列间 |
| 重置 | 手动 (vkResetFences) | 自动消耗 |
| 等待侧 | CPU (vkWaitForFences) | GPU Queue |
| 本项目用途 | CPU 等待帧完成 | Image 可用 / 渲染完成通知 present |

---

## 11. 性能优化策略

### 11.1 TBDR 感知带宽优化

- G-Buffer 的 `storeOp=DONT_CARE` + `VK_DEPENDENCY_BY_REGION_BIT`
- 在 Apple Silicon / Adreno / Mali 等 TBDR GPU 上，G-Buffer 永远不离开 tile-local memory
- 有效将 G-Buffer 读写从主存带宽变为 On-Chip 操作（零带宽代价）

### 11.2 Mesh 缓存

`ResourceUploadManager` 内部维护 `unordered_map<FMesh*, FVulkanMesh>` 缓存，
避免每帧重复上传同一 Mesh 到 GPU。

### 11.3 UBO 持久映射

所有频繁更新的 Buffer（SceneDataUBO 等）使用 VMA 持久映射，
每帧仅 `memcpy`，无需 `vkMapMemory/vkUnmapMemory` 开销。

### 11.4 命令缓冲重用

`MainCommandBuffer` 在初始化时分配一次，每帧通过 `vkResetCommandBuffer` 重置重用，
避免分配/释放开销。

---

## 12. Swapchain 重建流程

当窗口大小改变时：

```
SwapChain.Cleanup()
SwapChain.Init(newWidth, newHeight)
for each Pipeline:
    pipeline->RecreateResources(renderer)
        ├── vkDestroyFramebuffer (all)
        ├── DestroyGBuffers (Deferred only)
        ├── InitGBuffers (Deferred only) ← 按新尺寸重建
        └── InitFramebuffers ← 引用新 SwapchainImageViews
vkResetCommandBuffer(MainCommandBuffer)
```

---

## 13. 调试与验证

### 13.1 启用 Vulkan 验证层

在 Debug 模式下，确保验证层已启用：

```cpp
const char* validationLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};
```

### 13.2 常见验证错误对照

| 错误 VUID | 原因 | 解决 |
|-----------|------|------|
| `VUID-VkGraphicsPipelineCreateInfo-renderPass` | Pipeline 创建时的 RenderPass 与 RecordCommands 时不同 | 确保 Pipeline 绑定到其所属 Pipeline 对象的 RenderPass |
| `VUID-vkCmdNextSubpass-commandBuffer-recording` | 在 Recording 状态以外调用 NextSubpass | 确认 vkBeginCommandBuffer 已调用且未提前 End |
| `VUID-vkEndCommandBuffer-commandBuffer-recording` | CB 不在 Recording 状态时调 End | 检查是否有多处 vkEndCommandBuffer 调用（仅 VulkanRenderer 负责此调用） |
| `VUID-vkCmdBindDescriptorSets-pDescriptorSets-00358` | 描述符集布局与 Pipeline Layout 不匹配 | 确认 Set0=GlobalSetLayout, Set1 与对应管线 Layout 一致 |
| `VUID-vkCmdDrawIndexed-indexBuffer-00497` | 索引缓冲未绑定 | 检查 `vkCmdBindIndexBuffer` 调用 |
