# CLAUDE.md

本文件为 Claude Code (claude.ai/code) 在此仓库中工作时提供指导。

## 环境变量

不同机器的配置差异较大，参见 **`docs/machine-setups.md`**。当前机器配置摘要：

**Linux**:
```bash
export VCPKG_ROOT=/home/icecreamsarkaz/.vcpkg-clion/vcpkg
export VULKAN_SDK=$HOME/.local/vulkan-sdk/1.4.350.1/x86_64
export LD_LIBRARY_PATH=$VULKAN_SDK/lib/VulkanLoader/lib:$LD_LIBRARY_PATH
export PATH=$VULKAN_SDK/bin:$PATH
```

**Windows**:
```powershell
$env:VCPKG_ROOT = "D:\vcpkg"  # 根据实际安装路径调整
```

## 构建命令

**Linux (Ninja + vcpkg)**:
```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DBUILD_TESTING=ON
cmake --build build-linux
```

**Windows (MSVC + vcpkg)**:
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_TESTING=ON
cmake --build build --config Debug --parallel
```

**资产导入工具**:
```powershell
# 导入完整场景（含所有依赖的 mesh + texture）
.\build\src\Tools\AssetImporter\Debug\TumblerImporter.exe --input assets/scenes/demo.tscene --output cooked/

# 导入单个网格 / 纹理
.\build\src\Tools\AssetImporter\Debug\TumblerImporter.exe mesh assets/models/xxx.obj --output cooked/
.\build\src\Tools\AssetImporter\Debug\TumblerImporter.exe texture assets/textures/xxx.png --output cooked/
```

**示例应用**:
```powershell
cmake --build build --config Debug --target App-Tumbler --parallel
```

## 测试命令

```bash
# 全部测试
ctest --test-dir build-linux --output-on-failure

# 仅单元测试（不含 Vulkan）
ctest --test-dir build-linux -L unit --output-on-failure

# 按名称筛选
ctest --test-dir build-linux -R "Engine" --output-on-failure
```

## Pre-commit Hook

每次提交前自动运行：格式检查 → 编译 → 测试。配置：
```bash
git config core.hooksPath .githooks
```

## 代码格式化

```bash
find src tests -name "*.h" -o -name "*.cpp" | xargs clang-format -i
```

配置文件: `.clang-format` (LLVM 风格, 4 空格缩进, C++20, 120 列宽)

---

## 架构

### 分层结构

- **`src/Core/`** — 引擎库（编译为 `TumblerCore` 静态库），禁止依赖示例代码。
- **`src/Tools/AssetImporter/`** — 资产导入 CLI 工具（独立可执行文件 `TumblerImporter`），不依赖 Vulkan/GLFW。
- **`src/Examples/`** — 应用层（待重写）。
- **`tests/unit/`** — GoogleTest 单元测试。Vulkan 测试在 `tests/unit/Graphics/`。

### 子系统所有权

`Engine`（`src/Core/Engine/`）按依赖顺序创建并持有所有子系统，通过依赖注入传递引用：

```
EngineConfig → AssetDatabase → AppWindow → VulkanContext → RenderDevice
  → CommandManager → VulkanSwapchain → DescriptorManager → ResourceManager
```

### 目录速查

| 路径 | 职责 |
|------|------|
| `src/Core/Engine/` | EngineConfig + Engine（生命周期编排、主循环） |
| `src/Core/Assets/AssetDatabase` | asset_map.json 加载、源路径→cooked 路径映射 |
| `src/Core/Scene/SceneLoader` | Scene JSON 解析 → FScene 实例化 + Component 挂载 |
| `src/Core/Platform/AppWindow` | GLFW 窗口 + Vulkan Surface 创建 |
| `src/Core/Graphics/VulkanContext` | Vulkan 1.4 Instance + Device、队列族选择 |
| `src/Core/Graphics/VulkanSwapchain` | 交换链 + D32 深度缓冲 + resize 重建 |
| `src/Core/Graphics/RenderDevice` | 通过 VMA 创建/销毁 GPU 资源 |
| `src/Core/Graphics/CommandManager` | 命令池分配、ImmediateSubmit、布局转换 |
| `src/Core/Graphics/ResourceManager` | 统一 VB/IB、Mesh/纹理上传、Shader 加载 |
| `src/Core/Graphics/DescriptorManager` | Set 0 (Global) + Set 1 (Bindless) 描述符管理 |
| `src/Core/Math/` | 数学库：Vector/Matrix/Quaternion/Plane/Frustum |
| `src/Core/GameSystem/` | ECS：FActor、FScene、CTransform + 组件 |
| `src/Tools/AssetImporter/` | CLI：MeshImporter、TextureImporter、SceneSerializer |

### ECS（实体-组件系统）

`FActor` 持有 `CTransform` 成员 + 一组 `Component` 子类。Component 类型通过 `typeid(T)` + TypeMap 查询。

**现有组件：**
- `CTransform` — 嵌入 FActor 的成员（非 Component list 中的条目）
- `CStaticMesh` — mesh 资产引用 + 按 materialSlot 覆盖材质（`FMaterialRef` 数组）
- `CCamera` — FOV / Near / Far / LookAt
- `CPointLight` — Color / Intensity / Range
- `CDirectionalLight` — Direction / Color / Intensity

`FScene` 拥有所有 Actor 及生命周期（`PendingKillActors` 延迟销毁）。

### 资产管线

```
源文件 (OBJ/PNG/JPG) ──► TumblerImporter ──► cooked/ (.tmesh/.ttex/.tmat + asset_map.json)
                                                       │
                                              ┌────────┘
                                              ▼
                                     AssetDatabase (运行时映射)
                                              │
                                              ▼
                                     SceneLoader (创建 Actor + Component)
