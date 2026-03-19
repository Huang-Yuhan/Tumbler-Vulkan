# 🚀 Tumbler Engine 开发路线图 (Current Status)

**状态**：🚧 **Stage 10: 交互式调试与资产接入 (进行中)**
**当前成就**：成功手撕 Cook-Torrance BRDF，跑通直接光照管线；集成 `tinyobjloader`，实现 `FMesh::LoadFromOBJ` 与 `AppLogic::LoadOBJMesh`。
**当前焦点**：ImGui UI 接入与法线映射实现。

---

## ✅ 已完成里程碑 (Milestones Completed)
* [x] 基础架构、渲染循环、管线与同步 (Stage 1-6)。
* [x] 纹理映射、资源缓存与 RAII 封装 (Stage 7)。
* [x] 材质母体/实例分离系统，描述符集的高效绑定 (Stage 8)。
* [x] **PBR 光照核心**：实现 GGX 微表面分布、Schlick 几何遮蔽、Fresnel 近似，并完成能量守恒计算 (Stage 9)。
* [x] 修复 `ModelMatrix` 缩放导致的法线扭曲 (逆转置矩阵)。

---

## 🚧 阶段十：交互式调试与资产接入 (UI & Assets) - **[当前任务]**

**目标**：摆脱硬编码，引入 UI 面板和复杂模型，让引擎“活”起来。

### 10.1 引入交互式 UI (ImGui Integration)
* *每次改颜色都要重新编译 C++ 太痛苦了，我们需要实时反馈。*
* [ ] 引入 `imgui` 库 (通过 vcpkg)。
* [ ] 搭建 Vulkan 的 ImGui 渲染后端 (`ImGui_ImplVulkan`)。
* [ ] 在每帧渲染末尾绘制 UI，实现滑块动态调整 `FMaterialInstance` 的粗糙度、金属度和光源坐标。

### 10.2 复杂 3D 模型加载 (Model Loading) - ✅ **已完成**
* [x] 集成 `tinyobjloader`（`vcpkg.json` + `CMakeLists.txt`）。
* [x] `FMesh::LoadFromOBJ`：解析 Position/Normal/UV，哈希去重顶点，V 轴翻转兼容 Vulkan。
* [x] `AppLogic::LoadOBJMesh(path, name)`：加载并自动创建 Actor 挂入场景。

> 把 OBJ 放到 `assets/models/`，在 renderer 初始化**后**调用：
> ```cpp
> auto mesh = appLogic.LoadOBJMesh("assets/models/bunny.obj", "Bunny");
> renderer.UploadMesh(mesh.get());
> ```

### 10.3 法线映射与切线空间 (Normal Mapping)
* *让平坦的模型表面长出逼真的凹凸细节。*
* [ ] 修改顶点布局，增加 `Tangent` (切线) 属性。
* [ ] 在 C++ 加载模型时计算切线向量。
* [ ] 在 Shader 中构建 TBN 矩阵，从 Normal Map 采样并转换法线。

---

## 📅 阶段十一：基于图像的照明 (Image-Based Lighting, IBL)

**目标**：PBR 的终极形态。用一张全景 HDR 照片照亮整个场景。

* [ ] 解析 `.hdr` 格式的高动态范围环境图。
* [ ] **Diffuse IBL**：预计算辐照度贴图 (Irradiance Map) 替换当前的单一固定环境光。
* [ ] **Specular IBL**：预计算环境反射贴图 (Prefiltered Env Map) 与 BRDF 积分图 (LUT)。

## 📅 阶段十二：多光源与阴影 (Lights & Shadows)
* [ ] 扩展 Scene UBO，支持点光源数组 (Point Light Array)。
* [ ] 实现基础阴影映射 (Shadow Mapping)。