# 🚀 Tumbler Engine 开发路线图 (Roadmap)

**当前状态**：✅ **优先级 1 完成！法线贴图与示例应用完善 + imgui API 兼容性修复**

经过阶段 1~10 的打磨和最近的 Bug 修复，Tumbler 引擎已具备：
✅ 基于实体-组件 (ECS雏形) 的逻辑架构
✅ 严格物理隔离的 `SceneViewData` 与 `RenderPacket` 数据流
✅ 基于资源缓存与依赖注入的 `FAssetManager`
✅ 高效的 Vulkan PBR 材质母体/实例管线绑定模型
✅ 包含环境采样、GGX微表面、Schlick菲涅尔的 Cook-Torrance BRDF Shader
✅ **完整的输入系统与漫游相机** (`InputManager` + `CFirstPersonCamera`)
✅ **窗口重置处理** (Swapchain Recreation，支持调整窗口大小)
✅ **真实的 DeltaTime 计时系统** (`std::chrono`)
✅ **完整的文档体系** (快速入门、架构文档、故障排除指南)
✅ **双面光照支持** (TwoSided 材质选项，使用 faceforward 确保法线朝向相机)
✅ **平面几何修复** (修正索引环绕顺序为顺时针)
✅ **Bug 修复完成** (AD方向修复、Double Tick BUG修复、Dead Code清理)
✅ **法线贴图完整实现** (切线计算、TBN矩阵、法线采样与转换)
✅ **新示例应用** (TinyRendererModels，渲染所有 tiny-renderer-obj 模型)
✅ **完整贴图支持** (Diffuse + Normal Map 为所有模型加载)
✅ **imgui API 兼容性修复** (适配新版 imgui 的 PipelineInfoMain 结构)

**优先级 1 (基础设施补全) 已 100% 完成！** 🎉

站在这个坚实的地基上，未来的开发计划按照由近及远、由基础设施到高级渲染，划分为 5 个优先级（IBL 已移至最后）：

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

## 🟡 优先级 2：编辑器与开发体验 (Developer Experience) ⭐ 最高优先级
*工欲善其事，必先利其器。完善的编辑器工具让后续开发事半功倍。*

- [ ] **2.1 imgui 编辑器集成与性能面板**
  - 在 `AppLogic` 中添加 imgui 绘制框架
  - 实现性能统计面板（FPS、帧时间、Draw Call 计数）
  - 添加相机参数实时调整面板
- [ ] **2.2 光源参数实时编辑**
  - 实现光源位置、颜色、强度的滑块控制
  - 支持在运行时添加/删除光源
  - 保存/加载光源配置
- [ ] **2.3 材质参数编辑器**
  - 实时调整基础色、粗糙度、金属度
  - 支持法线强度、TwoSided 等选项
  - 实时预览材质变化

## 🟢 优先级 3：高级渲染管线 (Advanced Rendering)
*打破单光的枯燥环境，迈入复杂的场景构建。*

- [ ] **3.1 多光源支持 (Multi-Lights Support)**
  - 扩展 `SceneDataUBO` 以支持多光源数组 `LightData[MAX_LIGHTS]`。
  - 修改 `pbr.frag` 用循环对所有点光源和平行光的光照贡献进行能量累加。
  - 在示例场景中添加多个光源进行测试。
- [ ] **3.2 基础阴影映射 (Shadow Mapping)**
  - 新增 Shadow Pass，从光源视角将场景深度渲染到一张 Depth Map 中。
  - 主 Pass 采样该深度图比较遮挡关系，生成真实的几何阴影。
  - 实现 PCF (Percentage Closer Filtering) 软阴影优化。

## 🔵 优先级 4：引擎架构的深度优化 (Architecture Depth)
*向着大型工业级引擎的复杂度迈进。*

- [ ] **4.1 Transform 层级树 (Hierarchy)**
  - 为 `CTransform` 增加 `Parent` 和 `Children` 指针，实现世界/局部矩阵的层级相乘连带（例如角色手拿武器）。
  - 优化：缓存世界矩阵，仅在变换或父级变化时更新。
