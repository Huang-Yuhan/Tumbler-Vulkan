# 🚀 Tumbler Engine 开发路线图 (Roadmap)

**当前状态**：✅ **优先级 1 完成！准备进入视觉效果升级期**

经过阶段 1~10 的打磨，Tumbler 引擎已具备：
✅ 基于实体-组件 (ECS雏形) 的逻辑架构
✅ 严格物理隔离的 `SceneViewData` 与 `RenderPacket` 数据流
✅ 基于资源缓存与依赖注入的 `FAssetManager`
✅ 高效的 Vulkan PBR 材质母体/实例管线绑定模型
✅ 包含环境采样、GGX微表面、Schlick菲涅尔的 Cook-Torrance BRDF Shader
✅ **完整的输入系统与漫游相机** (`InputManager` + `CFirstPersonCamera`)
✅ **窗口重置处理** (Swapchain Recreation，支持调整窗口大小)
✅ **真实的 DeltaTime 计时系统** (`std::chrono`)
✅ **完整的文档体系** (快速入门、架构文档、故障排除指南)

**优先级 1 (基础设施补全) 已 100% 完成！** 🎉

站在这个坚实的地基上，未来的开发计划按照由近及远、由基础设施到高级渲染，划分为 4 个优先级：

---

## 🔴 优先级 1：引擎基础设施补全 (Engine Hygiene) ✅ 已完成
*完善引擎的基础卫生，越早做后续开发越顺畅。*  **【全部完成！】**

- [x] **1.1 输入系统与漫游相机 (Camera Controller)**
  - ✅ 封装 GLFW 键盘/鼠标输入 (`InputManager`)
  - ✅ 实现 `CFirstPersonCamera` 组件，支持 3D 漫游 (WASD + 鼠标转动)
- [x] **1.2 窗口重置处理 (Swapchain Recreation)**
  - ✅ `RecreateSwapchain()` 方法已完整实现
  - ✅ 在 `AppWindow` 中添加了 GLFW Resize 事件回调
  - ✅ 在主循环中检测窗口大小变化
  - ✅ 自动处理 `VK_ERROR_OUT_OF_DATE_KHR` 和 `VK_SUBOPTIMAL_KHR`，安全重建 Swapchain崩溃
- [x] **1.3 真实的 DeltaTime (dt)**
  - ✅ 在主循环中接入 `std::chrono` 计算两帧真实的时间差
  - ✅ 传入 `Tick(float deltaTime)`，使相机的移动与旋转跟帧率解绑

## 🟡 优先级 2：PBR 与视觉效果的究极进化 (Visuals First)
*让引擎画面从“不错”迈向“惊艳”的核心步骤。*

- [x] **2.1 法线映射与切线空间 (Normal Mapping)** ✅ 已完成
  - ✅ 顶点布局增加 `Tangent` 属性并在 `FAssetManager` 的 OBJ 加载中计算切线
  - ✅ 在 Shader 构建 TBN 矩阵，从 Normal Map 采样并转换法线，赋予模型极其逼真的宏观凹凸质感
  - ✅ 扩展材质系统支持法线贴图参数（法线强度、贴图绑定）
  - ✅ 在 `CFirstPersonCamera` 中优化数学计算，直接使用 `CTransform` 的方向向量
  - ✅ 在 `InputManager` 中添加鼠标锁定功能，实现 Editor Camera 无尽拖拽体验
  - ✅ 在 `AppLogic` 中添加 `Scene->Tick()` 调用，确保完整的场景生命周期
- [ ] **2.2 基于图像的光照 (IBL - Image Based Lighting)**
  - 加载 `.hdr` 全景天空盒资源。
  - 预计算 Irradiance Map (用于 Diffuse 环境光) 替代现在的硬编码纯色。
  - 预计算 Prefilter Map 与 BRDF LUT (用于 Specular 环境反射)，使金属拥有真实的倒影。

## 🟢 优先级 3：高级渲染管线 (Advanced Rendering)
*打破单光的枯燥环境，迈入复杂的场景构建。*

- [ ] **3.1 多光源支持 (Multi-Lights Support)**
  - 扩展 `SceneDataUBO` 以支持多光源数组 `LightData[MAX_LIGHTS]`。
  - 修改 `pbr.frag` 用循环对所有点光源和平行光的光照贡献进行能量累加。
- [ ] **3.2 基础阴影映射 (Shadow Mapping)**
  - 新增 Shadow Pass，从光源视角将场景深度渲染到一张 Depth Map 中。
  - Normal Pass 采样该深度图比较遮挡关系，生成真实的几何阴影。

## 🔵 优先级 4：引擎架构的深度优化 (Architecture Depth)
*向着大型工业级引擎的复杂度迈进。*

- [ ] **4.1 Transform 层级树 (Hierarchy)**
  - 为 `CTransform` 增加 `Parent` 和 `Children` 指针，实现世界/局部矩阵的层级相乘连带（例如角色手拿武器）。
- [ ] **4.2 后台异步加载 (Async Asset Loading)**
  - 利用 C++ `std::thread` 或线程池，在后台线程中执行耗时的模型解析、贴图解码，并使用 Vulkan 的 StagingBuffer 与 Transfer Queue 配合 Fence 做到主线程零卡顿上传，彻底消除游戏启动或切图时的黑屏卡死！