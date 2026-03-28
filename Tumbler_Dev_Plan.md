# 🚀 Tumbler Engine 开发路线图 (Roadmap)

本项目旨在从零构建一个现代化的 Vulkan 渲染引擎。当前已完成底层渲染基础设施的搭建与核心架构的解耦，接下来的开发重心将是**突破单光限制**，并向真正的工业级**延迟渲染管线 (Deferred Rendering Pipeline)** 演进。

---

## ✅ 已完成里程碑 (Completed Milestones)

- [x] **基础设施解耦**: 成功剥离 `RenderDevice`、`CommandBufferManager` 和 `ResourceUploadManager`。
- [x] **数据流隔离**: 实现 `SceneViewData` 与 `RenderPacket`，完成逻辑层与渲染层的物理隔离。
- [x] **健壮性提升**: 完善交换链重建（Swapchain Recreation）逻辑，处理窗口缩放带来的崩溃问题。
- [x] **基础 PBR 渲染**: 完成基于 Cook-Torrance BRDF 的基础物理渲染管线（前向渲染，单光源）。

---

## 🚀 后续开发路线图 (Roadmap)

### 阶段一：基础多光源系统 (Priority: High)
*目标：作为前向渲染的最后一搏，跑通光照累加的数学逻辑与 UBO 数组传递。*

- [x] **C++ 端 UBO 改造**
  - 在 `SceneDataUBO` 中引入固定大小的光源数组（如 `LightData[8]`）。
  - 增加 `int LightCount` 字段传递当前实际光源数量。
  - 处理 Vulkan `std140` 内存对齐问题（使用 `glm::vec4` 传递位置与颜色数据）。
- [x] **Shader 端重构**
  - 更新 `pbr.frag`，提取光照计算逻辑为独立函数。
  - 引入 `for` 循环，累加所有激活光源的漫反射与高光贡献。

### 阶段二：Transform 层级树构建 (Priority: High)
*目标：建立完善的场景节点图，支持父子层级联动，为后续复杂场景打基础。*

- [x] **数据结构升级**
  - 在 `CTransform` 组件中引入 `CTransform* Parent` 和 `std::vector<CTransform*> Children`。
- [x] **矩阵分离与级联更新**
  - 分离 `LocalMatrix` 和 `WorldMatrix`。
  - 实现层级变换逻辑：`WorldMatrix = Parent->WorldMatrix * LocalMatrix`。
  - 引入 `isDirty` 标记，避免每帧无意义的矩阵重算。

### 阶段三：核心架构跃迁 —— 延迟渲染管线 (Priority: Highest) 🔥
*目标：重构 Vulkan 渲染通道，彻底解除光源数量的性能瓶颈。*

- [ ] **G-Buffer 设计与创建**
  - 设计合理的 G-Buffer 布局（例如：Albedo+Roughness, Normal+Metallic, Depth）。
  - 在 Vulkan 中分配对应的图像资源与 Framebuffer 附件。
- [ ] **Vulkan Subpass 配置**
  - 重构 `VkRenderPass`，配置两个 Subpass：
    - **Subpass 0 (Geometry Pass)**：渲染不透明物体，将材质属性写入 G-Buffer。
    - **Subpass 1 (Lighting Pass)**：使用 `Input Attachment` 读取 G-Buffer，进行全屏光照计算。
- [ ] **Shader 重写**
  - 编写新的 `geometry.frag` 输出多目标渲染 (MRT)。
  - 编写新的 `lighting.frag` 进行全屏 Quad 绘制与 PBR 能量累加。

### 阶段四：高级视觉效果与阴影 (Priority: Medium)
*目标：在 G-Buffer 的加持下，实现更高级的屏幕空间特效与物理阴影。*

- [ ] **方向光阴影映射 (Shadow Mapping)**
  - 增加一个专属的 Depth Pass 生成 Shadow Map。
  - 在 Lighting Pass 中采样 Shadow Map 实现 PCF 软阴影。
- [ ] **屏幕空间环境光遮蔽 (SSAO)**
  - 利用 G-Buffer 的深度和法线，计算屏幕空间的遮蔽率，增强角落与裂缝的物理真实感。

### 阶段五：资源异步加载机制 (Priority: Medium)
*目标：消除大型模型或高清贴图加载时的线程阻塞与画面卡顿。*

- [ ] **专属传输队列**
  - 在 Vulkan 初始化时分离出专门的 Transfer Queue。
- [ ] **多线程解码与 GPU 同步**
  - 使用 `std::thread` 在后台解析模型与图片，写入 Staging Buffer。
  - 提交到 Transfer Queue，利用 `VkFence` 监控完成状态，无缝挂载到场景中。

---

## 🛠️ 间隙小任务与调试工具 (Warm-up & Debugging)

作为硬核图形开发过程中的调剂，随时加入以下辅助工具提升 Debug 效率：

- [ ] **法线可视化 (Normal Debugging)**：编写一个 Line Pass，可视化模型的顶点法线和切线。
- [ ] **线框模式 (Wireframe Mode)**：利用 `VK_POLYGON_MODE_LINE` 创建 Pipeline 变体，并在 ImGui 中一键切换。
- [ ] **ImGui 渲染调试面板**：
  - 增加 G-Buffer 实时预览小窗口（直接查看法线图、深度图）。
  - 基础的 FPS、Draw Call 和光源数量统计。