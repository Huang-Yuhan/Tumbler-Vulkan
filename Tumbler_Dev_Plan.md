没问题，我们将状态回退一步。这意味着虽然我们可能看过代码，但还没有正式将其集成到项目中并验证通过。

这是更新后的路线图，**Stage 7.1** 已重置为待办状态。

---

# 🚀 Tumbler Engine 开发路线图 (Current Status)

**状态**：🚧 **Stage 7: 纹理映射 (启动中)**
**当前成就**：已实现完整的 3D 渲染闭环（Mesh -> MVP 变换 -> 深度测试）。
**当前焦点**：生成并上传纹理资源到显存。

---

## ✅ 已完成里程碑 (Milestones Completed)

### 🏁 阶段一：基建施工 (Platform & Context)

* [x] **窗口系统**：GLFW 封装，Surface 创建。
* [x] **Vulkan 核心**：Instance, PhysicalDevice, LogicalDevice 初始化。
* [x] **内存管理**：集成 **VMA (Vulkan Memory Allocator)**，实现安全的资源销毁。

### 🔄 阶段二：渲染循环 (Swapchain & Loop)

* [x] **交换链**：实现双/三缓冲 (Double/Triple Buffering)。
* [x] **同步机制**：正确使用 Fence (CPU等待) 和 Semaphore (GPU等待)。
* [x] **渲染架构**：`Acquire` -> `Reset` -> `Record` -> `Submit` -> `Present` 流程打通。

### 📦 阶段三：数据与网格 (Mesh & Data)

* [x] **网格架构**：`FMesh` (CPU) 与 `FVulkanMesh` (GPU) 分离。
* [x] **上传机制**：实现 `Staging Buffer` -> `Device Buffer` 的高速拷贝。
* [x] **内存布局**：修正 Stride (32字节) 与 Attribute 绑定。

### 🎨 阶段四：管线与绘制 (Pipeline)

* [x] **Shader**：编译 SPIR-V，实现基础顶点/片元着色器。
* [x] **Pipeline Builder**：链式构建器封装复杂的管线状态。
* [x] **DrawCall**：实现 `vkCmdDrawIndexed` 绘制索引几何体。

### 📐 阶段五：3D 数学 (MVP Matrix)

* [x] **Push Constants**：打通 CPU 到 Shader 的轻量级数据通道。
* [x] **GLM 集成**：实现 Model(旋转) * View(相机) * Projection(透视) 变换。
* [x] **坐标系修正**：修复 Vulkan Y 轴翻转问题。

### 🧊 阶段六：深度缓冲 (Depth Buffer)

* [x] **深度资源**：创建 `D32_SFLOAT` 深度图与 ImageView。
* [x] **RenderPass 更新**：添加 Depth Attachment (Clear/Store)。
* [x] **管线更新**：开启 Depth Test & Depth Write。
* [x] **资源管理**：修复 Swapchain 重建/销毁时的 ImageView 泄漏问题。

---

## 🚧 阶段七：纹理映射 (Texture Mapping) - **[当前任务]**

**目标**：将显存中的棋盘格图片贴到 3D 平面上。

### 7.1 纹理资源准备 - **[TODO]**

*我们要在此步骤手动生成像素数据并上传。*

* [ ] **通用结构**：定义 `AllocatedImage` 结构体。
* [ ] **生成纹理**：CPU 生成棋盘格像素数据 (Staging Buffer)。
* [ ] **创建图像**：创建 `VkImage` (Device Local)。
* [ ] **布局转换**：实现 `TransitionImageLayout` (Pipeline Barrier) 处理图片状态。
* [ ] **数据拷贝**：实现 `CopyBufferToImage`。
* [ ] **视图与采样**：创建 `VkImageView` 和 `VkSampler`。

### 7.2 描述符系统 (Descriptors)

*Vulkan 中最复杂的部分之一，用于将 Uniform Buffer 和 Texture 绑定到 Shader。*

* [ ] **Pool**: 创建 `VkDescriptorPool` (显存池，用于分配 Set)。
* [ ] **Layout**: 创建 `VkDescriptorSetLayout` (告诉管线我们将绑定 1 个纹理)。
* [ ] **Sets**: 分配 `VkDescriptorSet` 并写入纹理信息。

### 7.3 管线集成

* [ ] **Shader 更新**: 修改 `.frag`，增加 `sampler2D`。
* [ ] **Pipeline 更新**: 将 `DescriptorSetLayout` 绑定到管线布局中。
* [ ] **绘制更新**: 在 `RecordCommandBuffer` 中调用 `vkCmdBindDescriptorSets`。

---

## 📅 阶段八：材质与重构 (Materials & Refactor)

**目标**：摆脱硬编码，建立真正的材质系统。

* [ ] **抽象 FTexture**: 封装纹理加载逻辑。
* [ ] **抽象 FMaterial**: 管理 Pipeline 和 Descriptor Sets。
* [ ] **重构 Renderer**: `Render(Mesh, Material, Transform)`。

---

### 💡 准备好了吗？

既然我们重置了状态，你可以把之前的纹理代码先**移除**或者**注释掉**（如果还没写完的话），我们随时可以重新开始 **7.1** 的编写。告诉我什么时候开始！