# 🚀 Tumbler Engine 开发路线图 (Roadmap)

本项目旨在从零构建一个现代化的 Vulkan 渲染引擎。当前已完成前向多光源与延迟渲染主通路，接下来的开发重心是把稳定性和可回归性持续做厚，再推进高级视觉效果。

---

## ✅ 已完成里程碑 (Completed Milestones)

- [x] **基础设施解耦**: 成功剥离 `RenderDevice`、`CommandBufferManager` 和 `ResourceUploadManager`。
- [x] **数据流隔离**: 实现 `SceneViewData` 与 `RenderPacket`，完成逻辑层与渲染层的物理隔离。
- [x] **前向 PBR 多光源**: 完成 `SceneDataUBO` 光源数组与 Shader 端光照累加。
- [x] **延迟渲染主通路**: 完成 G-Buffer、双 Subpass、Lighting Pass 与输入附件采样。
- [x] **交换链与 UI 稳定性**: 完善 swapchain 重建链路，并在附件变化时自动重建 UI Framebuffer。
- [x] **描述符生命周期回收**: 支持 descriptor set 延迟回收，避免材质实例频繁重建导致泄漏/耗尽。
- [x] **Smoke Test 基础覆盖**: 接入 CTest，覆盖资源、Shader 编译产物与示例程序产物检查。

---

## 🚀 后续开发路线图 (Roadmap)

### 阶段一：基础多光源系统 (Priority: High)
*目标：作为前向渲染阶段的收口，跑通光照累加数学逻辑与 UBO 数组传递。*

- [x] **C++ 端 UBO 改造**
  - 在 `SceneDataUBO` 中引入固定大小光源数组（如 `LightData[8]`）。
  - 增加 `LightCount` 字段传递当前实际光源数量。
  - 处理 Vulkan `std140` 内存对齐问题。
- [x] **Shader 端重构**
  - 更新 `pbr.frag`，提取光照计算逻辑为独立函数。
  - 引入 `for` 循环，累加所有激活光源的漫反射与高光贡献。

### 阶段二：Transform 层级树构建 (Priority: High)
*目标：建立完善的场景节点图，支持父子层级联动，为复杂场景打基础。*

- [x] **数据结构升级**
  - 在 `CTransform` 组件中引入 `Parent` 与 `Children`。
- [x] **矩阵分离与级联更新**
  - 分离 `LocalMatrix` 和 `WorldMatrix`。
  - 实现层级变换逻辑：`WorldMatrix = Parent->WorldMatrix * LocalMatrix`。
  - 引入 `isDirty` 标记，避免每帧无意义矩阵重算。

### 阶段三：核心架构跃迁 —— 延迟渲染管线 (Priority: Highest) 🔥
*目标：重构 Vulkan 渲染通道，降低多光源场景下的性能瓶颈。*

- [x] **G-Buffer 设计与创建**
  - 实现 Albedo / Normal / Depth 等 G-Buffer 附件资源管理。
  - 完成对应 Framebuffer 与重建逻辑。
- [x] **Vulkan Subpass 配置**
  - 重构 `VkRenderPass`，配置 Geometry Pass + Lighting Pass 两个 Subpass。
  - 在 Lighting Pass 中通过 `Input Attachment` 读取 G-Buffer。
- [x] **Deferred Shader 主链路**
  - 落地 `deferred_geometry.frag` 与 `deferred_lighting.frag/.vert`。
  - 完成全屏光照累加与场景光源 UBO 接入。

### 阶段三点五：稳定性与回归防护 (Priority: Highest)
*目标：把“能跑”升级为“稳定可维护”，降低后续迭代回归风险。*

- [x] **Swapchain 重建闭环**
  - 延迟管线在资源重建后刷新 Lighting Descriptor Set。
  - UI 在 swapchain 附件/分辨率变化时自动重建 Framebuffer。
- [x] **Descriptor 生命周期治理**
  - Descriptor pool 支持 set 释放。
  - 材质实例析构时排队回收，渲染器在安全点统一释放。
- [x] **Smoke Tests**
  - 已覆盖资源存在性、Shader `.spv` 产物、示例可执行产物。
  - 已覆盖延迟渲染关键产物与关键管线钩子存在性检查。
- [x] **CI 合并门禁**
  - GitHub Actions 已接入 Windows/MSVC 流水线，自动执行 configure + build + ctest。
  - PR 与 `main` 推送都会触发，失败时自动上传测试日志。
- [ ] **下一步测试增强**
  - 增加 headless 渲染一致性快照（如 RenderDoc/离屏比对）。
  - 增加 resize 压力测试与 descriptor 高频创建/销毁测试。

### 阶段四：高级视觉效果与阴影 (Priority: Medium)
*目标：在 G-Buffer 的加持下，实现更高级的屏幕空间特效与物理阴影。*

- [ ] **方向光阴影映射 (Shadow Mapping)**
  - 增加专属 Depth Pass 生成 Shadow Map。
  - 在 Lighting Pass 中采样 Shadow Map 实现 PCF 软阴影。
- [ ] **屏幕空间环境光遮蔽 (SSAO)**
  - 利用 G-Buffer 深度和法线计算遮蔽率，增强角落与缝隙细节。

### 阶段五：资源异步加载机制 (Priority: Medium)
*目标：消除大型模型或高清贴图加载时的线程阻塞与画面卡顿。*

- [ ] **专属传输队列**
  - 在 Vulkan 初始化时分离专门的 Transfer Queue。
- [ ] **多线程解码与 GPU 同步**
  - 使用 `std::thread` 后台解析模型与图片并写入 Staging Buffer。
  - 提交到 Transfer Queue，利用 `VkFence` 监控完成状态并挂载到场景。

---

## 🛠️ 间隙小任务与调试工具 (Warm-up & Debugging)

作为图形开发过程中的调剂，可随时加入以下辅助工具提升 Debug 效率：

- [ ] **法线可视化 (Normal Debugging)**：编写 Line Pass，可视化模型顶点法线和切线。
- [ ] **线框模式 (Wireframe Mode)**：利用 `VK_POLYGON_MODE_LINE` 创建 Pipeline 变体，并在 ImGui 中切换。
- [ ] **ImGui 渲染调试面板**：
  - 增加 G-Buffer 实时预览（法线图、深度图）。
  - 增加 FPS、Draw Call、光源数量等基础统计。
