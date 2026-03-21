# 快速入门指南 (Getting Started)

本文档将帮助你快速搭建 Tumbler 引擎的开发环境并成功编译运行。

## 1. 系统要求

- **操作系统**: Windows 10/11
- **编译器**: Visual Studio 2022 (MSVC)
- **CMake**: 3.28 或更高版本
- **Vulkan SDK**: 最新版本
- **Git**: 用于版本控制

## 2. 依赖安装

### 2.1 安装 Visual Studio 2022

1. 下载并安装 [Visual Studio 2022 Community](https://visualstudio.microsoft.com/downloads/)
2. 在安装时选择 **"使用 C++ 的桌面开发"** 工作负载
3. 确保安装了 **C++ CMake tools for Windows**

### 2.2 安装 Vulkan SDK

1. 访问 [LunarG Vulkan SDK](https://vulkan.lunarg.com/sdk/home)
2. 下载并安装最新的 Windows 版本
3. 安装完成后，验证环境变量 `VULKAN_SDK` 是否已设置

### 2.3 安装 vcpkg

本项目使用 vcpkg 管理第三方依赖：

```powershell
# 克隆 vcpkg 仓库
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# 引导脚本（Windows）
.\bootstrap-vcpkg.bat

# 集成 vcpkg 到 CMake
.\vcpkg integrate install
```

设置环境变量 `VCPKG_ROOT` 指向你的 vcpkg 安装目录。

## 3. 获取项目代码

```powershell
git clone <你的仓库地址>
cd Tumbler-Vulkan
```

## 4. 构建项目

### 4.1 使用 CMake 配置

```powershell
# 创建构建目录
mkdir build
cd build

# 配置 CMake (Release 模式)
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake

# 或者使用 Debug 模式
cmake .. -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake
```

### 4.2 编译

```powershell
# 编译项目
cmake --build . --config Release

# 或者 Debug 模式
cmake --build . --config Debug
```

### 4.3 使用 Visual Studio 打开

如果你更喜欢使用 IDE：

```powershell
# 生成 Visual Studio 解决方案
cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%/scripts/buildsystems/vcpkg.cmake

# 打开生成的解决方案
start Tumbler-Engine.sln
```

在 Visual Studio 中：
1. 设置 `Tumbler` 为启动项目
2. 选择配置（Debug/Release）
3. 按 F5 或点击"本地 Windows 调试器"运行

## 5. 运行示例

编译成功后，可执行文件位于：
- `build/src/Examples/Tumbler/Release/Tumbler.exe`
- `build/src/Examples/Tumbler/Debug/Tumbler.exe`

### 5.1 控制方式

- **WASD**: 移动相机
- **QE**: 上下移动
- **鼠标拖动**: 旋转视角
- **ImGui 面板**: 调整光源位置、颜色、强度等参数

## 6. 项目结构

```
Tumbler-Vulkan/
├── assets/              # 资源文件（模型、着色器、纹理）
│   ├── models/         # 3D 模型文件
│   ├── shaders/        # GLSL 着色器
│   └── textures/       # 纹理图片
├── docs/               # 文档目录
├── src/
│   ├── Core/           # 引擎核心代码
│   │   ├── Assets/     # 资源管理
│   │   ├── Editor/     # 编辑器相关
│   │   ├── GameSystem/ # 游戏系统（ECS、场景）
│   │   ├── Geometry/   # 几何处理
│   │   ├── Graphics/   # 渲染系统
│   │   ├── Platform/   # 平台抽象
│   │   └── Utils/      # 工具类
│   └── Examples/       # 示例项目
├── CMakeLists.txt      # 主 CMake 配置
├── vcpkg.json          # vcpkg 依赖清单
└── Tumbler_Dev_Plan.md # 开发路线图
```

## 7. 下一步

- 阅读 [架构概览](01_Architecture_Overview.md) 了解引擎整体设计
- 查看 [资源管理](02_Asset_Management.md) 学习如何加载和管理资源
- 探索 [材质系统](03_Material_System.md) 理解 PBR 材质的工作原理
- 参考 [开发路线图](../Tumbler_Dev_Plan.md) 了解未来计划

## 8. 常见问题

### Q: CMake 找不到 Vulkan？
A: 确保已正确安装 Vulkan SDK 并设置了 `VULKAN_SDK` 环境变量。

### Q: 编译错误 "必须使用 MSVC 编译器"？
A: 本项目强制要求使用 MSVC。在 CLion 或其他 IDE 中，请确保 Toolchain 设置为 Visual Studio。

### Q: vcpkg 依赖安装失败？
A: 尝试运行 `vcpkg upgrade --no-dry-run` 更新 vcpkg，然后重新配置 CMake。

### Q: 运行时找不到着色器文件？
A: 确保工作目录设置正确，或者从项目根目录运行可执行文件。
