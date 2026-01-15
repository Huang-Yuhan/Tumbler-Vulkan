这是一个为你量身定制的开发路线图文档。你可以把它保存为 `Tumbler_Dev_Plan.md`，或者直接导入到你的 Notion/Obsidian 里，每完成一项就打个钩。

这份计划严格遵循了我们确定的 **“引擎底层 (Platform/Graphics) 与 游戏逻辑 (GameSystem) 分离”** 的架构思想。

---

# 🚀 Tumbler Engine 开发路线图

**目标**：构建一个基于 Vulkan 的现代 C++ 渲染引擎框架，实现逻辑与渲染分离。
**核心工具**：C++20, Vulkan, GLFW, VMA, GLM, CMake.

---

## 📅 阶段一：基建施工 (Platform & Context)

**目标**：程序能编译通过，运行后弹出一个空白窗口，后台成功初始化 Vulkan 和 VMA，退出时无报错。

### 1.1 文件结构与 CMake

* [ ] **整理目录**：确保 `src` 下存在 `Platform`、`Graphics`、`Utils` 文件夹。
* [ ] **更新 CMakeLists.txt**：
* [ ] 确保 `file(GLOB ...)` 或手动添加包含了上述新目录。
* [ ] 确保链接了 `glfw` 和 `vulkan` 库。
* [ ] **关键**：通过 vcpkg 或手动引入 `VulkanMemoryAllocator` (VMA)。



### 1.2 窗口系统 (AppWindow)

* [ ] **创建 `src/Platform/AppWindow.h` / `.cpp**`
* [ ] 封装 `GLFWwindow*`。
* [ ] 实现 `Init()`: 调用 `glfwInit`, `glfwCreateWindow`。
* [ ] 实现 `PollEvents()`: 调用 `glfwPollEvents`。
* [ ] 实现 `ShouldClose()`。
* [ ] 实现 `CreateSurface(VkInstance)`: 封装 `glfwCreateWindowSurface`。


* [ ] **验证点 1**：在 `main.cpp` 中实例化 `AppWindow`，确保窗口弹出且能关闭。

### 1.3 Vulkan 上下文 (VulkanContext)

* [ ] **创建 `src/Utils/VulkanImplementation.cpp**`
* [ ] 定义 `#define VMA_IMPLEMENTATION` 并包含 `vk_mem_alloc.h`。


* [ ] **创建 `src/Graphics/VulkanContext.h` / `.cpp**`
* [ ] **Instance**: 创建 Vulkan Instance (开启 Validation Layers)。
* [ ] **Surface**: 调用 `window->CreateSurface`。
* [ ] **PhysicalDevice**: 选择一张支持 Graphics Queue 的显卡。
* [ ] **LogicalDevice**: 创建 `VkDevice` 和 `VkQueue`。
* [ ] **VMA**: 初始化 `VmaAllocator`。


* [ ] **验证点 2**：程序启动后控制台打印 "Vulkan Initialized"，退出时 Validation Layers 无报错。

---

## 📅 阶段二：建立画布 (Swapchain & Loop)

**目标**：渲染器接管屏幕，能建立渲染循环，虽然屏幕是黑的（或闪烁颜色），但证明 GPU 正在每一帧工作。

### 2.1 交换链 (VulkanSwapchain)

* [ ] **创建 `src/Graphics/VulkanSwapchain.h` / `.cpp**`
* [ ] **Init**: 创建 `VkSwapchainKHR` (选择 SurfaceFormat, PresentMode, Extent)。
* [ ] **ImageViews**: 获取 `VkImage` 并为每个创建 `VkImageView`。
* [ ] **Acquire**: 封装 `vkAcquireNextImageKHR`。
* [ ] **Present**: 封装 `vkQueuePresentKHR`。



### 2.2 渲染器框架 (VulkanRenderer - Part 1)

* [ ] **更新 `src/Graphics/VulkanRenderer.h**`
* [ ] 持有 `VulkanContext` 和 `VulkanSwapchain` 成员。
* [ ] 实现 `Initialize(AppWindow*)`: 按顺序初始化 Context -> Swapchain。


* [ ] **实现同步对象**
* [ ] 创建 `VkFence` (RenderFence, 初始状态 Signal)。
* [ ] 创建 `VkSemaphore` (ImageAvailable, RenderFinished)。


* [ ] **实现命令缓冲**
* [ ] 创建 `VkCommandPool`。
* [ ] 分配 `VkCommandBuffer` (MainCommandBuffer)。



### 2.3 渲染循环 (Render Loop)

* [ ] **实现 `Render(FScene* scene)**` (暂时忽略 scene 参数)
* [ ] `vkWaitForFences`: 等待上一帧。
* [ ] `vkResetFences`: 重置信号。
* [ ] `Swapchain.AcquireNextImage`: 获取图片索引。
* [ ] `vkResetCommandBuffer`: 重置命令缓冲。
* [ ] `vkBeginCommandBuffer` -> `vkEndCommandBuffer`: 录制一个空命令。
* [ ] `vkQueueSubmit`: 提交命令 (等待 ImageAvailable，触发 RenderFinished)。
* [ ] `Swapchain.PresentImage`: 显示画面。


* [ ] **验证点 3**：运行程序，帧率正常（不卡顿），验证层无报错。

---

## 📅 阶段三：数据搬运 (Mesh & Memory)