```

**文件格式：**
- `.tscene` — JSON，引用源文件路径（mesh、materials 数组、lights）
- `.tmesh` — 二进制，Header(64B) + SubMeshArray(N×44B) + VertexData + IndexData
- `.ttex` — 二进制，Header(32B) + MipData（CPU 端 mipmap 生成）
- `.tmat` — JSON，PBR Metallic-Roughness（合并通道）
- `asset_map.json` — 结构化分组（meshes/textures/materials），含 sourceHash + dependsOn

**关键约定：**
- 只提交源文件，`cooked/` 在 `.gitignore` 中
- Scene JSON 引用源文件路径（非 cooked），AssetDatabase 做运行时映射
- Material 的 Metallic-Roughness 合并为单纹理的两个通道（glTF 风格）
- 顶点格式：8×float32 交织（pos+normal+uv），资产不压缩，GPU 端自行量化
- Rotation 序列化格式：Quaternion [x,y,z,w]（Unity 风格）

### 渲染器子系统（当前状态）

Engine 创建所有 Vulkan 子系统但**尚未有 Renderer 管线**（Phase 4 以后）。当前主循环仅做 Acquire + Present（无 draw call）。

**Descriptor Set 绑定模型（已创建，渲染时启用）：**
```
Set 0 (Global): binding 0 = SceneUBO, binding 1 = CombinedImageSampler (ShadowMap)
Set 1 (Bindless): binding 0 = SampledImage[], binding 1 = MaterialData SSBO, binding 2 = ObjectData SSBO
```

**Vulkan 1.4 必需特性：**
- `bufferDeviceAddress` — GPU-Driven 间接绘制
- `descriptorIndexing` — Bindless 纹理数组
- `drawIndirectCount` — GPU 端生成绘制命令

### GPU-Driven 渲染管线（规划中）

目标：复现 UE5 Nanite 核心管线
```
InstanceCull → NodeCull → ClusterCull → RasterBin → SW/HW Raster → VisBuffer → DepthExport → ShadeBinning → ShadeGBuffer → Lighting
```

详见 `docs/gpu-driven-dev-plan.md`。

---

## 关键约定

- **命名空间**：所有代码统一使用 `namespace Tumbler`（旧 ECS 类型 FActor/FScene/Component/CTransform 也已迁入）。
- **日志**：强制使用 `LOG_INFO`/`LOG_ERROR`/`LOG_WARN` 等宏（`Core/Utils/Log.h`），**禁止** `std::cout` / `std::cerr`。
- **单例**：不使用单例模式。子系统由 Engine 持有 `std::unique_ptr`，通过引用注入。
- **编码规范**：详见 `docs/standards/coding-style.md`。
- **Windows 强制 MSVC** — CMake 在 WIN32 上显式拒绝非 MSVC 编译器。
- **vcpkg GLFW 3.4+** — CMake 强制优先解析 vcpkg 提供的 `glfw3Config.cmake`。
- **着色器编译** — 通过 `add_subdirectory(assets/shaders)` 处理，`.spv` 在 `.gitignore` 中。
- **第三方库实现**：`VMAImplementations.cpp`（仅 TumblerCore）+ `StbImplementations.cpp`（TumblerCore 和 TumblerImporter 共享）。
- **新模块添加**：`src/Core/` 下新增 `.cpp` 由 `GLOB_RECURSE` 自动收集。TumblerImporter 在 CMakeLists.txt 中显式列举源文件。

## 改动后验证

- 修改 CMake/依赖/平台层 → 重新配置、完整编译、`ctest`。
- 修改 Engine/子系统 Init 顺序 → `EngineSmokeTest`。
- 修改资产管线 → 执行 `TumblerImporter` 端到端验证。
- 修改 ECS → `CTransformTests` / `FActorTests` / `FSceneTests`。

## 文档索引

- `docs/machine-setups.md` — 各开发机工具链路径与环境变量
- `docs/gpu-driven-dev-plan.md` — Nanite 渲染器开发计划（Phase 4-10）
- `docs/guides/continuation.md` — 后续开发指南（Phase 进度 + 待做任务）
- `docs/standards/coding-style.md` — 编码规范（命名、日志、架构约束）
- `docs/architecture/overview.md` — 设计原则与数据流
- `docs/architecture/decisions.md` — 关键设计决策
- `docs/getting-started/setup.md` — 环境搭建
- `docs/code-navigation.md` — 文件级速查表
- `docs/troubleshooting.md` — 构建、运行时及 Linux/Wayland 问题
- `docs/testing-and-ci.md` — 测试结构与 CI 流程
