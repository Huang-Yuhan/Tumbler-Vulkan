# 代码导航指南 (Code Navigation Guide)

本文档帮助你快速理解 Tumbler 引擎的代码结构，找到你需要阅读的代码。

---

## 📂 目录结构概览

```
Tumbler-Vulkan/
├── src/
│   ├── Core/                    # 引擎核心代码
│   │   ├── Assets/              # 资产管理
│   │   ├── Editor/              # 编辑器工具
│   │   ├── GameSystem/          # 游戏系统 (ECS)
│   │   ├── Geometry/            # 几何数据
│   │   ├── Graphics/            # 渲染系统
│   │   ├── Platform/            # 平台抽象
│   │   └── Utils/               # 工具类
│   └── Examples/                # 示例程序
│       ├── Tumbler/             # 主要示例 (PBR 渲染)
│       └── TinyRendererModels/  # 简单示例
├── assets/                      # 资源文件
│   ├── models/                  # 3D 模型
│   ├── shaders/                 # 着色器
│   └── textures/                # 纹理
└── docs/                        # 文档
```

---

## 🎯 按功能查找代码

### 1. 想了解渲染流程？

**入口文件：** `src/Examples/Tumbler/main.cpp`

阅读顺序：
1. **`main.cpp`** - 主循环，了解整体流程
   - 初始化顺序
   - 帧循环结构
   - 数据准备 → UI → 渲染 流程

2. **`VulkanRenderer.cpp`** - 渲染器核心
   - 位置：`src/Core/Graphics/VulkanRenderer.cpp`
   - 重点看 `Render()` 方法

3. **`RenderPacket.h`** - 渲染数据包
   - 位置：`src/Core/Graphics/RenderPacket.h`
   - 了解要传给 GPU 什么数据

4. **着色器**
   - 顶点着色器：`assets/shaders/engine/pbr.vert`
   - 片段着色器：`assets/shaders/engine/pbr.frag`

### 2. 想了解游戏系统 (ECS)？

**核心文件：**

| 功能 | 文件位置 |
|------|----------|
| 游戏实体 | `src/Core/GameSystem/FActor.h` |
| 组件基类 | `src/Core/GameSystem/Components/Component.h` |
| 场景管理 | `src/Core/GameSystem/FScene.h` |
| 变换组件 | `src/Core/GameSystem/Components/CTransform.h` |
| 网格渲染 | `src/Core/GameSystem/Components/CMeshRenderer.h` |
| 相机 | `src/Core/GameSystem/Components/CCamera.h` |
| 第一人称相机 | `src/Core/GameSystem/Components/CFirstPersonCamera.h` |
| 点光源 | `src/Core/GameSystem/Components/CPointLight.h` |

**阅读示例：** `src/Examples/Tumbler/AppLogic.cpp` - `InitializeScene()` 方法

### 3. 想了解材质系统？

**核心文件：**

| 功能 | 文件位置 |
|------|----------|
| 材质母体 | `src/Core/Assets/FMaterial.h` |
| 材质实例 | `src/Core/Assets/FMaterialInstance.h` |
| 材质 UBO | `src/Core/Assets/FMaterialInstance.h` (看 `FMaterialUBO` 结构体) |
| 资产管理 | `src/Core/Assets/FAssetManager.h` |
| 纹理 | `src/Core/Assets/FTexture.h` |

**重要提示：**
- 编辑材质参数时，使用 `UpdateUBO()` 而不是 `ApplyChanges()`（见 `08_Editor_and_Debugging.md`）
- 材质参数必须与着色器中的 `MaterialParams` 结构体完全一致

### 4. 想了解编辑器功能？

**核心文件：**

| 功能 | 文件位置 |
|------|----------|
| UI 管理器 | `src/Core/Editor/UIManager.h` |
| 应用逻辑 | `src/Examples/Tumbler/AppLogic.h` |
| 编辑器实现 | `src/Examples/Tumbler/AppLogic.cpp` |

**编辑器面板方法：**
- `DrawPerformancePanel()` - 性能统计
- `DrawLightPanel()` - 光源设置
- `DrawCameraPanel()` - 相机参数
- `DrawSceneHierarchyPanel()` - 场景层级
- `DrawMaterialPanel()` - 材质编辑器

### 5. 想了解 Vulkan 底层？

**核心文件：**

| 功能 | 文件位置 |
|------|----------|
| Vulkan 上下文 | `src/Core/Graphics/VulkanContext.h` |
| 渲染设备 | `src/Core/Graphics/RenderDevice.h` |
| 交换链 | `src/Core/Graphics/VulkanSwapchain.h` |
| 命令缓冲管理 | `src/Core/Graphics/CommandBufferManager.h` |
| 资源上传 | `src/Core/Graphics/ResourceUploadManager.h` |
| Vulkan 类型 | `src/Core/Graphics/VulkanTypes.h` |
| 管线构建器 | `src/Core/Graphics/VulkanPipelineBuilder.h` |

---

## 🔍 关键代码位置速查

### 主循环 (`main.cpp`)

```
src/Examples/Tumbler/main.cpp
├── 初始化部分
│   ├── 创建窗口
│   ├── 创建渲染器
│   ├── 初始化资源管理器
│   └── 初始化 AppLogic
└── 主循环
    ├── 计算帧时间
    ├── 更新输入
    ├── 更新游戏逻辑 (logic.Tick)
    ├── 提取渲染数据 (ExtractRenderPackets)
    ├── 绘制编辑器 UI (DrawEditorUI)
    └── 渲染 (renderer.Render)
```

