# 代码导航指南

快速定位代码位置，理解项目结构。

## 目录结构

```
Tumbler-Vulkan/
├── src/
│   ├── Core/
│   │   ├── Assets/              # AssetDatabase — 运行时资产映射 (asset_map.json)
│   │   ├── Engine/              # Engine + EngineConfig — 生命周期编排
│   │   ├── GameSystem/          # ECS — FActor / FScene / InputManager
│   │   │   └── Components/      # CTransform / CStaticMesh / CCamera / CLights
│   │   ├── Graphics/            # Vulkan 基础设施（无 Renderer）
│   │   │   ├── VulkanContext    # Instance + Device 1.4
│   │   │   ├── VulkanSwapchain  # 交换链 + D32 深度
│   │   │   ├── RenderDevice     # VMA Buffer/Image
│   │   │   ├── CommandManager   # CommandPool + ImmediateSubmit
│   │   │   ├── ResourceManager  # 统一 VB/IB + Mesh/纹理上传
│   │   │   └── DescriptorManager # Set 0 Global + Set 1 Bindless
│   │   ├── Math/                # Vector/Matrix/Quaternion/Plane/Frustum
│   │   ├── Platform/            # AppWindow — GLFW + Surface
│   │   ├── Scene/               # SceneLoader — JSON → FScene
│   │   └── Utils/               # Log / Singleton / Stb / VMA 实现
│   ├── Examples/
│   │   └── Tumbler/             # App-Tumbler 主示例（Engine API 集成）
│   └── Tools/
│       └── AssetImporter/       # CLI — OBJ→.tmesh, PNG→.ttex, scene→asset_map
├── assets/
│   ├── models/                  # 3D 模型（OBJ）
│   ├── shaders/                 # GLSL → SPIR-V
│   ├── scenes/                  # 场景 JSON (.tscene)
│   ├── materials/               # 材质 JSON (.tmat)
│   └── textures/                # 纹理
├── docs/                        # 文档
└── tests/unit/                  # GoogleTest 单元测试
```

## 按功能查找

### 1. 入口与主循环

```
src/Examples/Tumbler/main.cpp
├── EngineConfig::LoadFromFile("engine.json")
├── Engine::Init(config)          — 初始化 8 个子系统
├── SceneLoader::LoadFromFile()   — 加载场景
├── Engine::Run()                 — 主循环 (Acquire+Present)
└── Engine::Shutdown()
```

### 2. Engine 子系统依赖链

| 顺序 | 子系统 | 文件 | 依赖 |
|------|--------|------|------|
| 1 | AssetDatabase | `src/Core/Assets/AssetDatabase.h` | 无 |
| 2 | AppWindow | `src/Core/Platform/AppWindow.h` | 无 |
| 3 | VulkanContext | `src/Core/Graphics/VulkanContext.h` | AppWindow |
| 4 | RenderDevice | `src/Core/Graphics/RenderDevice.h` | VulkanContext |
| 5 | CommandManager | `src/Core/Graphics/CommandManager.h` | VulkanContext |
| 6 | VulkanSwapchain | `src/Core/Graphics/VulkanSwapchain.h` | VulkanContext + RenderDevice |
| 7 | DescriptorManager | `src/Core/Graphics/DescriptorManager.h` | VulkanContext |
| 8 | ResourceManager | `src/Core/Graphics/ResourceManager.h` | RenderDevice + CommandManager |

### 3. ECS (实体-组件系统)

| 类型 | 文件 | 职责 |
|------|------|------|
| FActor | `src/Core/GameSystem/FActor.h` | 实体容器，持有 Transform + Components |
| FScene | `src/Core/GameSystem/FScene.h` | Actor 生命周期管理，延迟销毁 |
| Component | `src/Core/GameSystem/Components/Component.h` | 组件基类 |
| CTransform | `src/Core/GameSystem/Components/CTransform.h` | 位置/旋转/缩放，层级支持 |
| CStaticMesh | `src/Core/GameSystem/Components/CStaticMesh.h` | Mesh 资产引用 + 材质覆盖 |
| CCamera | `src/Core/GameSystem/Components/CCamera.h` | FOV / Near / Far / LookAt |
| CPointLight | `src/Core/GameSystem/Components/CPointLight.h` | Color / Intensity / Range |
| CDirectionalLight | `src/Core/GameSystem/Components/CDirectionalLight.h` | Direction / Color / Intensity |
| InputManager | `src/Core/GameSystem/InputManager.h` | 输入轮询与动作绑定 |

所有 ECS 类型均在 `namespace Tumbler` 中。

### 4. 资产管线

```
源文件 (OBJ/PNG/jpg) ──► TumblerImporter ──► cooked/
                                               ├── meshes/*.tmesh
                                               ├── textures/*.ttex
                                               ├── materials/*.tmat
                                               └── asset_map.json

Scene JSON (*.tscene) ──► AssetDatabase ──► SceneLoader ──► FScene + Actors
```

| 模块 | 文件 | 职责 |
|------|------|------|
| AssetImporter | `src/Tools/AssetImporter/` | CLI 工具：OBJ→.tmesh, PNG→.ttex, Scene→asset_map |
| AssetDatabase | `src/Core/Assets/AssetDatabase.h` | 加载 asset_map.json，源路径→cooked 查询 |
| SceneLoader | `src/Core/Scene/SceneLoader.h` | 解析 Scene JSON，创建 Actor + Component |
| AssetFormats | `src/Core/AssetFormats.h` | .tmesh/.ttex 二进制格式定义 |

### 5. 渲染基础设施 (当前状态)

**尚无 Renderer 管线**。Engine::Run() 目前只做 Acquire + Present。

| 模块 | 文件 | 职责 |
|------|------|------|
| VulkanContext | `src/Core/Graphics/VulkanContext.h` | Instance + Device + 队列族 (Vulkan 1.4) |
| RenderDevice | `src/Core/Graphics/RenderDevice.h` | Buffer/Image/Sampler 创建 (VMA) |
| CommandManager | `src/Core/Graphics/CommandManager.h` | CommandPool + ImmediateSubmit + 布局转换 |
| VulkanSwapchain | `src/Core/Graphics/VulkanSwapchain.h` | 交换链 + D32 深度 + resize 重建 |
| DescriptorManager | `src/Core/Graphics/DescriptorManager.h` | Set 0 (Global) + Set 1 (Bindless) |
| ResourceManager | `src/Core/Graphics/ResourceManager.h` | 统一 VB/IB + Mesh/纹理/Shader 上传 |
| VulkanUtils | `src/Core/Graphics/VulkanUtils.h` | VK_CHECK 宏 |

### 6. 数学库

| 类型 | 文件 |
|------|------|
| Vector3f | `src/Core/Math/Vector.h` |
| Vector2f | `src/Core/Math/Vector2.h` |
| Vector4f | `src/Core/Math/Vector4.h` |
| Matrix4f | `src/Core/Math/Matrix.h` |
| Quaternionf | `src/Core/Math/Quaternion.h` |
| Plane | `src/Core/Math/Plane.h` |
| Frustum | `src/Core/Math/Frustum.h` |
| 工具函数 | `src/Core/Math/MathUtility.h` |

## 文档配套

- [架构概览](architecture/overview.md) — 设计原则
- [编码规范](standards/coding-style.md) — 命名/日志/架构约束
- [GPU-Driven 开发计划](gpu-driven-dev-plan.md) — Phase 4-10 规划
- [后续开发指南](guides/continuation.md) — 当前进度 + 待做任务
- [故障排除](troubleshooting.md)
