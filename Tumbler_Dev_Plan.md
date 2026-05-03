# 🚀 Tumbler Engine 开发路线图

## ✅ 已完成

- 基础设施解耦（RenderDevice / CommandBufferManager / ResourceUploadManager）
- 数据流隔离（SceneViewData + RenderPacket，逻辑与渲染物理分离）
- 前向 PBR 多光源 + 延迟渲染双管线（G-Buffer / Subpass / Lighting Pass / 热切换）
- 材质母体-实例模式 + 双管线自动编译
- Swapchain 重建闭环 + UI Framebuffer 自动重建
- 描述符延迟回收 + DescriptorSetFreeQueue
- 稳定性测试：Resize 压力 / Descriptor 压力 / 隐藏窗口 Smoke / 产物 & 管线钩子检查
- CI 门禁（Windows/MSVC configure + build + ctest）
- 运行时控制台（历史/Tab 补全/命令注册/可配置切换键）
- G-Buffer 调试预览（Albedo + Normal，Deferred 模式）
- 设计审查：P0-P3 共 24 项修复，Review 文档见 `Design_Review.md`
- 文档重构：入门 / 架构 / 指南 / 参考 四层结构

---

## 🎯 当前执行计划

1. **Core 化调试窗口框架** ✅
   - `DebugWindowHost` 已下沉到 Core/Editor，提供 section 注册 + 优先级排序 + CollapsingHeader 渲染
   - AppLogic 通过 lambda 注册 5 个 section（Performance / Camera / Lighting / Rendering / G-Buffer）
   - G-Buffer 预览（Albedo + Normal）仅在 Deferred 模式显示

2. **进入高级视觉效果**
   - 先做方向光阴影（Shadow Mapping），再做 SSAO
   - Shadow Mapping 是更基础的光照能力，SSAO 适合作为后续增强

3. **资源异步加载**
   - 分离 Transfer Queue + 后台线程上传
   - 系统跨度大，放在回归能力完善之后推进

---

## 🚀 后续路线图

### 阶段三后继：Core 化调试窗口 (Priority: High)

- [x] **Core 调试窗口框架**
  - `DebugWindowHost`（Core/Editor）提供 section 注册 + 优先级排序 + CollapsingHeader 渲染
  - 框架不依赖 Example 层
- [x] **Core 通用诊断 Section**
  - Performance / Rendering / G-Buffer 三个通用 section 已实现
  - G-Buffer 预览（Albedo + Normal）仅在 Deferred 模式显示
- [x] **Tumbler 专属 Section 绑定**
  - Camera / Lighting 通过 `RegisterDebugSections()` lambda 注册到 `DebugWindowHost`
  - Scene Hierarchy / Inspector 保留为独立窗口

### 阶段四：高级视觉效果与阴影 (Priority: Medium)

- [ ] **方向光阴影映射 (Shadow Mapping)**
  - 单方向光 + 单张 Shadow Map 最小闭环
  - 专属 Depth Pass、Shadow Map 资源、Light View Projection
  - Lighting Pass 中接入采样，PCF 软阴影作为第二步
  - 必须配套最小调试预览能力
- [ ] **屏幕空间环境光遮蔽 (SSAO)**
  - Shadow Mapping 跑稳之后再推进
  - 利用 G-Buffer 深度和法线计算遮蔽率
  - 最终效果需能通过调试面板观察 raw AO 和合成结果

### 阶段五：资源异步加载 (Priority: Medium)

- [ ] **专属传输队列**：Vulkan 初始化时分离 Transfer Queue
- [ ] **多线程解码与 GPU 同步**：`std::thread` 后台解析 + Staging Buffer → Transfer Queue → Fence 监控完成

---

## 🛠️ 小任务与调试工具

- [ ] **法线可视化**：Line Pass 可视化顶点法线和切线
- [ ] **线框模式**：`VK_POLYGON_MODE_LINE` Pipeline 变体 + ImGui 切换
- [ ] **更完整的渲染统计**：CPU/GPU 帧耗时、材质实例数量、可见对象数量
