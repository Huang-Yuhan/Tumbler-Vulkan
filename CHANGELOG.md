# Changelog

## 2026-05-03 — 代码质量与设计审查

### 分层修复
- InputManager 解耦 ImGui：UI 焦点状态改由 main.cpp 注入
- AppLogic 不再直接管理 Vulkan 句柄：提取 `DebugTexturePreview` 到 Core/Editor

### 抽象修复
- `dynamic_cast<FDeferredPipeline*>` 消除：G-Buffer 访问器提升到 `IRenderPipeline` 接口
- `RecordCommands` 移除死代码 `onUIRender` 参数
- `RenderPacket` 改为 `shared_ptr` 持有 Mesh/Material
- `UploadMesh` 改为接受 `shared_ptr<FMesh>`

### 工具改进
- `RenderDevice::CreateImage` 增加 `requiredFlags` 参数
- G-Buffer 改为通过 `RenderDevice` 创建/销毁
- Deferred 管线 shader 加载改用统一的 `LoadShaderModule`
- `VulkanSwapchain` / `VulkanRenderer` 超时从 `UINT64_MAX` 改为 5 秒
- Forward 管线深度格式改用 `GetSwapchainDepthFormat()`

### 代码去重
- `DrawMeshPackets` 和 `CreateFramebuffers` 提取为 `IRenderPipeline` 静态方法
- 描述符池容量提取为命名常量 2000

### P3 改进
- `CMeshRenderer::GetMesh/GetMaterial` 返回 `const shared_ptr&`
- `FTexture` 抽取 `Release()` 方法
- 控制台 `SetToggleKey` 可配置
- `RenderSettings` 与 `EditorSessionState` 拆分
- `FActor` 增加 `type_index` 映射 O(1) 组件查找

### 文档
- 文档目录重构为四层结构：入门 / 架构 / 指南 / 参考
- 新增：设计决策记录、着色器参考、控制台命令参考、Changelog
- `Design_Review.md` 记录全部代码审阅项（P0→P3）