### AppLogic 架构 (`AppLogic.h/cpp`)

```
AppLogic
├── 初始化
│   └── InitializeScene() - 创建场景中的物体
├── 每帧更新
│   └── Tick() - 更新相机等
├── 编辑器 UI
│   ├── DrawEditorUI() - 统一入口
│   ├── DrawPerformancePanel()
│   ├── DrawLightPanel()
│   ├── DrawCameraPanel()
│   ├── DrawSceneHierarchyPanel()
│   └── DrawMaterialPanel()
└── 数据
    ├── Scene - 场景
    ├── SelectedActor - 选中的物体
    └── Stats - 性能统计
```

### 渲染流程 (`VulkanRenderer.cpp`)

```
Render() 方法流程：
1. 等待上一帧完成
2. 获取下一帧图像
3. 重置命令缓冲
4. 开始 RenderPass
5. 绑定全局描述符集
6. 遍历 RenderPackets：
   ├── 绑定材质管线
   ├── 绑定材质描述符集
   ├── 绑定网格
   └── 绘制
7. 绘制 UI
8. 结束 RenderPass
9. 提交命令缓冲
10. 呈现
```

---

## 📖 推荐阅读顺序

如果你是第一次阅读代码，建议按以下顺序：

### 第一阶段：理解整体流程
1. **`src/Examples/Tumbler/main.cpp`** - 看主循环
2. **`src/Examples/Tumbler/AppLogic.cpp`** - 看 `InitializeScene()` 和 `Tick()`
3. **`docs/01_Architecture_Overview.md`** - 读架构文档

### 第二阶段：理解游戏系统
1. **`src/Core/GameSystem/FActor.h`** - 了解 Actor
2. **`src/Core/GameSystem/Components/Component.h`** - 了解 Component
3. **`src/Core/GameSystem/FScene.h`** - 了解 Scene
4. **`docs/06_Game_System_Architecture.md`** - 读游戏系统文档

### 第三阶段：理解渲染
1. **`src/Core/Graphics/RenderPacket.h`** - 看渲染数据包
2. **`src/Core/Graphics/VulkanRenderer.cpp`** - 看 `Render()` 方法
3. **`assets/shaders/engine/pbr.vert`** - 看顶点着色器
4. **`assets/shaders/engine/pbr.frag`** - 看片段着色器
5. **`docs/09_Rendering_Pipeline_Deep_Dive.md`** - 读渲染管线文档

### 第四阶段：理解材质
1. **`src/Core/Assets/FMaterialInstance.h`** - 看 `FMaterialUBO`
2. **`src/Core/Assets/FMaterialInstance.cpp`** - 看 `UpdateUBO()` 和 `ApplyChanges()`
3. **`docs/03_Material_System.md`** - 读材质系统文档

### 第五阶段：深入 Vulkan
1. **`src/Core/Graphics/VulkanContext.h`**
2. **`src/Core/Graphics/VulkanSwapchain.h`**
3. **`docs/05_Vulkan_Core_Concepts.md`** - 读 Vulkan 文档

---

## 💡 阅读技巧

### 1. 从示例入手
先看 `src/Examples/Tumbler/` 下的代码，这是实际使用引擎的地方，最容易理解。

### 2. 先看头文件
头文件 (`.h`) 通常包含类的接口和关键数据结构，先看头文件能快速了解这个类是做什么的。

### 3. 使用搜索
如果你在找某个功能，可以用搜索工具（如 VS Code 的搜索）搜索关键词：
- 找 PBR 相关：搜索 "PBR" 或 "BRDF"
- 找 Vulkan 相关：搜索 "Vk" 或 "Vulkan"
- 找材质相关：搜索 "Material"

### 4. 画图理解
遇到复杂的数据流时，不妨画个图：
- 数据从哪里来？
- 经过哪些处理？
- 最终到哪里去？

### 5. 调试 + 断点
在关键位置打断点，看变量的值，理解代码执行流程。

---

## 🔧 常见问题

### Q: 我想添加一个新的 Component，应该怎么做？
A: 参考 `src/Core/GameSystem/Components/` 下的现有组件，继承 `Component` 类即可。

### Q: 我想修改着色器，应该改哪里？
A: 修改 `assets/shaders/engine/` 下的 `.vert` 和 `.frag` 文件，然后重新编译项目（CMake 会自动编译着色器）。

### Q: 我想添加一个新的编辑器面板，应该怎么做？
A: 在 `AppLogic` 类中添加新的 `DrawXxxPanel()` 方法，然后在 `DrawEditorUI()` 中调用它。

### Q: 渲染数据是如何从 CPU 传到 GPU 的？
A: 看 `RenderPacket` 结构，以及 `VulkanRenderer::Render()` 方法中如何遍历和使用 `RenderPacket`。

---

## 📚 配套文档

阅读代码时，配合以下文档会更容易理解：

- [架构概览 (01_Architecture_Overview.md)](01_Architecture_Overview.md)
- [游戏系统架构 (06_Game_System_Architecture.md)](06_Game_System_Architecture.md)
- [渲染管线深度解析 (09_Rendering_Pipeline_Deep_Dive.md)](09_Rendering_Pipeline_Deep_Dive.md)
- [材质系统 (03_Material_System.md)](03_Material_System.md)
- [Vulkan 核心概念 (05_Vulkan_Core_Concepts.md)](05_Vulkan_Core_Concepts.md)
