# 🚀 Tumbler Engine 开发路线图 (Roadmap)

本项目旨在从零构建一个现代化的 Vulkan 渲染引擎。当前已完成前向多光源与延迟渲染主通路，接下来的开发重心不是继续盲目堆功能，而是先把稳定性、回归防护和调试抓手做厚，再推进高级视觉效果。

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

## 🎯 当前执行计划 (Current Execution Plan)

为了避免后续的阴影、SSAO、异步加载把现有渲染主链路再次带崩，当前推荐按下面顺序推进：

1. **补完稳定性与回归测试**
   - 优先落地 resize 压力测试与 descriptor 高频创建/销毁测试。
   - 目标是先把 swapchain、UI framebuffer、descriptor 生命周期的回归风险压下去。
2. **建立最小隐藏窗口渲染 smoke + 离屏快照基线**
   - 先基于当前窗口驱动架构，建立可自动运行的隐藏窗口渲染 smoke。
   - 再给 forward / deferred 各建立一条可自动验证的离屏图像基线。
   - 目标是后续加阴影或 SSAO 时不再只能靠肉眼对比，也不误把“true headless”当成近期小任务。
3. **补齐调试面板**
   - 优先做 G-Buffer 预览、FPS、Draw Call、光源数量统计。
   - 目标是为 Shadow Mapping 和 SSAO 提供足够的可视化诊断能力。
4. **进入高级视觉效果**
   - 先做方向光阴影，再做 SSAO。
   - 原因是 Shadow Mapping 是更基础的光照能力，SSAO 更适合作为后续增强项。
5. **最后再做异步加载**
   - 这部分系统跨度大，适合放在回归能力完善之后推进。

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
  - [x] **Resize 压力测试**
    - `App-Tumbler` 已增加 `--resize-stress-test` 模式，可自动连续切换窗口尺寸并在固定帧数后退出。
    - 已验证 swapchain 重建、UI Framebuffer 重建、Lighting Descriptor 更新链路在高频 resize 下稳定。
    - 已接入默认关闭的 `TUMBLER_ENABLE_RUNTIME_SMOKE_TESTS` 选项，可注册 `Smoke.ResizeStressRuntime` 到 CTest。
  - [x] **Descriptor 高频创建/销毁测试**
    - 已为 descriptor 延迟释放队列补纯逻辑单元测试，覆盖空句柄忽略、重复入队去重、顺序与清空语义。
    - `App-Tumbler` 已增加 `--descriptor-stress-test` 模式，可批量创建、更新、释放材质实例并在多轮渲染中自动退出。
    - 已接入默认关闭的 `Smoke.DescriptorStressRuntime`，用于运行时 descriptor 分配/释放压力回归。
  - [ ] **隐藏窗口渲染 Smoke Test**
    - 基于当前 `AppWindow -> Surface -> Swapchain` 启动链路运行固定场景若干帧，但不要求显示正常交互窗口。
    - 目标是验证初始化、渲染、退出、资源释放链路稳定，而不是一步到位做 true headless。
    - 首版至少要能在本地自动运行，并具备接入 CTest 的能力。
  - [ ] **离屏渲染一致性快照**
    - 为 forward / deferred 各建立至少一条固定场景的离屏渲染基线。
    - 首版可以从 hash、关键像素或低分辨率容差比对起步，不要求一步到位做复杂图像 diff。
    - UI 不应参与截图基线，避免把编辑器噪声混进结果。
    - 要求能在本地和 CI 上稳定执行，避免只在开发机可用。
  - [ ] **验收标准**
    - resize 压力运行过程中无崩溃、无卡死、无明显资源重建遗漏。
    - descriptor 压力测试后，场景删除和材质实例析构不触发断言或资源泄漏异常。
    - 隐藏窗口 smoke 能稳定跑完固定帧数并正常退出。
    - 快照测试能稳定区分“渲染逻辑未变”和“渲染结果发生回归”。

### 阶段三点七：调试可视化与诊断能力 (Priority: High)
*目标：在推进阴影和屏幕空间特效之前，先补够渲染诊断抓手。*

- [ ] **ImGui 渲染调试面板**
  - 增加 G-Buffer 预览：至少包含法线图、深度图，必要时补 Albedo。
  - 增加实时统计：FPS、Draw Call、光源数量、当前 Render Path。
  - 为后续阴影贴图和 SSAO 预留调试窗口结构，避免后面再拆 UI。
- [ ] **管线切换与观察辅助**
  - 结合现有控制台或编辑器 UI，增加更直接的 render path 观察入口。
  - 视情况补充线框模式或法线可视化，作为轻量级 debug pass。
- [ ] **验收标准**
  - 常见渲染问题能通过 UI 直接观察到，而不必每次下断点或抓 RenderDoc。
  - 调试面板本身不会破坏正常渲染路径和 swapchain 重建流程。

### 阶段四：高级视觉效果与阴影 (Priority: Medium)
*目标：在 G-Buffer 的加持下，实现更高级的屏幕空间特效与物理阴影。*

- [ ] **方向光阴影映射 (Shadow Mapping)**
  - 先做单方向光 + 单张 Shadow Map 的最小闭环版本。
  - 增加专属 Depth Pass、Shadow Map 资源和 Light View Projection 计算。
  - 在 Lighting Pass 中接入 Shadow Map 采样，PCF 软阴影可作为第二步优化。
  - 必须配套最小调试预览能力，避免阴影错误时完全黑盒。
- [ ] **屏幕空间环境光遮蔽 (SSAO)**
  - 在 Shadow Mapping 跑稳之后再推进，不与阴影实现并行展开。
  - 利用 G-Buffer 深度和法线计算遮蔽率，增强角落与缝隙细节。
  - 最终效果需要能通过调试面板观察 raw AO 和合成结果。

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
- [ ] **更完整的渲染统计**
  - 在已有调试面板基础上继续补充 CPU/GPU 帧耗时、材质实例数量、可见对象数量等指标。
