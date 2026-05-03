# 代码导航指南

帮助你快速理解代码结构，找到需要阅读的代码。

## 目录结构

```
Tumbler-Vulkan/
├── src/
│   ├── Core/
│   │   ├── Assets/              # 资产管理（Mesh/Texture/Material）
│   │   ├── Editor/              # 编辑器工具（UIManager/RuntimeConsole/DebugTexturePreview）
│   │   ├── GameSystem/          # ECS（FActor/FScene/Component/InputManager）
│   │   │   └── Components/      # 组件（CTransform/CMeshRenderer/CCamera/CPointLight/...）
│   │   ├── Geometry/            # 几何数据（FMesh）
│   │   ├── Graphics/            # 渲染系统
│   │   │   └── Pipelines/       # 管线策略实现（FForwardPipeline/FDeferredPipeline）
│   │   ├── Platform/            # 平台抽象（AppWindow GLFW 封装）
│   │   └── Utils/               # 工具类（Log/Singleton/VMA 实现）
│   └── Examples/
│       ├── Tumbler/             # 主示例（PBR 渲染 + 编辑器）
│       └── TinyRendererModels/  # 简单渲染示例
├── assets/
│   ├── models/                  # 3D 模型
│   ├── shaders/engine/          # 着色器（GLSL → SPIR-V）
│   └── textures/                # 纹理
├── docs/                        # 文档（入门/架构/指南/参考）
└── tests/unit/                  # GoogleTest 单元测试
```

## 按功能查找

### 1. 渲染流程

入口：`src/Examples/Tumbler/main.cpp` — 主循环

阅读顺序：
1. **`main.cpp`** — 帧循环（数据提取 → UI → 渲染）
2. **`VulkanRenderer.cpp`** — `Render()` / `RecordCommandBuffer()` 方法
3. **`IRenderPipeline.h`** — 管线策略接口
4. **`FForwardPipeline.cpp`** / **`FDeferredPipeline.cpp`** — 具体管线实现
5. **`PipelineUtils.cpp`** — `DrawMeshPackets()` / `CreateFramebuffers()` 共享辅助

### 2. 游戏系统（ECS）

| 功能 | 文件 |
|------|------|
| Actor | `src/Core/GameSystem/FActor.h` |
| Component 基类 | `src/Core/GameSystem/Components/Component.h` |
| 场景管理 | `src/Core/GameSystem/FScene.h` |
| Transform | `src/Core/GameSystem/Components/CTransform.h` |
| 网格渲染 | `src/Core/GameSystem/Components/CMeshRenderer.h` |
| 相机 | `src/Core/GameSystem/Components/CCamera.h` |
| 第一人称相机 | `src/Core/GameSystem/Components/CFirstPersonCamera.h` |
| 点光源 | `src/Core/GameSystem/Components/CPointLight.h` |
| 方向光 | `src/Core/GameSystem/Components/CDirectionalLight.h` |

参考示例：`src/Examples/Tumbler/AppLogic.cpp` — `InitializeScene()` 方法

### 3. 材质系统

| 功能 | 文件 |
|------|------|
| 材质母体 | `src/Core/Assets/FMaterial.h` |
| 材质实例 | `src/Core/Assets/FMaterialInstance.h`（含 `FMaterialUBO` 结构体） |
| 资产管理 | `src/Core/Assets/FAssetManager.h` |
| 纹理 | `src/Core/Assets/FTexture.h` |

重要提示：编辑材质参数时使用 `UpdateUBO()` 而非 `ApplyChanges()`。

### 4. 编辑器

| 功能 | 文件 |
|------|------|
| UI 管理器 | `src/Core/Editor/UIManager.h` |
| 运行时控制台 | `src/Core/Editor/RuntimeConsole.h` |
| G-Buffer 预览 | `src/Core/Editor/DebugTexturePreview.h` |
| 共享编辑状态 | `src/Core/Editor/EditorSessionState.h`（含 `RenderSettings`） |
| AppLogic 编辑器方法 | `src/Examples/Tumbler/AppLogic.h` / `.cpp` |
| 控制台命令绑定 | `src/Examples/Tumbler/TumblerConsoleBindings.cpp` |

