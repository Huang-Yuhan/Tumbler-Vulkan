# 快速入门指南

## 1. 系统要求

- **操作系统**: Windows 10/11 或 Linux (Wayland/X11)
- **编译器**: Visual Studio 2022 (MSVC) 或 GCC 14+ / Clang 19+
- **CMake**: 3.28+
- **Vulkan SDK**: 1.3+ (需要 Vulkan 1.2 核心特性)
- **GPU**: 支持 Vulkan 1.3 的显卡（需 `bufferDeviceAddress`、`descriptorIndexing`、`drawIndirectCount`）

## 2. 依赖安装

### 2.1 vcpkg

```powershell
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg integrate install
```

设置环境变量 `VCPKG_ROOT` 指向 vcpkg 安装目录。

### 2.2 Vulkan SDK

访问 [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home) 下载安装。

## 3. 构建

### Windows (MSVC)

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_TESTING=ON

cmake --build build --config Debug --parallel
```

### Linux (Ninja)

```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DBUILD_TESTING=ON

cmake --build build-linux --target App-Tumbler
```

## 4. 运行

编译成功后，可执行文件位于：
- Windows: `build/src/Examples/Tumbler/Debug/Tumbler.exe`
- Linux: `build-linux/src/Examples/Tumbler/App-Tumbler`

从项目根目录运行，确保能找到 `assets/` 目录。

### 控制方式

- **WASD**: 移动相机
- **QE**: 上下移动
- **鼠标拖动**: 旋转视角
- **ImGui 面板**: 查看性能统计、可见实例数

## 5. 项目结构

```
Tumbler-Vulkan/
├── assets/              # 模型、着色器、纹理
│   ├── models/
│   ├── shaders/engine/
│   └── textures/
├── docs/                # 文档
├── src/
│   ├── Core/
│   │   ├── Platform/    # 窗口 + Surface
│   │   ├── Graphics/    # Vulkan 基础设施 + 渲染 Pass
│   │   ├── Scene/       # 场景管理 + 相机
│   │   ├── Editor/      # ImGui 调试面板
│   │   └── Utils/       # 日志、Vulkan 工具、数学
│   └── Examples/
│       └── Tumbler/     # 主示例
├── tests/               # 单元测试
├── CMakeLists.txt
└── vcpkg.json
```

## 6. 下一步

- [架构概览](../architecture/overview.md) — 理解 GPU-Driven 设计
- [渲染架构](../architecture/rendering.md) — 理解帧循环和 GPU 数据流
- [设计决策](../architecture/decisions.md) — 理解关键取舍
