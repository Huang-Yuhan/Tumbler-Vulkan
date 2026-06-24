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

**前置要求：** 确保以下环境变量已设（建议写入 `~/.zshrc` 或 `~/.bashrc`）：

```bash
export VCPKG_ROOT=$HOME/vcpkg
export VULKAN_SDK=$HOME/.local/vulkan-sdk/1.4.350.1/x86_64  # 按实际路径调整
export LD_LIBRARY_PATH=$VULKAN_SDK/lib/VulkanLoader/lib:$LD_LIBRARY_PATH
export PATH=$VULKAN_SDK/bin:$PATH
export VK_ADD_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d
```

> ⚠️ `VK_ADD_LAYER_PATH` 必须设置，否则 validation layer 不可用，Vulkan 测试全部失败。
>
> ⚠️ 部分 vcpkg 包提供的是共享库（如 `libslang-compiler.so`），IDE 中运行测试需额外配置 `LD_LIBRARY_PATH`（含 vcpkg installed lib 路径），详见 `docs/troubleshooting.md`。

**构建：**

```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DBUILD_TESTING=ON

cmake --build build-linux
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
├── assets/              # 模型、场景、纹理
│   ├── models/
│   ├── scenes/
│   └── textures/
├── docs/                # 文档
├── src/
│   ├── Core/
│   │   ├── Assets/      # 资产数据库 (AssetDatabase)
│   │   ├── Engine/      # 引擎编排、生命周期
│   │   ├── GameSystem/  # ECS (FActor, FScene, Components)
│   │   ├── Graphics/    # Vulkan 基础设施
│   │   ├── Math/        # 数学库
│   │   ├── Platform/    # 窗口 + Surface (AppWindow)
│   │   ├── Scene/       # 场景加载 (SceneLoader)
│   │   └── Utils/       # 日志
│   ├── Tools/
│   │   └── AssetImporter/  # 资产导入 CLI
│   └── Examples/
│       └── Tumbler/     # 主示例
├── tests/               # 单元测试
├── CMakeLists.txt
└── vcpkg.json
```

## 6. 下一步

- [ECS 架构](../architecture/ecs.md) — 理解实体-组件系统
- [设计决策](../architecture/decisions.md) — 理解关键取舍
- [GPU-Driven 开发计划](../gpu-driven-dev-plan.md) — Nanite 管线开发进度
- [编码规范](../standards/coding-style.md) — 命名、日志、架构约束
