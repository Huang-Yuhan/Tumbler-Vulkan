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

## 机器 B：待添加

**角色**: (填入此机器的用途)

**OS**: (Linux / macOS / Windows)

**GPU**: (型号)

### 工具链路径

| 工具 | 路径 |
|------|------|
| CMake | |
| 编译器 | |
| vcpkg root | |
| Vulkan SDK | |

### 环境变量

```bash
# Linux/macOS
```

### CMake 配置命令

```bash
# Linux (Ninja)
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