- [ ] **4.2 后台异步加载 (Async Asset Loading)**
  - 利用 C++ `std::thread` 或线程池，在后台线程中执行耗时的模型解析、贴图解码，并使用 Vulkan 的 StagingBuffer 与 Transfer Queue 配合 Fence 做到主线程零卡顿上传，彻底消除游戏启动或切图时的黑屏卡死！
- [ ] **4.3 更多 PBR 材质贴图支持**
  - 粗糙度贴图 (Roughness Map)
  - 金属度贴图 (Metallic Map)
  - AO 贴图 (Ambient Occlusion)
  - 自发光贴图 (Emissive Map)

## 🟣 优先级 5：高级视觉效果 (Premium Visuals) - 放到最后
*让引擎画面从“不错”迈向“惊艳”的核心步骤。*

- [x] **5.1 法线映射与切线空间 (Normal Mapping)** ✅ 已完成
  - ✅ 顶点布局增加 `Tangent` 属性并在 `FAssetManager` 的 OBJ 加载中计算切线
  - ✅ 在 Shader 构建 TBN 矩阵，从 Normal Map 采样并转换法线，赋予模型极其逼真的宏观凹凸质感
  - ✅ 扩展材质系统支持法线贴图参数（法线强度、贴图绑定）
  - ✅ 在 `CFirstPersonCamera` 中优化数学计算，直接使用 `CTransform` 的方向向量
  - ✅ 在 `InputManager` 中添加鼠标锁定功能，实现 Editor Camera 无尽拖拽体验
  - ✅ 在 `AppLogic` 中添加 `Scene->Tick()` 调用，确保完整的场景生命周期
  - ✅ 创建 TinyRendererModels 示例应用，完整展示法线贴图效果
- [x] **5.2 双面光照与材质 TwoSided 选项** ✅ 已完成
  - ✅ 修复平面索引环绕顺序为正确的顺时针
  - ✅ 在 pbr.frag 中使用 `faceforward` 函数确保法线始终朝向相机
  - ✅ 在 FMaterialInstance 中添加 `SetTwoSided(bool)` 方法
  - ✅ 在 FMaterialUBO 中添加 TwoSided 标志，通过 UBO 传递到着色器
  - ✅ 为墙壁材质设置 TwoSided=true，从任意角度都能正确接收光照
- [ ] **5.3 基于图像的光照 (IBL - Image Based Lighting)**
  - 加载 `.hdr` 全景天空盒资源。
  - 预计算 Irradiance Map (用于 Diffuse 环境光) 替代现在的硬编码纯色。
  - 预计算 Prefilter Map 与 BRDF LUT (用于 Specular 环境反射)，使金属拥有真实的倒影。
  - 实现天空盒渲染。

---

## 📊 推荐开发顺序

| 阶段 | 预计时间 | 功能 | 优先级 |
|------|----------|------|--------|
| 第一周 | 1-3天 | imgui 编辑器与性能面板 | ⭐⭐⭐⭐⭐ |
| 第一周 | 3-7天 | 光源参数实时编辑 | ⭐⭐⭐⭐ |
| 第二周 | 7-14天 | 多光源支持 | ⭐⭐⭐⭐ |
| 第三周 | 14-21天 | 基础阴影映射 | ⭐⭐⭐⭐ |
| 第四周 | 21-28天 | Transform 层级树 | ⭐⭐⭐ |
| 第五周 | 28-35天 | 异步资源加载 | ⭐⭐⭐ |
| 第六周+ | 35天+ | 更多 PBR 贴图支持 | ⭐⭐ |
| 最后 | - | IBL 实现 | ⭐ |

---

## 🔧 随时可以做的小改进

在实现大功能的间隙，可以做这些小改进来热身：

- **调试工具**：线框渲染模式、法线可视化、切线空间可视化
- **性能优化**：视锥剔除、渲染队列排序
- **代码质量**：添加更多日志、完善错误处理