**目标**：打通 CPU 到 GPU 的数据通道，实现网格上传机制。

### 3.1 资源定义

* [ ] **创建 `src/Graphics/VulkanTypes.h**`
* [ ] 定义 `struct AllocatedBuffer` (VkBuffer + VmaAllocation)。


* [ ] **创建 `src/Graphics/FVulkanMesh.h**`
* [ ] 定义 `struct FVulkanMesh` (持有 VertexBuffer, IndexBuffer)。
* [ ] 实现 `Destroy(VmaAllocator)` 方法。



### 3.2 净化 FMesh

* [ ] **修改 `src/geometry/FMesh.h**`
* [ ] **移除** `<vulkan/vulkan.h>`。
* [ ] 确保 `FMesh` 只是纯数据 (`vector<uint8_t>`, `vector<uint32_t>`)。
* [ ] 确保 `CreatePlane` 使用了 `#pragma pack` 和正确的内存布局。



### 3.3 上传机制 (UploadMesh)

* [ ] **在 `VulkanRenderer` 中实现辅助函数**
* [ ] `CreateBuffer(...)`: 封装 `vmaCreateBuffer`。
* [ ] `ImmediateSubmit(...)`: 用于执行单次拷贝命令。


* [ ] **实现 `UploadMesh(FMesh* cpuMesh)**`
* [ ] 检查 `MeshCache` 是否存在。
* [ ] 创建 Staging Buffer (Host Visible)。
* [ ] `memcpy` 数据到 Staging。
* [ ] 创建 GPU Buffer (Device Local)。
* [ ] `vkCmdCopyBuffer`: 拷贝 Staging -> GPU。
* [ ] 销毁 Staging，存入 Cache。



---

## 📅 阶段四：管线与绘制 (Pipeline & Draw)

**目标**：屏幕上出现一个彩色的几何体。这是最关键的视觉里程碑。

### 4.1 渲染管线基础设施

* [ ] **RenderPass**: 在 Renderer 中创建 `VkRenderPass` (包含 Color Attachment, ClearOp=Clear)。
* [ ] **Framebuffers**: 为 Swapchain 的每个 ImageView 创建一个 `VkFramebuffer`。

### 4.2 着色器与管线

* [ ] **编写 Shader**:
* [ ] `shaders/triangle.vert`: 接收 Position/Normal/UV。
* [ ] `shaders/triangle.frag`: 输出颜色。
* [ ] 编译为 `.spv` 文件。


* [ ] **创建 `VkPipeline**`:
* [ ] 定义 `VkPipelineLayout` (包含 Push Constants 范围，用于传 Model 矩阵)。
* [ ] 编写 `VulkanUtils::GenerateAttributeDescriptions(FVertexLayout)`。
* [ ] 配置 InputAssembly, Rasterizer, Multisampling 等。



### 4.3 完成绘制命令

* [ ] **完善 `RecordCommands**`:
* [ ] `vkCmdBeginRenderPass` (设置 ClearColor 为蓝色或其他色)。
* [ ] `vkCmdBindPipeline`.
* [ ] `vkCmdPushConstants` (上传 Model 矩阵)。
* [ ] `vkCmdBindVertexBuffers` & `IndexBuffer` (使用 `FVulkanMesh`).
* [ ] `vkCmdDrawIndexed`.
* [ ] `vkCmdEndRenderPass`.


* [ ] **验证点 4**：屏幕上出现一个平面！

---

## 📅 阶段五：逻辑层接入 (Logic Integration)

**目标**：通过 ECS 组件控制渲染，实现真正的引擎化。

### 5.1 组件实现

* [ ] **更新 `src/GameSystem/Components/CMeshRenderer.h**`
* [ ] 持有 `std::shared_ptr<FMesh>`。
* [ ] 持有 `std::shared_ptr<FMaterial>` (可选，先预留)。



### 5.2 逻辑层代码

* [ ] **在 `AppLogic::Init` 中**:
* [ ] `FMesh::CreatePlane(...)` 生成网格数据。
* [ ] `Scene->CreateActor("Floor")`。
* [ ] `Actor->AddComponent<CMeshRenderer>()->SetMesh(...)`。
* [ ] 设置 `Actor->Transform` 位置。



### 5.3 联调 (The Final Link)

* [ ] **更新 `VulkanRenderer::Render**`:
* [ ] 遍历 `scene->GetAllActors()`。
* [ ] 获取 `CMeshRenderer`。
* [ ] 调用 `UploadMesh(renderer->GetMesh())`。
* [ ] 获取 `Actor->Transform.GetLocalToWorldMatrix()`。
* [ ] 执行绘制。



---

# 🛠️ 常用开发小贴士

1. **Vulkan Validation Layers 是你的朋友**：只要屏幕黑了或者程序崩了，第一时间看控制台的红色报错。
2. **小步快跑**：不要一次性写完一个阶段的代码再运行。比如写完 `VulkanContext` 就运行一下，确保 `vkCreateInstance` 返回 `VK_SUCCESS`。
3. **命名空间**：记得所有代码都包在 `namespace Tumbler { ... }` 里。
4. **编码问题**：如果 CLion 控制台乱码，记得设置环境变量 `VSLANG=1033`。

祝开发顺利！我们先从 **阶段 1.2 AppWindow** 开始。