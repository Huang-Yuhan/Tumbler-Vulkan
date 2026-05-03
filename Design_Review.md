# 代码设计审阅 (Design Review)

本文档记录对当前代码架构的系统性审阅，按严重程度从高到低排列。每项标注了优先级（P0-P3）、影响范围和修复建议。

---

## 分层违规

### P2 — VulkanRenderer 的 G-Buffer getter 仅被 AppLogic 调用

**位置**：`src/Core/Graphics/VulkanRenderer.h:105-106`

`GetGBufferAlbedoImageView()` / `GetGBufferNormalImageView()` 的唯一调用者是 AppLogic（通过 `DebugTexturePreview::SetImage`）。这两个 getter 本质是调试用途，且调用链仍经过 Example 层。G-Buffer ImageView 的传递路径应该在 Core 内部完成闭环。

**修复方向**：让 `DebugTexturePreview` 直接接收 `IRenderPipeline*` 自行查询，或通过回调注册机制让管线自行推送调试纹理，AppLogic 不再负责传递 ImageView。

---

### P2 — UIManager 是第二个渲染器

**位置**：`src/Core/Editor/UIManager.h:31-34` / `UIManager.cpp`

UIManager 拥有自己的 `VkRenderPass`、`VkFramebuffer` 数组，在 `RecordDrawCommands` 中自己调用 `vkCmdBeginRenderPass`/`vkCmdEndRenderPass`。它硬编码了交换链最终布局为 `VK_IMAGE_LAYOUT_PRESENT_SRC_KHR`。如果主渲染器将来加 HDR 或后处理 pass，UIManager 的布局假设会静默崩溃。

**修复方向**：将 UI 渲染通道的创建收敛到统一的渲染通道工厂。UIManager 只负责录制 ImGui 绘制命令到现有 CommandBuffer，不做自己的 RenderPass 管理。

---

## 抽象漏洞与策略模式破坏

### P3 — TransitionImageLayout 只支持两种转换路径

**位置**：`src/Core/Graphics/CommandBufferManager.cpp:152-167`

```
if (UNDEFINED → TRANSFER_DST_OPTIMAL) { ... }
else if (TRANSFER_DST_OPTIMAL → SHADER_READ_ONLY_OPTIMAL) { ... }
else { throw std::invalid_argument("Unsupported layout transition!"); }
```

任何其他组合（包括 Deferred 管线中 `COLOR_ATTACHMENT_OPTIMAL → SHADER_READ_ONLY_OPTIMAL` 的 G-Buffer 屏障）都会抛异常。这个工具函数几乎不可用。

**修复方向**：改为通用的布局转换函数，根据 `oldLayout` 和 `newLayout` 自动推导正确的 `srcAccessMask`/`dstAccessMask`/`srcStageMask`/`dstStageMask`。

---

## 硬编码与可配置性

### P3 — 组件成员全部 public

**位置**：`CCamera.h:12-14` / `CPointLight.h:22-23` / `CDirectionalLight.h:25-26`

FOV、NearPlane、FarPlane、Color、Intensity 等字段直接暴露为 public，无 setter 校验，无脏标记，无观察者通知。修改任一属性不会触发任何副作用（如标记 UBO 需要更新、标记场景图脏等）。

**修复方向**：改为 private + getter/setter，setter 中触发必要的更新通知。

---

## 资源生命周期

### P2 — RenderDevice::Cleanup 不销毁资源

**位置**：`src/Core/Graphics/RenderDevice.cpp:47-54`

`Cleanup()` 只把指针置空，不释放任何 Buffer/Image。释放顺序完全依赖调用者按正确顺序手动调用。当前 `VulkanRenderer::Cleanup()` 的顺序碰巧正确，但无任何强制。

**修复方向**：RenderDevice 应维护已分配资源的注册表，Cleanup 时检查并报告泄露。

---

## ECS 设计

### P3 — FMesh 是值类型却强制用 shared_ptr

**位置**：`FMesh.h` / `FAssetManager.cpp`

`FMesh` 无虚函数、可拷贝，但被强制通过 `shared_ptr` 使用。增加了不必要的堆分配和引用计数开销。

**修复方向**：引入 `AssetHandle<FMesh>` 或直接用 `unique_ptr` 管理。

---

## 测试

### P3 — 测试 Runner 类放在 main.cpp 匿名命名空间

**位置**：`src/Examples/Tumbler/main.cpp:29-342`

`ResizeStressTestRunner`、`DescriptorStressTestRunner`、`HiddenWindowSmokeTestRunner` 定义在 main.cpp 匿名命名空间内，无法被单元测试实例化，无法独立运行。

**修复方向**：将测试 runner 提取到独立文件中，使其可被 CTest 或其他测试框架引入。

---

## 总结

| 优先级 | 数量 | 核心主题 |
|--------|------|----------|
| P2 | 1 | RenderDevice::Cleanup 不销毁资源 |
| P3 | 4 | TransitionImageLayout, 组件 public, FMesh shared_ptr, 测试 Runner |

建议修复顺序：P1（策略模式 + 资源安全）→ P2（硬编码消除 + 去重）→ P3（按需修复）。
