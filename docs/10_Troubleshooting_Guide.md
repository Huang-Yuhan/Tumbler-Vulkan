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
