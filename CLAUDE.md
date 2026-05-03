# CLAUDE.md

本文件为 Claude Code (claude.ai/code) 在此仓库中工作时提供指导。

## 构建命令

```powershell
# Windows 配置 (MSVC + vcpkg, 在仓库根目录执行)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_TESTING=ON

# Windows 编译
cmake --build build --config Debug --parallel
```

```bash
# Linux 配置 (Ninja + vcpkg)
cmake -S . -B build-linux -G Ninja `
  -DCMAKE_BUILD_TYPE=Debug `
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DBUILD_TESTING=ON

# Linux 编译
cmake --build build-linux --target App-Tumbler
```

## 测试命令

```powershell
# 运行全部测试 (smoke + unit)
ctest --test-dir build -C Debug --output-on-failure

# 仅运行单元测试
ctest --test-dir build -C Debug -L unit --output-on-failure

# 按名称运行单个测试
ctest --test-dir build -C Debug -R "CTransform" --output-on-failure

# 列出所有已注册的测试（不执行）
ctest --test-dir build -C Debug -N
```

运行时冒烟测试需要开启 `-DTUMBLER_ENABLE_RUNTIME_SMOKE_TESTS=ON`。着色器产物冒烟检查依赖 `glslc`，如果 CI 环境中没有 `glslc`，着色器断言会被自动跳过。

## 架构

### 分层结构

- **`src/Core/`** — 可复用的引擎库（编译为 `TumblerCore` 静态库）。不得依赖示例代码。
- **`src/Examples/`** — 应用层（`App-Tumbler` 主示例、`TinyRendererModels` 小型示例）。
- **`tests/unit/`** — GoogleTest 单元测试，链接 `TumblerCore`。

### 实体-组件系统（ECS 变体）

`FActor` 是一个容器，持有一个 `CTransform` 和一组 `Component` 子类。通过挂载不同的组件来组合行为（`CMeshRenderer`、`CPointLight`、`CDirectionalLight`、`CCamera`、`CFirstPersonCamera`）。`FScene` 拥有所有 Actor 及其生命周期，通过 `PendingKillActors` 实现延迟销毁。

### 渲染：逻辑与物理的分离

渲染器绝不直接访问 Actor。数据流如下：

1. `FScene::ExtractRenderPackets()` 从 `CMeshRenderer` 组件中提取纯净的 `RenderPacket` 结构（网格指针、材质实例、`mat4` 变换矩阵）。
2. `FScene::GenerateSceneView()` 构建 `SceneViewData`，包含相机矩阵和可见光源数据。
3. `VulkanRenderer::Render()` 仅消费上述两种数据结构——它对实体或场景一无所知。

### 渲染器子系统

`VulkanRenderer` 协调五个子系统：

| 子系统 | 职责 |
|---|---|
| `VulkanContext` | Vulkan 实例、设备、队列族 |
| `VulkanSwapchain` | 交换链图像、深度缓冲、重建 |
| `RenderDevice` | 通过 VMA 创建/销毁 GPU 资源（Buffer、Image、Sampler） |
| `CommandBufferManager` | 命令池分配、即时提交、布局转换 |
| `ResourceUploadManager` | Mesh 上传（staging → device-local）、纹理加载、Mesh 去重缓存 |

### 双管线策略

`IRenderPipeline` 是策略接口，有两个具体实现：

- **`FForwardPipeline`** — 单个 Subpass，在 `pbr.frag` 中同时计算几何与光照。
- **`FDeferredPipeline`** — 2 个 Subpass 的 MRT G-Buffer：Albedo（`R8G8B8A8`）+ Normal+Roughness（`R16G16B16A16`），通过深度反投影重建世界坐标，光照阶段通过 `subpassInput` 读取 G-Buffer，对 Tile-Based GPU 友好。

`VulkanRenderer` 持有 `std::unordered_map<ERenderPath, std::unique_ptr<IRenderPipeline>>`。`SceneViewData::RenderPath` 每帧选择使用哪条管线。两条管线在启动时一起初始化，运行时可热切换。

### 材质系统：母体-实例模式

`FMaterial` 拥有烘焙后的管线（通过 `VulkanPipelineBuilder`）、管线布局和描述符集布局。`FMaterialInstance` 持有每实例的 `DescriptorSet`、`UBOBuffer` 及 CPU 端 `FMaterialUBO` 镜像。当两种管线均可用时，材质会同时为 Forward 和 Deferred 路径编译。

### UIManager / 控制台 / 编辑器状态

- `UIManager` 管理 ImGui 生命周期（GLFW/Vulkan 后端），驱动 `RuntimeConsole`，每帧调用 `TickInput()`。
- `RuntimeConsole` 管理输入框、历史记录、Tab 补全和命令注册——这是 Core 层通用框架。
- `EditorSessionState` 是共享编辑状态（`SelectedActor`、`CurrentRenderPath`），被 Inspector、层级面板、控制台命令和渲染路径切换共同消费。
- 示例专属的控制台命令通过 `TumblerConsoleBindings.cpp` 注册，不硬编码在 Core 中。

### 输入分离

`InputManager` 显式区分游戏输入（`GetAxis`、`IsActionPressed`、`GetMouseDelta`）和原始按键输入（`WasKeyJustPressed`）。控制台打开时（`~` / `GraveAccent`），立即调用 `SetGameplayInputBlocked(true)` 阻断相机移动，无需等待 ImGui 的焦点捕获。

## 关键约定

- **Windows 强制使用 MSVC** —— CMake 在 WIN32 上显式拒绝非 MSVC 编译器。
- **vcpkg GLFW 3.4+** —— CMake 强制优先解析 vcpkg 提供的 `glfw3Config.cmake`，避免误链系统自带的旧版 GLFW（项目使用了 `glfwGetPlatform` 等新 API）。
- **跨平台环境变量** —— `AppWindow.cpp` 中 Windows 用 `_putenv_s`，非 Windows 用 `setenv`/`unsetenv`。
- **着色器编译** —— 通过 `add_subdirectory(assets/shaders)` 处理。`.spv` 产物被 `.gitignore` 忽略，必须在本地构建。
- **资源拷贝** —— 构建后将 `assets/` 复制到可执行文件目录，运行时资源相对于二进制文件路径加载。
- **材质参数编辑** —— 需要参数跨帧持久时使用 `UpdateUBO()`（直接写入持久化映射的缓冲），而非 `ApplyChanges()`（仅更新描述符写入），否则参数值会丢失。

## 改动后验证

- 修改了 CMake/依赖/GLFW/平台层 → 重新配置、完整编译、运行 `ctest`。
- 修改了控制台/编辑器/输入逻辑 → 启动 `App-Tumbler`，验证控制台开关、历史记录、Tab 补全、命令执行和输入阻断。
- 修改了 Windows 构建链或平台兼容性 → 关注 `.github/workflows/windows-ci.yml` 的 CI 结果。

## 文档索引

完整文档地图见 `docs/INDEX.md`。常用文档速查：
- `docs/01_Architecture_Overview.md` — 设计原则与数据流
- `docs/09_Rendering_Pipeline_Deep_Dive.md` — 帧循环、G-Buffer、同步细节
- `docs/RenderingArchitecture.md` — 双管线策略、Deferred 优化、UI 解耦
- `docs/12_Code_Navigation_Guide.md` — 文件级"功能在哪"速查表
- `docs/10_Troubleshooting_Guide.md` — 构建、运行时及 Linux/Wayland 问题
- `docs/13_Testing_and_CI.md` — 测试结构与 CI 流程
