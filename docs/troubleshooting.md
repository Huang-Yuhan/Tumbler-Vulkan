# 故障排除指南 (Troubleshooting Guide)

本文档收集了使用 Tumbler 引擎时常见的问题及其解决方案。

---

## 1. 构建问题

### 1.1 CMake 找不到 Vulkan

**错误信息：**
```
Could NOT find Vulkan (missing: Vulkan_INCLUDE_DIR Vulkan_LIBRARY)
```

**原因：** Vulkan SDK 未安装或环境变量未设置。

**解决方案：**
1. 下载并安装 [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
2. 重启电脑或重新打开终端
3. 验证环境变量 `VULKAN_SDK` 是否存在：
   ```powershell
   echo $env:VULKAN_SDK
   ```

### 1.2 必须使用 MSVC 编译器

**错误信息：**
```
本项目必须使用 MSVC 编译器编译。请在 CLion Toolchain 中切换为 Visual Studio 并重新 Reload。
```

**原因：** 项目强制要求使用 MSVC，当前使用的是 MinGW 或其他编译器。

**解决方案：**

**CLion 用户：**
1. 打开 `File` → `Settings` → `Build, Execution, Deployment` → `Toolchains`
2. 点击 `+` 添加新的 Toolchain
3. 选择 `Visual Studio`
4. 确保 `Architecture` 设置为 `amd64`
5. 将其设为默认 Toolchain
6. 点击 `File` → `Reload CMake Project`

**Visual Studio 用户：**
- 使用 CMake 生成 Visual Studio 解决方案：
  ```powershell
  cmake .. -G "Visual Studio 17 2022" -A x64
  ```

### 1.3 vcpkg 依赖安装失败

**错误信息：**
```
error: while looking for 'xxx':
```

**解决方案：**
1. 更新 vcpkg：
   ```powershell
   cd $env:VCPKG_ROOT
   git pull
   .\bootstrap-vcpkg.bat
   ```
2. 清理 CMake 缓存并重新配置：
   ```powershell
   cd build
   Remove-Item * -Recurse -Force
   cmake .. -DCMAKE_TOOLCHAIN_FILE=$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake
   ```

### 1.4 CMake 版本过旧

**错误信息：**
```
CMake 3.28 or higher is required. You are running version X.X.X
```

**解决方案：**
1. 下载并安装最新版 [CMake](https://cmake.org/download/)
2. 或使用 Visual Studio 自带的 CMake（版本通常足够新）

### 1.5 CLion Build 输出乱码

**现象：**
- Build 窗口中的中文日志显示为 `閫傜敤浜?`、`姝ｅ湪缂栬瘧...` 等乱码。

**原因：**
- 项目文件和 CMake 消息是 UTF-8，但 CLion Build 窗口或其子进程按 CP936/GBK 解码，导致 UTF-8 文本被错误解释。

**解决方案（UTF-8 主线）：**
1. 打开 `File` → `Settings` → `Editor` → `File Encodings`
2. 将 `Global Encoding`、`Project Encoding` 设为 `UTF-8`
3. 打开 `Settings` → `Tools` → `Terminal`，将默认编码设为 `UTF-8`
4. 打开 `Settings` → `Build, Execution, Deployment` → `CMake`
5. 在当前 Build Profile 的 Environment 中添加 `VSLANG=2052`（中文输出）
6. 点击 `File` → `Reload CMake Project` 后重新构建

**兜底方案（稳定可读）：**
- 如果仍偶发乱码，将同一位置的 `VSLANG` 改为 `1033`，强制 MSBuild 输出英文日志（ASCII，最稳定）。
- 如果 `VSLANG=1033` 未生效，先在 Visual Studio Installer 中安装英文语言包（English），再重启 CLion 后重试。

**命令行验证（可选）：**
```powershell
$env:VSLANG=2052
cmake --build build --config Debug

$env:VSLANG=1033
cmake --build build --config Debug

# 如果需要，可显式传递 MSBuild 语言参数
cmake --build build --config Debug -- /p:PreferredUILang=en-US
```

### 1.6 VSCode CMake Tools 输出乱码/中文日志

**现象：**
- VSCode 输出中出现 `閫傜敤浜�`、`姝ｅ湪鎵�鎻忔簮...` 等乱码。
- 或希望在 VSCode 中统一英文日志。

**推荐做法（工作区配置）：**
在项目的 `.vscode/settings.json` 中设置：

```json
{
  "files.encoding": "utf8",
  "cmake.outputLogEncoding": "utf-8",
  "cmake.environment": { "VSLANG": "1033" },
  "cmake.configureEnvironment": { "VSLANG": "1033" },
  "cmake.buildEnvironment": { "VSLANG": "1033" },
  "cmake.buildToolArgs": ["/p:PreferredUILang=en-US"]
}
```

**说明：**
- `cmake.outputLogEncoding` 用于修正 CMake Tools 对外部命令输出的解码。
- `VSLANG=1033` + `PreferredUILang=en-US` 用于将 MSBuild 日志切到英文（需安装 VS 英文语言包）。
- 你看到的 `[main]/[driver]` 前缀文本属于 VSCode/CMake Tools UI 文案，语言跟随 VSCode 显示语言；如需英文 UI，请在 VSCode 执行 `Configure Display Language` 切换到 `en`。

---

## 2. 运行时问题

### 2.1 找不到着色器文件

**错误信息：**
```
Failed to load shader module: shaders/pbr.vert.spv
```

**原因：** 工作目录不正确，着色器未编译，或路径错误。

**解决方案：**

**方法 1: 从项目根目录运行**
```powershell
cd c:\path\to\Tumbler-Vulkan
.\build\src\Examples\Tumbler\Release\Tumbler.exe
```

**方法 2: 在 Visual Studio 中设置工作目录**
1. 右键点击 `Tumbler` 项目 → `属性`
2. `调试` → `工作目录`
3. 设置为 `$(ProjectDir)..\..\..` (相对于可执行文件的项目根目录)

**方法 3: 检查着色器是否编译**
确保 CMake 配置时包含了 `assets/shaders` 子目录，且编译成功生成了 `.spv` 文件。

### 2.2 VK_ERROR_OUT_OF_DATE_KHR

**错误信息：**
```
vkAcquireNextImageKHR failed: VK_ERROR_OUT_OF_DATE_KHR
```

**原因：** 窗口大小改变后，Swapchain 需要重建。

**解决方案：**
这是正常现象，引擎应该会自动调用 `RecreateSwapchain()`。如果崩溃，检查：
- Swapchain 重建流程是否正确
- 旧资源是否在重建前正确清理

### 2.3 验证层错误：命令缓冲正在使用中

**错误信息：**
```
VUID-vkFreeCommandBuffers-pCommandBuffers-00047
vkFreeCommandBuffers(): pCommandBuffers[0] is in use
```

**原因：** 尝试释放或修改 GPU 还在使用的命令缓冲。

**解决方案：**
使用 Fence 等待 GPU 完成，或者**重用命令缓冲**而不是每帧分配/释放。

**正确做法：**
```cpp
// 初始化时分配一次
VkCommandBuffer mainCmdBuffer = AllocateCommandBuffer();

// 每帧
vkWaitForFences(device, 1, &renderFence, VK_TRUE, UINT64_MAX);
vkResetCommandBuffer(mainCmdBuffer, 0);  // 重置，不释放
RecordAndSubmit(mainCmdBuffer);
// 不释放，下一帧继续用
```

### 2.4 验证层错误：描述符集布局不匹配

**错误信息：**
```
VUID-vkCmdBindDescriptorSets-pipelineLayout-00316
```

**原因：** Pipeline 使用的 DescriptorSetLayout 与实际绑定的 DescriptorSet 布局不一致。

**解决方案：**
确保：
1. `FMaterial` 创建 Pipeline 时使用的 Layout
2. `FMaterialInstance` 分配 DescriptorSet 时使用的 Layout
3. `vkCmdBindDescriptorSets` 时传入的 Layout

三者必须是同一个！

### 2.5 黑屏 / 看不到任何东西

可能的原因和排查步骤：

**1. 检查清除颜色**
确认渲染通道的清除颜色不是黑色，或者物体颜色与背景相同。

**2. 检查相机位置**
- 相机是否在物体内部？
- 相机朝向是否正确？
- 近/远裁剪面设置是否合理？

**3. 检查变换矩阵**
打印或用 ImGui 显示 Model/View/Proj 矩阵，确认它们不是零矩阵或包含 NaN。

**4. 禁用背面剔除**
暂时在 Pipeline 中禁用背面剔除，看看物体是否被错误剔除。

**5. 检查深度测试**
尝试禁用深度测试，或者检查深度缓冲是否正确创建。

**6. 使用 RenderDoc**
捕获一帧并在 RenderDoc 中分析：
- 顶点数据是否正确？
- 着色器是否执行？
- 输出的颜色是什么？

### 2.6 Linux / Wayland：`glfwGetRequiredInstanceExtensions` 返回空，或缺少 `VK_KHR_wayland_surface`

**典型日志：**
```text
GLFW platform is Wayland. Vulkan surface extension query failed.
No Wayland Vulkan WSI extension was exposed. Expected VK_KHR_wayland_surface.
```

或：

```text
Crash: glfwGetRequiredInstanceExtensions returned NULL
```

**常见原因：**
1. `glfw3` 没有按 Wayland 方式构建
2. `vulkan-loader` 没有启用 `wayland/xcb/xlib` WSI feature
3. 构建目录还是旧的，`vcpkg.json` 已改但依赖没重新解算

**当前项目要求的 Linux manifest 关键项：**

```json
{
  "name": "glfw3",
  "features": [
    { "name": "wayland", "platform": "linux" }
  ]
},
{
  "name": "vulkan",
  "platform": "windows | linux"
},
{
  "name": "vulkan-loader",
  "platform": "linux",
  "features": ["wayland", "xcb", "xlib"]
}
```

**解决方案：**
1. 删除旧构建目录或至少重新运行 CMake 配置
2. 确认 `vcpkg.json` 包含上面的 Linux 依赖
3. 重新配置并构建：

```bash
cmake -S . -B cmake-build-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake --build cmake-build-debug --target App-Tumbler
```

**验证思路：**
- 日志中应看到 `GLFW Requested Extension: VK_KHR_wayland_surface`
- `vulkan-loader` 的 CMakeCache 中 `BUILD_WSI_WAYLAND_SUPPORT` / `BUILD_WSI_XCB_SUPPORT` / `BUILD_WSI_XLIB_SUPPORT` 应为 `ON`

**实现细节说明：**
- 项目不再依赖 `imgui[vulkan-binding]` 间接把 `vulkan` 带进来，而是在 `vcpkg.json` 中显式声明 Linux 上的 `vulkan`
- Linux 下额外显式声明了 `vulkan-loader[wayland,xcb,xlib]`
- `AppWindow` 内部不是只调用一次 `glfwInit()` 就结束，而是按 Wayland → Any → X11 的顺序逐步尝试，并在失败时输出详细的 Vulkan WSI 扩展诊断

### 2.7 Linux / Wayland：窗口没有标题栏或关闭按钮

**典型日志：**
```text
Failed to load plugin 'libdecor-gtk.so': failed to init
No plugins found, falling back on no decorations
```

**原因：**
- 不一定是系统没装 `libdecor`
- 更常见的是从 Snap Code / VSCode Snap 终端启动时，GTK / GIO / GSettings 环境变量被 Snap 运行时污染，导致 `libdecor-gtk` 初始化失败

**当前引擎行为：**
- 新版本会在 Wayland 下检测 Snap Code 注入环境
- 在 GLFW 初始化前自动清理相关 GTK / GIO / Snap 变量
- 因此新的 `App-Tumbler` 在同一终端环境下也应该恢复正常窗口装饰

**实现细节说明：**
- `AppWindow` 会先检查当前是否是 Wayland 会话
- 如果检测到 `SNAP_NAME=code`，或若干 GTK / GIO / XDG 变量指向 `/snap/code/`
  的运行时，就视为 Snap Code 环境污染
- 代码会优先恢复 `XDG_DATA_DIRS_VSCODE_SNAP_ORIG` / `XDG_CONFIG_DIRS_VSCODE_SNAP_ORIG`
- 然后移除 `GTK_EXE_PREFIX`、`GTK_PATH`、`GSETTINGS_SCHEMA_DIR`、`GIO_MODULE_DIR`、`XDG_DATA_HOME` 等会让 `libdecor-gtk` 误连 Snap 运行时的变量
- 最后强制把 `GDK_BACKEND` 设为 `wayland`

这个修复的重点不是“安装更多包”，而是让原生 GLFW / GTK 进程不要继承 VSCode Snap 自己的桌面运行时环境。

**如果你仍然遇到问题：**
1. 先确认你运行的是最新构建出的可执行文件
2. 尽量从普通系统终端而不是 Snap Code 集成终端启动
3. 检查系统包是否安装：
   - `libdecor-0-0`
   - `libdecor-0-plugin-1-gtk`
   - `libgtk-3-0`
4. 如果需要，使用最小干净环境验证：

```bash
env -i HOME=$HOME USER=$USER LOGNAME=$LOGNAME \
  PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  LANG=$LANG TERM=$TERM \
  XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR \
  XDG_SESSION_TYPE=$XDG_SESSION_TYPE \
  WAYLAND_DISPLAY=$WAYLAND_DISPLAY \
  DISPLAY=$DISPLAY \
  DBUS_SESSION_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS \
  XDG_CURRENT_DESKTOP=$XDG_CURRENT_DESKTOP \
  ./cmake-build-debug/src/Examples/Tumbler/App-Tumbler
```

---

## 3. 资源问题

### 3.1 纹理加载失败

**错误信息：**
```
Failed to load texture: assets/textures/xxx.jpg
```

**可能原因：**
1. 文件路径错误
2. 文件格式不支持（STB 支持常见格式如 JPG、PNG、BMP）
3. 文件损坏

**解决方案：**
- 检查工作目录
- 确认文件存在且路径正确
- 尝试用图片查看器打开文件确认其有效性

### 3.2 模型加载失败

**错误信息：**
```
tinyobjloader error: ...
```

**解决方案：**
- 确认是 Wavefront OBJ 格式
- 检查 OBJ 文件是否附带 MTL 材质文件（可选）
- 尝试用 Blender 等 3D 软件打开并重新导出

### 3.3 内存泄漏

**检测方法：**
1. 使用 Visual Studio 的内存诊断工具
2. 启用 Vulkan 验证层的 `VK_EXT_debug_utils`
3. 检查 VMA 统计信息

**常见泄漏点：**
- Vulkan 对象（Buffer, Image, Pipeline 等）未正确销毁
- 描述符集未返回 Pool
- 资源加载后未释放（应该由 `FAssetManager` 管理）

---

## 4. 性能问题

### 4.1 帧率低

**排查步骤：**

**1. 检查是否在 Debug 模式运行**
Debug 模式下验证层和调试信息会显著降低性能，切换到 Release 模式。

**2. 使用 ImGui 显示 FPS**
```cpp
ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
ImGui::Text("Frame Time: %.3f ms", 1000.0f / ImGui::GetIO().Framerate);
```

**3. 使用 RenderDoc**
- 找出最耗时的绘制调用
- 检查 Draw Call 数量是否过多

**4. 检查 VSync**
Swapchain 的 Present Mode 可能开启了 VSync，导致帧率锁定在显示器刷新率。

### 4.2 卡顿 / 帧率波动

**可能原因：**

**1. 资源在渲染时加载**
确保所有资源在加载阶段预先上传，而不是在渲染时：
```cpp
// ✅ 正确：预加载
renderer.UploadMesh(assetManager.GetOrLoadMesh("Sword").get());

// ❌ 错误：渲染时才上传（会卡顿）
void Render() {
    renderer.UploadMesh(mesh);  // 第一次调用时会卡
}
```

**2. GC 或内存分配**
避免在主循环中进行大量动态内存分配。

**3. 场景中物体过多**
考虑实现视锥体剔除（Frustum Culling）。

---

## 5. 调试工具

### 5.1 RenderDoc

RenderDoc 是 Vulkan 开发必不可少的调试工具。

**使用步骤：**
1. 下载安装 [RenderDoc](https://renderdoc.org/)
2. 在 RenderDoc 中启动程序或附加到正在运行的程序
3. 按 `F12` 或点击 "Capture Frame" 捕获一帧
4. 分析捕获的帧：
   - 查看 API 调用序列
   - 检查资源（Buffer、Image、Shader）
   - 调试着色器
   - 查看像素历史

### 5.2 Vulkan 验证层

**启用验证层（Debug 模式）：**
```cpp
const char* validationLayers[] = {
    "VK_LAYER_KHRONOS_validation"
};
```

验证层会在控制台输出：
- 警告
- 错误
- 最佳实践建议

### 5.3 日志系统

Tumbler 引擎使用 spdlog 进行日志记录：

```cpp
LOG_TRACE("Trace message");
LOG_DEBUG("Debug message");
LOG_INFO("Info message");
LOG_WARN("Warning message");
LOG_ERROR("Error message");
LOG_CRITICAL("Critical message");
```

---

## 6. 寻求帮助

如果以上方案都无法解决你的问题：

1. **检查日志**：查看控制台输出的错误信息和警告
2. **搜索错误代码**：将 Vulkan 错误代码或验证层错误信息复制到搜索引擎
3. **参考官方文档**：
   - [Vulkan 规范](https://www.khronos.org/registry/vulkan/)
   - [Vulkan Tutorial](https://vulkan-tutorial.com/)
4. **检查开发路线图**：查看 `Tumbler_Dev_Plan.md`，问题可能已在计划中解决

---

## 附录：常见 Vulkan 错误码速查

| 错误码 | 含义 | 常见原因 |
|--------|------|----------|
| `VK_ERROR_OUT_OF_HOST_MEMORY` | 主机内存不足 | 系统内存不足 |
| `VK_ERROR_OUT_OF_DEVICE_MEMORY` | 设备内存不足 | 显存不足 |
| `VK_ERROR_INITIALIZATION_FAILED` | 初始化失败 | 驱动/硬件问题 |
| `VK_ERROR_DEVICE_LOST` | 设备丢失 | GPU 重置/崩溃 |
| `VK_ERROR_MEMORY_MAP_FAILED` | 内存映射失败 | 内存类型不兼容 |
| `VK_ERROR_LAYER_NOT_PRESENT` | 层不存在 | 验证层未安装 |
| `VK_ERROR_EXTENSION_NOT_PRESENT` | 扩展不存在 | 硬件不支持该扩展 |
| `VK_ERROR_FEATURE_NOT_PRESENT` | 特性不存在 | 硬件不支持该特性 |
| `VK_ERROR_INCOMPATIBLE_DRIVER` | 驱动不兼容 | Vulkan 版本不匹配 |
| `VK_ERROR_SURFACE_LOST_KHR` | Surface 丢失 | 窗口被销毁 |
| `VK_ERROR_NATIVE_WINDOW_IN_USE_KHR` | 原生窗口被占用 | 窗口已被其他 API 使用 |
| `VK_ERROR_OUT_OF_DATE_KHR` | Swapchain 过期 | 窗口大小改变 |
| `VK_ERROR_INCOMPATIBLE_DISPLAY_KHR` | 显示器不兼容 | 显示模式问题 |
