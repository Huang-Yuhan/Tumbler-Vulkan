# Tumbler Vulkan 渲染架构

Tumbler 已经从早期的单体式前向渲染器，演进为一个支持 **Forward** 与 **Deferred** 两条路径的**多态双管线系统**。  
这套架构的核心目标，是让渲染主流程在桌面与移动平台上都能保持较好的扩展性，并尽可能利用 Vulkan 的现代特性，例如 **Subpass** 与 **Input Attachment**，来降低显存带宽压力。

## 1. 双管线策略模式

引擎通过 `IRenderPipeline` 接口实现策略模式，`VulkanRenderer` 本身不再硬编码某一种渲染实现，而是作为统一协调者，持有所有渲染管线实例，并在运行时把命令录制工作分发给当前选中的管线：

```cpp
std::unordered_map<ERenderPath, std::unique_ptr<IRenderPipeline>> Pipelines;
```

每一帧中，`SceneViewData::RenderPath` 会决定当前使用哪一条渲染路径。两条管线都会在启动时初始化完成，因此它们可以在运行时热切换。

- **`FForwardPipeline`**
  传统的单 Pass 渲染架构。几何与光照在同一个片元着色器 `pbr.frag` 中完成。  
  结构上只有一个 Subpass，使用 `1 个颜色附件 + 1 个深度附件`。

- **`FDeferredPipeline`**
  将几何写入与光照计算拆开。整个渲染过程放在一个包含 **2 个 Subpass** 的 `VkRenderPass` 中：
  - **Subpass 0：Geometry Pass**
    把材质和几何信息写入 G-Buffer
  - **Subpass 1：Lighting Pass**
    通过 Vulkan 的 `subpassInput` 从 G-Buffer 读取数据，并执行全屏光照累加

## 2. 延迟渲染的内存与带宽优化

Deferred 管线并没有采用那种“位置、法线、颜色全部展开写满”的重型 G-Buffer 方案，而是有意识地压缩了显存占用，以减少标准延迟渲染中常见的显存膨胀与带宽瓶颈。

### 2.1 G-Buffer 分配方式

在 **Subpass 0：Geometry Pass** 中，只显式分配了 **两个颜色附件**：

1. **Albedo Buffer**：`R8G8B8A8_UNORM`
   - `R / G / B`：基础颜色
   - `A`：`Metallic`

2. **Normal Buffer**：`R16G16B16A16_SFLOAT`
   - `R / G / B`：世界空间法线
   - `A`：`Roughness`

3. **Depth Buffer**
   - 复用交换链相关的深度资源
   - 世界空间位置 **不会直接写入 G-Buffer**
   - Lighting Pass 会通过深度值和 `SceneDataUBO` 中的 `InvViewProj` 矩阵进行反投影，恢复世界坐标

这种设计的关键点在于：**能算出来的数据就不额外存**。  
位置不是一个单独的 G-Buffer 附件，而是通过深度重建，从而省掉一个代价很高的 Position Buffer。

### 2.2 Subpass 依赖与 Input Attachment

`FDeferredPipeline` 并没有把 G-Buffer 先写出到显存，再在下一个独立 RenderPass 中当纹理读回来。  
相反，它把 Geometry Pass 和 Lighting Pass 放进了**同一个 `VkRenderPass` 的两个 Subpass** 中。

Lighting Pass 使用的是 Vulkan 的 `subpassInput` 机制，也就是 `VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT`：

- G-Buffer 作为输入附件直接在下一个 Subpass 中读取
- 配合 `VK_DEPENDENCY_BY_REGION_BIT` 的子通道依赖配置
- 在 Tile-Based Deferred Rendering（TBDR）GPU 上，例如移动芯片或 Apple Silicon，这些中间结果有机会一直停留在更快的片上存储中，而不是频繁往返主显存

此外，G-Buffer 附件统一使用：

```cpp
storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE
```

这表示这些中间结果不会在 RenderPass 结束后继续保留。  
从驱动角度看，这是一个非常明确的优化信号：这些附件只是中间计算产物，不值得额外写回 VRAM。

## 3. 动态材质双路径编译

