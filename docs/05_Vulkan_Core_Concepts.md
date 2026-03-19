# Vulkan 核心概念与工作流机制

与其说 Vulkan 是一个图形 API，不如说它是一个“跨平台的 GPU 控制台”。与 OpenGL 这种高度封装的、基于隐式状态机的古老 API 不同，Vulkan 没有隐式状态、没有全局上下文、更不替你管理内存。你必须极度显式地通过数百行代码声明每一个细节，换来的是极致的多线程能力和最小开销。

## 1. 核心数据结构与金字塔

Vulkan 的架构建立在清晰的依赖链条上，销毁时必须逆序执行：

1. **VkInstance (实例)**：连接应用程序与 Vulkan 驱动的顶层桥梁（带有验证层）。
2. **VkPhysicalDevice (物理设备)**：真实的显卡实体（如 RTX 4090），用来查询其是否支持特定特性。
3. **VkDevice (逻辑设备)**：你与这张显卡交互的具体句柄。大多数对象的创建和销毁依赖此接口。
4. **VkQueue (指令队列)**：显查本身是个异步处理器，有 Graphics, Compute, Transfer 队列。你把指令打包塞给它，它跑它的。

## 2. 显存与资源的绝对把控 (VMA)

在 OpenGL 中，你调用 `glGenBuffers`；在 Vulkan 中，这极其复杂：
1. 调用 `vkCreateBuffer` （只产生了一个句柄，没有内存）。
2. 查询这块 Buffer 需要多少内存结构与对齐字节数要求。
3. 枚举物理显卡的内存堆（Device Local 极速但不让 CPU 碰？还是 Host Visible 让 CPU 能往里 memcpy？）。
4. 向系统申请真正的内存 `vkAllocateMemory`。
5. 将句柄和这块生内存绑定 `vkBindBufferMemory`。

**VMA (Vulkan Memory Allocator)**：为了不被这套反人类逻辑搞死，业界（AMD提供）标准是使用 VMA 库统一接管，通过诸如 `VMA_MEMORY_USAGE_AUTO_PREFER_HOST` 等枚举，由 VMA 帮我们自动寻找和对齐内存块，这就是引擎底层 `CreateBuffer` 的实现基石。

## 3. Shader Variables：Descriptors & Push Constants

Vulkan 怎么把 C++ 的矩阵、贴图、参数传进 GPU 里的 Shader？
Vulkan 中不存在 `glGetUniformLocation`。必须高度显式：

### Push Constants (推送常量)
- **极限高速**：它不像普通内存变量，它其实用的是显卡硬件命令流上极其宝贵的 128/256 字节的寄存器空间。
- **用途**：专供每一帧、每个物体都会频繁剧烈变动的心脏级别小数据。**目前引擎用于存放每个物体的 Model 矩阵 (MVP)**。

### Descriptor Sets (描述符集)
- 对应 Shader 中类似 `layout(set = 0, binding = 1) uniform(...)` 的区域。
- 也就是给“哪块内存/什么图片绑定到哪个槽位”写下一纸证明（需要 `DescriptorPool` 提供内存，`DescriptorSetLayout` 定义规范）。
- **优化设计**：Vulkan 鼓励按频率将资源捆绑。
  - `Set 0 (全局 Set)`：存放 Camera 矩阵、全局所有的光源数据 `SceneDataUBO`，**每帧只绑定更新一次**！
  - `Set 1 (材质 Set)`：存放独属于各材质的 `Albedo Texture` 和 `PBR 参数属性 UBO`。每渲染一个新的实例（物体材质替换）才会重新绑定一次。大大优化性能。

## 4. 渲染管线对象 (VkPipeline)

Vulkan 将 Shader代码、图元拓扑方式(三角形)、深度测试开关、混合开关、多重采样参数、颜色格式等一系列状态在启动时就 **完全烘焙死 (Bake)** 成了不可变的全局常量状态汇总：`VkPipeline`。

- 在 `FMaterial` 创建子实例前，我们调用 `vkCreateGraphicsPipelines` 申请的就是这玩意。
- 因为它是死状态（ Immutable ），意味着显卡驱动可以在游戏主线加载时对它进行终极硬件优化。而在运行时，你不能再像 OpenGL 那样随便调用 `glDisable(GL_DEPTH_TEST)`；你要改深度测试？很抱歉，请重新走流程创建一条新的完全烘焙完毕的 `VkPipeline` 并在运行时切换过去。

## 5. 命令池与命令缓冲 (CommandPool & CommandBuffer)

Vulkan 要求一切指令在 **CommandBuffer (命令缓冲)** 里被录制后，作为一个大包裹提交。
在 `VulkanRenderer::RecordCommandBuffer` 里，你能清晰看到流程：
1. `vkBeginCommandBuffer` （开始录影）
2. `vkCmdBeginRenderPass` （开始清理屏幕并建立附件）
3. `vkCmdBindPipeline` （大喊：下面的行为全按这个 FMaterial 管线的规矩画！）
4. `vkCmdBindDescriptorSets` （把全局的摄像机镜头 Set 0、某个特定模型的具体属性 Set 1 怼进机器）
5. `vkCmdPushConstants` （把该模型的 Transform 指令光速射上电梯）
6. `vkCmdDrawIndexed` （画出来！）
7. `vkEndCommandBuffer` （停止录影）。

这就是为什么我们重构实现了 `AppLogic` → `RenderPacket` ：打包完毕的数据才能被最清爽干净地填入这段流水线中！
