# 引擎架构概览

本项目是纯 GPU-Driven 渲染引擎，核心理念：**渲染决策发生在 GPU 上，CPU 只负责上传数据和编排 Pass**。

## 1. 场景管理（无 ECS）

场景不依赖传统的 ECS 系统。`Scene` 直接维护 `vector<ObjectData>`（CPU 副本），每个 Object 包含：

- `mat4 modelMatrix` — 世界变换
- `uint meshIndex` — 索引到 GPU 端 MeshDataSSBO
- `uint materialIndex` — 索引到 GPU 端 MaterialDataSSBO

通过 Dirty 标记机制，只同步变更的 Object 到 GPU Buffer，避免每帧全量上传。

## 2. 逻辑与渲染的物理隔离

渲染器 (`Renderer`) 只消费 GPU Buffer 中的数据，不访问任何 CPU 侧游戏对象：

1. **`Scene::SyncToGPU()`** — 将 Dirty Object 写入 `ObjectDataSSBO`
2. **`GPUScene`** — 管理三个 GPU SSBO：ObjectData / MeshData / MaterialData
3. **`Renderer::Render()`** — Compute Culling → Shadow Depth → GBuffer → Lighting → UI

## 3. 渲染器子系统

```
Renderer (协调者)
├── VulkanContext          — Instance、Device、VMA、队列族
├── VulkanSwapchain        — 交换链图像、深度缓冲、重建
├── RenderDevice           — GPU 资源创建/销毁（VMA 封装）
├── CommandManager         — CommandPool、即时提交、通用布局转换
├── ResourceManager        — 统一 VB/IB 分配、Mesh/纹理上传与索引化
├── DescriptorManager      — Set 0 (Global) + Set 1 (Bindless 纹理数组)
├── GPUScene               — ObjectDataSSBO + MeshDataSSBO + MaterialDataSSBO
├── IndirectDraw           — Indirect Command Buffer 管理 + 每帧重置
├── CullingPass            — Compute Shader Frustum Culling
├── DepthPass              — Shadow Map 深度 Pass (Indirect Draw)
├── GBufferPass            — MRT G-Buffer Pass (Indirect Draw)
└── LightingPass           — 全屏 Deferred Lighting
```

## 4. 渲染管线（纯 Deferred，无 Forward）

单一路径，不使用策略模式。每帧 GPU 管线：

```
Compute  │ Frustum Culling → 填充 IndirectDraw + VisibleCount
Barrier  │
Graphics │ Shadow Depth Pass    → vkCmdDrawIndexedIndirect
Graphics │ GBuffer Pass (MRT)   → vkCmdDrawIndexedIndirect
Graphics │ Lighting Pass        → vkCmdDraw(3)  全屏三角形
Graphics │ ImGui Pass
```

### G-Buffer 格式

| Attachment | Format | 通道 |
|------------|--------|------|
| 0 (Albedo) | `R8G8B8A8_UNORM` | RGB = Albedo |
| 1 (Normal+Roughness+Metallic) | `R16G16B16A16_SFLOAT` | RG = Oct Normal, B = Roughness, A = Metallic |
| 2 (Depth) | Swapchain Depth Format | 复用 Swapchain Depth |

## 5. Descriptor Set 绑定模型

```
Set 0 (Global, 每帧绑定一次):
  binding 0: SceneUBO        (ViewProj, CameraPos, LightData, ShadowMatrix)
  binding 1: sampler2DShadow (Shadow Map)

Set 1 (Bindless, 初始化时写入):
  binding 0: texture2D[]     (所有纹理的数组, 最多 1024)
  binding 1: MaterialData[]  (SSBO, 材质参数数组)
  binding 2: ObjectData[]    (SSBO, Transform + MeshIndex + MaterialIndex)
```

## 6. GPU 数据布局

```
ObjectDataSSBO (Host-Visible, Persistent Mapped):
  mat4 modelMatrix / uint meshIndex / uint materialIndex / padding

MeshDataSSBO (Device-Local):
  uint64_t vertexAddress / uint64_t indexAddress /
  uint indexCount / uint vertexCount /
  vec4 boundingSphere

MaterialDataSSBO (Host-Visible):
  uint albedoTexIndex / uint normalTexIndex /
  uint metallicRoughnessTexIndex /
  float roughness / float metallic / vec4 baseColorTint
```

## 7. 资源管理

`ResourceManager` 采用 Unified Buffer 策略：
- 一个大 Vertex Buffer（如 64MB）+ 一个大 Index Buffer（如 16MB）
- Mesh 上传时 sub-allocate 并记录 offset 和 `VkDeviceAddress`
- GPU Culling Shader 通过 `MeshDataSSBO[meshIndex]` 直接获取 GPU 地址

纹理同样索引化：加载时分配 `texIndex`，写入 Bindless 纹理数组。

## 8. Vulkan 1.2 特性需求

- `bufferDeviceAddress` — GPU 直接访问 VB/IB
- `descriptorIndexing` — Bindless 纹理数组 + SSBO 数组
- `drawIndirectCount` — Multi-Draw Indirect

## 9. 编辑器与调试

- `UIManager` — ImGui GLFW/Vulkan 后端
- `DebugUI` — 性能面板 (FPS/FrameTime/DrawCalls/VisibleObjects)、Shadow Map 预览

## 10. 设计原则

| 原则 | 实践 |
|------|------|
| GPU-Driven | 渲染决策（Culling、Draw Call 生成）全部在 GPU |
| 纯 Deferred | 单一路径，无 Forward |
| Bindless | 一个 DescriptorSet 访问所有纹理和材质 |
| Unified Buffer | 全局 VB/IB，一次绑定，Indirect Draw |
| 单一职责 | 每个子系统只负责一个领域 |
| 逻辑渲染分离 | Renderer 不访问 Scene 对象，只消费 GPU Buffer |