由于 Forward 和 Deferred 两条渲染路径在 Shader 结构上并不相同，`FMaterial` 负责把这种差异封装起来，对上层屏蔽实现细节。

材质系统的行为可以概括为：

- 一个母体材质在可用情况下，会同时为 **Forward** 和 **Deferred** 两条路径编译对应的 `VkPipeline`
- Deferred 的 Geometry Pass 会自动关闭颜色混合
- `colorWriteMask` 和打包输出格式由 `VulkanPipelineBuilder` 统一配置，确保写入 G-Buffer 时不会破坏打包后的浮点或材质参数

运行时，`AppLogic` 通过 `SceneViewData` 中的 `ERenderPath` 指定当前渲染模式，材质系统则根据这个枚举自动绑定正确的 `VkPipeline` 变体。

换句话说，上层逻辑只关心“我要用 Forward 还是 Deferred”，而不必自己处理每种材质在不同管线下的 Vulkan 细节。

## 4. 命令缓冲的生命周期

`VulkanRenderer` 对主命令缓冲 `MainCommandBuffer` 拥有完整控制权，每帧的生命周期是固定的：

1. `vkResetCommandBuffer`
   - 每帧开始时重置命令缓冲

2. `IRenderPipeline::RecordCommands()`
   - 在这里调用 `vkBeginCommandBuffer`
   - 录制场景绘制指令
   - 结束对应的 RenderPass
   - **不会在这里调用 `vkEndCommandBuffer`**

3. `onUIRender(cmdBuffer, imageIndex)`
   - 场景渲染结束后，执行 UI 回调
   - ImGui 在自己的 Vulkan RenderPass 中录制命令

4. `vkEndCommandBuffer`
   - 只由 `VulkanRenderer` 统一调用一次

这样做的好处是：

- 整个场景渲染与 UI 渲染共享同一个命令缓冲录制阶段
- 命令缓冲只会被结束一次
- 可以避免诸如 `VUID-vkEndCommandBuffer-commandBuffer-recording` 之类的验证层错误

这本质上是把“谁负责录制场景”与“谁负责关闭命令缓冲”分开，避免多个系统重复接管同一个 Vulkan 生命周期对象。

## 5. UI 渲染解耦

ImGui 渲染已经和场景渲染主通路解耦。  
`VulkanRenderer::Render()` 提供了一个 `onUIRender` 回调，让 UI 可以在场景绘制结束后插入自己的录制阶段：

```cpp
void VulkanRenderer::Render(
    const SceneViewData& viewData,
    const std::vector<RenderPacket>& renderPackets,
    std::function<void(VkCommandBuffer, uint32_t)> onUIRender = nullptr
);
```

这个回调会收到两个参数：

- `VkCommandBuffer`
- `imageIndex`

其中 `imageIndex` 对 ImGui Vulkan 后端非常重要，因为它需要根据当前交换链图像选择正确的 framebuffer。

UI RenderPass 由 ImGui 相关系统自行管理，`VulkanRenderer` 只提供一个明确的录制插槽：

- 场景管线先完成自己的 RenderPass
- 然后由 UI 系统把 ImGui 绘制命令录入同一个命令缓冲

这种设计的结果是：

- 场景渲染与 UI 渲染之间职责边界更清晰
- Forward / Deferred 管线本身不需要关心 ImGui
- UI 系统也不需要侵入场景渲染内部逻辑

## 6. 总结

当前的渲染架构可以概括成下面几条原则：

- **渲染路径多态化**：Forward 与 Deferred 通过统一接口共存
- **G-Buffer 压缩化**：只存真正必要的数据，位置通过深度重建
- **带宽友好**：尽量利用 Subpass 与 Input Attachment 避免中间结果反复出入显存
- **材质系统自动适配双路径**：上层不直接处理不同管线差异
- **命令缓冲生命周期集中管理**：减少 Vulkan 使用错误
- **UI 与场景绘制解耦**：让编辑器能力与渲染主链路保持清晰边界

这套设计已经不只是“能画出来”，而是在朝着一个**可维护、可切换、可扩展**的现代 Vulkan 渲染框架发展。
