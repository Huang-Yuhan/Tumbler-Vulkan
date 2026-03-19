# 引擎架构概览 (Architecture Overview)

本项目作为一个 Vulkan 与现代游戏引擎概念的实践沙盒，在结构上遵循了“逻辑与渲染分离”的核心理念。

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
   渲染器拿着这堆“包裹”，只管往画面上画，完全不知道它们属于哪个“Actor”。

2. **观察者视图 (`SceneViewData`)**
   游戏视角不仅有相机矩阵（View, Projection），还包含了**能被这个相机看到的**所有光源信息（`std::vector<LightData> Lights`）。
   `SceneViewData` 代表环境上下文。如果做分屏双人游戏或阴影映射，我们只需要生成多份不同的 `SceneViewData`，它们依然可以利用同一批 `RenderPacket`。

这种严格的物理层面隔离，为将来的多线程渲染（逻辑线程打包，渲染线程解包提交）打下了稳固的基础。
