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
