# 🚀 Tumbler Engine 开发路线图 (Current Status)

**状态**：🚧 **Stage 9: 光照与 PBR 渲染 (启动中)**
**当前成就**：成功构建材质实例系统 (FMaterial/FMaterialInstance)，利用 UBO 和白模贴图还原了物理级色值的康奈尔盒子。
**当前焦点**：引入全局场景 UBO (相机/光源数据)，在 Shader 中实现真实的物理光照公式。

---

## ✅ 已完成里程碑 (Milestones Completed)

### 🏁 阶段一至阶段六：基础架构与 3D 渲染闭环
* [x] 窗口系统、Vulkan 核心初始化、VMA 内存管理。
* [x] Swapchain 双/三缓冲、同步机制 (Fence/Semaphore)。
* [x] CPU/GPU 网格分离、Staging Buffer 数据上传。
* [x] SPIR-V 着色器编译、图形管线构建器 (Pipeline Builder)。
* [x] GLM 矩阵运算、Push Constants 传递 MVP 矩阵。
* [x] 深度缓冲 (Depth Buffer) 与深度测试。

### 🎨 阶段七：纹理映射 (Texture Mapping)
* [x] **资源加载**：集成 `stb_image` 解析外部图片。
* [x] **图像缓冲**：创建 `VkImage`，使用 Pipeline Barrier 处理图像布局转换。
* [x] **纹理抽象**：封装 `FTexture`，实现 RAII 与移动语义防显存泄漏。
* [x] **资源缓存**：实现 `TextureManager`，利用 `shared_ptr` 共享纹理实例。

### 📦 阶段八：材质与重构 (Materials & Refactor)
* [x] **材质母体**：封装 `FMaterial`，接管 `VkPipeline` 和 `VkDescriptorSetLayout`。
* [x] **材质实例**：封装 `FMaterialInstance`，管理独立的 `VkDescriptorSet` 和 UBO 显存。
* [x] **渲染解耦**：改造 `CMeshRenderer` 和 `VulkanRenderer`，实现数据驱动的 DrawCall 提交。
* [x] **物理色值**：构建 Cornell Box 测试场景，验证材质系统的多实例复用。

---

## 🚧 阶段九：光照与 PBR 渲染 (Lighting & PBR) - **[当前任务]**

**目标**：打破纯色 Unlit 状态，引入光源，实现微表面物理渲染。

### 9.1 全局场景数据 (Global Scene UBO) - **[TODO]**
* [ ] **定义 C++ 结构**：定义包含 ViewProj、CameraPos、LightPos 的全局参数结构。
* [ ] **描述符拆分**：将材质参数移至 `Set 1`，将场景参数绑定至 `Set 0`。
* [ ] **每帧更新**：在 `Render` 函数开头更新全局 UBO 显存。

### 9.2 PBR 光照核心 (Shader Math) - **[TODO]**
* [ ] **辐射度量学基础**：实现点光源的光距衰减 (Inverse Square Falloff)。
* [ ] **Cook-Torrance BRDF**：
    * [ ] 实现法线分布函数 (NDF - Trowbridge-Reitz GGX)。
    * [ ] 实现几何遮蔽函数 (Geometry - Schlick-GGX)。
    * [ ] 实现菲涅尔方程 (Fresnel - Schlick Approximation)。
* [ ] **能量守恒**：结合漫反射 (Lambert) 与高光 (Specular)，输出最终颜色。

---

## 📅 阶段十：高级资产与场景 (Advanced Assets)

* [ ] **外部模型加载**：集成 `tiny_obj_loader` 读取复杂 3D 模型与法线。
* [ ] **法线映射**：引入 `Tangent` 顶点属性，解析并使用 Normal Map。
* [ ] **编辑器 UI**：集成 Dear ImGui，实现运行时的材质、灯光参数实时调节。