# 开发机器配置记录

每台用于开发 Tumbler 引擎的机器在此记录关键路径和配置，方便跨机器协作。

---

## 机器 A：IcecreamSarkaz-Desktop

**角色**: 主力开发机

**OS**: Windows 11 Pro 10.0.26200

**GPU**: NVIDIA GeForce RTX 4070 Ti SUPER (Vulkan 1.4)

### 工具链路径

| 工具 | 路径 |
|------|------|
| CMake | `C:\Program Files\CMake\bin\cmake.exe` |
| Visual Studio | `C:\Program Files\Microsoft Visual Studio\2022\Community` |
| vcpkg root | `C:\Users\Icecream_Sarkaz\.vcpkg-clion\vcpkg` |
| vcpkg triplet | `x64-windows` |
| Windows SDK | `10.0.26100.0` |

### 参考引擎

| 引擎 | 路径 |
|------|------|
| Unreal Engine 5 源码 | `C:\UnrealEngine\` |
| UE Nanite 渲染器 | `C:\UnrealEngine\Engine\Source\Runtime\Renderer\Private\Nanite\` |

### 环境变量 (PowerShell)

```powershell
$env:VCPKG_ROOT = "C:\Users\Icecream_Sarkaz\.vcpkg-clion\vcpkg"
```

### CMake 配置命令

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_TESTING=ON
```

---

## 机器 B：LAPTOP-LMLQIAPM

**角色**: Windows 笔记本开发机

**OS**: Windows 10 Home China 10.0.19045 (64-bit)

**GPU**: NVIDIA GeForce RTX 3060 Laptop GPU (6 GB VRAM, Vulkan 1.4.325, Driver 591.86)

**RAM**: 16 GB

**IDE**: CLion 2026.1

### 工具链路径

| 工具 | 路径 |
|------|------|
| CMake | `F:\cmake\bin\cmake.exe` (v4.1.0) |
| Visual Studio 2022 | `F:\VS2022\`（MSVC 14.44.35207） |
| cl.exe | `F:\VS2022\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe` |
| vcvarsall | `F:\VS2022\VC\Auxiliary\Build\vcvarsall.bat` |
| vcpkg root | `F:\vcpkg`（CLion 2026.1 管理） |
| vcpkg triplet | `x64-windows` |
| Vulkan SDK | `C:\VulkanSDK\1.4.321.1` |
| Windows SDK | `10.0.26100.0` |
| glslc | `C:\VulkanSDK\1.4.321.1\Bin\glslc.exe`（随 Vulkan SDK） |
| clang-format | `F:\VS2022\VC\Tools\Llvm\x64\bin\clang-format.exe`（随 VS 2022） |
| Git | `Huang_Yuhan` / `huang_yuhan@sjtu.edu.cn` |

### 参考引擎

| 引擎 | 路径 | 备注 |
|------|------|------|
| Unreal Engine 5 源码 | — | ⚠️ 无 Git 仓库版本，`F:\UE_Engine\UE_5.7\` 为非 git 管理的 5.7.3 解压目录，不适合做源码调研 |

| Shader 参考 | 路径 |
|------|------|
| UE Nanite Shader | —（无可用 UE 源码） |

### 磁盘

| 分区 | 总容量 | 可用 |
|------|--------|------|
| F: | 932 GB | ~582 GB |

仓库位于 `F:\GitRepo\Tumbler-Vulkan\`。

### 环境变量 (PowerShell)

```powershell
$env:VCPKG_ROOT = "F:\vcpkg"
```

### CMake 配置命令

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" `
  -DVCPKG_TARGET_TRIPLET=x64-windows `
  -DBUILD_TESTING=ON
```

---

## 机器 Alpha：alpha

**角色**: Linux 开发机 + AI 辅助

**OS**: Arch Linux (7.0.12-arch1-1)

**GPU**: Intel(R) Graphics (RPL-P) — 集成显卡，Mesa 26.1.3

**IDE**: VS Code (clangd + CMake Tools)

**RAM**: 不详

### 工具链路径

| 工具 | 路径 |
|------|------|
| CMake | `/usr/sbin/cmake` (v4.3.4) |
| GCC | `/usr/sbin/g++` (16.1.1) |
| clangd | `/usr/sbin/clangd` (22.1.6) |
| clang-format | `/usr/sbin/clang-format` (22.1.6) |
| Ninja | `/usr/sbin/ninja` (1.13.2) |
| vcpkg root | `~/vcpkg` |
| vcpkg triplet | `x64-linux` |
| Vulkan SDK | `~/.local/vulkan-sdk/1.4.350.1/x86_64` |
| slangc | 由 vcpkg shader-slang 包提供，位于 `build-linux/vcpkg_installed/x64-linux/tools/shader-slang/slangc` (v2026.7.1) |

### Vulkan 特性支持

| 特性 | 支持 |
|------|:----:|
| Vulkan API 版本 | 1.4.348 |
| `bufferDeviceAddress` | ✅ |
| `descriptorIndexing` | ✅ |
| `drawIndirectCount` | ✅ |
| `shaderBufferInt64Atomics` | ✅ |
| `shaderImageInt64Atomics` | ✅ |
| `shaderSharedInt64Atomics` | ❌ (需 2-stage 32-bit fallback) |

### 环境变量 (zsh)

```bash
export VCPKG_ROOT=$HOME/vcpkg
export VULKAN_SDK=$HOME/.local/vulkan-sdk/1.4.350.1/x86_64
export LD_LIBRARY_PATH=$VULKAN_SDK/lib/VulkanLoader/lib:$LD_LIBRARY_PATH
export PATH=$VULKAN_SDK/bin:$PATH
export VK_ADD_LAYER_PATH=$VULKAN_SDK/share/vulkan/explicit_layer.d
```

⚠️ **注意**: `VK_ADD_LAYER_PATH` 必须设置，否则 Vulkan validation layer 不可用。

### CMake 配置命令

```bash
cmake -S . -B build-linux -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DBUILD_TESTING=ON

cmake --build build-linux
```

### 仓库

路径: `/home/alpha/Tumbler-Vulkan`  
Clone: `git clone git@github.com:Huang-Yuhan/Tumbler-Vulkan.git`

### 参考引擎

无 UE5 源码。

---

## 模板

新增机器时复制以下模板：

```markdown
## 机器 N：<hostname>

**角色**: <用途>
**OS**: <系统版本>
**GPU**: <型号>

### 工具链路径

| 工具 | 路径 |
|------|------|
| CMake | |
| 编译器 | |
| vcpkg root | |
| Vulkan SDK | |

### 参考引擎（可选）

| 引擎 | 路径 |
|------|------|
| | |

### 环境变量

### CMake 配置命令
```