编辑器面板方法：
- `DrawDebugPanel()` — 统一调试窗口（含 `CollapsingHeader` 布局）
- `DrawPerformanceSection()` — 性能统计
- `DrawLightingSection()` — 光源设置
- `DrawCameraSection()` — 相机参数
- `DrawRenderingSection()` — 渲染路径选择
- `DrawSceneHierarchyPanel()` — 场景层级（独立窗口）
- `DrawInspectorPanel()` — Inspector（独立窗口）

### 5. Vulkan 底层

| 功能 | 文件 |
|------|------|
| VulkanContext | `src/Core/Graphics/VulkanContext.h` |
| RenderDevice | `src/Core/Graphics/RenderDevice.h` |
| Swapchain | `src/Core/Graphics/VulkanSwapchain.h` |
| CommandBuffer | `src/Core/Graphics/CommandBufferManager.h` |
| 资源上传 | `src/Core/Graphics/ResourceUploadManager.h` |
| 管线构建器 | `src/Core/Graphics/VulkanPipelineBuilder.h` |
| VulkanTypes | `src/Core/Graphics/VulkanTypes.h`（AllocatedBuffer/Image、SceneDataUBO） |
| 描述符队列 | `src/Core/Graphics/DescriptorSetFreeQueue.h` |

## 关键代码位置速查

### 主循环

```
src/Examples/Tumbler/main.cpp
├── 初始化
│   ├── 创建窗口 + VulkanRenderer + FAssetManager
│   ├── InputManager 绑定（MoveForward/MoveRight/MoveUp）
│   ├── EditorSessionState + RenderSettings
│   └── AppLogic::Init(renderer, assetMgr, inputMgr, sessionState, renderSettings)
└── 帧循环
    ├── SetUIFocused() → inputManager.Tick() → ui_manager.TickInput()
    ├── logic.Tick(frameTime)
    ├── scene->ExtractRenderPackets(renderPackets)
    ├── ui_manager.BeginFrame() → logic.UpdatePerformanceStats() → DrawEditorUI() → EndFrame()
    ├── scene->GenerateSceneView(...)
    └── renderer.Render(viewData, renderPackets, UI callback)
```

### AppLogic 数据成员

```
AppLogic
├── Scene (unique_ptr<FScene>)
├── AssetMgr / InputMgr / SessionState / RenderCfg / Renderer（指针）
├── MainCamera (CFirstPersonCamera*)
├── Stats (PerformanceStats)
└── DebugPreview (DebugTexturePreview)
```

### EditorSessionState / RenderSettings

```cpp
struct EditorSessionState {
    FActor* SelectedActor;      // Hierarchy 面板选中
    bool ShowDebugPanel;        // 调试窗口开关
};
struct RenderSettings {
    ERenderPath CurrentRenderPath;  // Forward / Deferred / GPUDriven
};
```

### 渲染流程

```
Render() 方法：
1. FlushPendingDescriptorSetFrees()
2. vkWaitForFences(kFenceTimeoutNs)  // 5 秒超时
3. AcquireNextImage(kAcquireTimeoutNs)
4. vkResetCommandBuffer()
5. 更新 SceneDataUBO (memcpy 到持久映射缓冲)
6. pipeline->RecordCommands(cmd, imageIndex, ...)
   ├── [Forward]  单 Subpass：几何 + 光照
   └── [Deferred] Subpass 0: G-Buffer 写入 → Subpass 1: 全屏光照
7. onUIRender(cmd, imageIndex)  // ImGui 录制
8. vkEndCommandBuffer()
9. vkQueueSubmit() → vkQueuePresentKHR()
```

## 配套文档

- [架构概览](architecture/overview.md)
- [设计决策](architecture/decisions.md)
- [渲染架构](architecture/rendering.md)
- [渲染管线深度解析](../reference/rendering-pipeline.md)
- [ECS 游戏系统](architecture/ecs.md)
- [材质系统](architecture/material.md)
- [编辑器与调试](../guides/editor.md)
- [Vulkan 核心概念](../reference/vulkan-concepts/)
