# Tumbler 编码规范

本文档规定 Tumbler 引擎项目的编码风格与约定。规范适用于 `src/Core/`、`src/Examples/`、`src/Tools/`、`tests/` 下所有 C++ 代码。

---

## 1. 格式化

使用 `clang-format`（配置见仓库根目录 `.clang-format`）：

- **风格**: LLVM
- **缩进**: 4 空格（禁止 Tab）
- **C++ 标准**: C++20
- **列宽**: 120
- **大括号**: 函数/类左大括号另起一行，控制流左大括号同行

提交前运行：

```bash
find src tests -name "*.h" -o -name "*.cpp" | xargs clang-format -i
```

CI pre-commit hook 会自动检查。

---

## 2. 头文件

### 2.1 Include Guard

统一使用 `#pragma once`。

```cpp
#pragma once

namespace Tumbler {
// ...
}
```

### 2.2 Include 顺序

从上到下：

1. 对应的头文件（.cpp 文件的第一行）
2. 项目内其他头文件（`"Core/..."`）
3. 第三方库（`<vulkan/...>`、`<glm/...>`）
4. C++ 标准库（`<memory>`、`<string>`、`<vector>`）

各组之间用空行分隔。

```cpp
#include "Engine.h"

#include "Core/Assets/AssetDatabase.h"
#include "Core/Graphics/VulkanContext.h"
#include "Core/Platform/AppWindow.h"
#include "Core/Utils/Log.h"

#include <vulkan/vulkan.h>

#include <memory>
#include <string>
```

### 2.3 前向声明

头文件中优先使用前向声明，避免不必要的 include 传递。

```cpp
// Engine.h — 用前向声明
namespace Tumbler {
class AppWindow;
class VulkanContext;
class RenderDevice;

class Engine { /* ... */ };
}
```

### 2.4 注释头

每个头文件顶部写一行文件职责描述：

```cpp
// VulkanContext.h — Vulkan 实例与设备管理
//
// 职责: 创建 Vulkan 1.4 Instance + Device, 选择物理设备和 Queue 族
//
// 依赖: VulkanUtils.h
// 层级: 图形基础设施 (Phase 2)
```

---

## 3. 命名

### 3.1 命名空间

- **新代码**: `namespace Tumbler { ... }`
- **旧 ECS 层**: 全局命名空间（`::FActor`、`::FScene`、`::Component`、`::CTransform`）
- 在 Tumbler 命名空间内引用旧 ECS 类型时，加 `::` 前缀：`::FActor*`、`::FScene&`
- 不使用 `using namespace` 在头文件中

### 3.2 类 / 结构体

| 类型 | 命名规则 | 示例 |
|------|----------|------|
| 新模块类 | PascalCase，无前缀 | `Engine`、`VulkanContext`、`AssetDatabase` |
| 旧 ECS Actor | `F` 前缀 | `FActor`、`FScene` |
| 旧 ECS Component | `C` 前缀 | `CTransform`、`CStaticMesh` |
| 纯数据结构 | PascalCase，`struct` | `BufferHandle`、`MeshHandle`、`EngineConfig` |
| 接口类 | `I` 前缀 | `IRenderPipeline`（待实现） |

### 3.3 成员变量

```cpp
class Example {
private:
    int m_Count = 0;            // m_ 前缀
    float m_DeltaTime = 0.0f;   // 驼峰命名
    bool m_bInitialized = false; // bool 加 b 前缀
    void* m_Ptr = nullptr;      // 指针 m_ 前缀，PascalCase

    VkDevice m_Device = VK_NULL_HANDLE; // Vulkan 句柄无特殊前缀
};
```

### 3.4 函数

- **公开方法**: PascalCase — `Init()`、`Shutdown()`、`GetDevice()`
- **私有方法**: PascalCase — `CreateInstance()`、`DestroySwapchainObjects()`
- **简单 getter**: 内联在头文件，`const` 修饰 — `VkDevice GetDevice() const { return m_Device; }`
- **自由函数**: PascalCase — `BytesPerPixel()`、`ComputeSimpleFileHash()`

### 3.5 局部变量与参数

```cpp
// 局部变量：camelCase
int imageCount = 0;
VkResult result = VK_SUCCESS;

// 函数参数：camelCase
bool Init(VkDevice device, uint32_t graphicsQueueFamily);

// 输出参数：out 前缀（指针）
bool LoadFromFile(FScene& scene, const std::string& jsonPath, Result& outResult);
```

### 3.6 常量与枚举

```cpp
// 编译期常量：k 前缀
static constexpr bool kEnableValidationLayers = true;

// 枚举成员：大写蛇形
enum class ETextureFormat : uint8_t {
    R8G8B8A8_UNORM = 0,
    R8G8B8A8_SRGB = 3,
    Count
};

// Magic Number：大写蛇形
inline constexpr uint32_t TMESH_MAGIC = 0x48534D54;
```

---

## 4. 日志

**强制使用项目 LOG 宏**，禁止 `std::cout` / `std::cerr` / `printf`。

```cpp
#include "Core/Utils/Log.h"

// 按严重程度选择
LOG_TRACE("...");     // 开发调试
LOG_DEBUG("...");     // 调试版本
LOG_INFO("...");      // 正常运行信息
LOG_WARN("...");      // 可恢复的异常
LOG_ERROR("...");     // 错误但继续运行
LOG_CRITICAL("...");  // 致命错误，即将退出
```

日志消息使用 fmt 格式（spdlog 风格）：

```cpp
LOG_INFO("Loaded scene '{}': {} meshes, {} lights", sceneName, meshCount, lightCount);
LOG_ERROR("Failed to open file: {}", path);
```

不需要在消息中手动加模块前缀 `[Engine]`——spdlog 会自动输出时间戳、日志级别和调用位置。

---

## 5. 架构约束

### 5.1 禁止单例

不使用 Singleton 模式。子系统由 `Engine` 以 `std::unique_ptr` 持有，通过指针/引用注入消费者。

```cpp
// Engine.cpp
m_VulkanContext = std::make_unique<VulkanContext>();
m_RenderDevice = std::make_unique<RenderDevice>();
m_RenderDevice->Init(m_VulkanContext->GetInstance(), /* ... */);
```

### 5.2 RAII 与生命周期

- 所有资源用 RAII 管理，构造函数不能失败
- 两阶段初始化：默认构造 → `Init()` → `Shutdown()` → 析构
- `Shutdown()` 中按创建相反顺序销毁资源
- 禁止拷贝（`= delete`），允许移动

```cpp
class VulkanContext {
public:
    bool Init(AppWindow* window);
    void Shutdown();

    VulkanContext() = default;
    ~VulkanContext() = default;
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
};
```

### 5.3 模块职责

每个文件只做一件事。参考架构分层：

- `Core/Platform/` — OS/窗口层，不依赖 Vulkan
- `Core/Graphics/` — GPU 资源，不依赖 ECS / 场景
- `Core/GameSystem/` — ECS 组件，不依赖 Graphics
- `Core/Engine/` — 编排层，注入依赖
- `Core/Scene/` — 场景加载/序列化
- `Core/Assets/` — 资产数据库

---

## 6. 错误处理

- `Init()` 返回 `bool`，调用方检查并传播失败
- 错误必须通过 `LOG_ERROR()` 记录
- 不使用异常（已有代码中的 JSON 异常除外，在 catch 块内转换为 LOG + return false）
- Vulkan 调用使用 `VK_CHECK` 宏：

```cpp
#define VK_CHECK(expr)                    \
    do {                                  \
        VkResult __res = (expr);          \
        if (__res != VK_SUCCESS) {        \
            LOG_ERROR("Vulkan error: {} returned {}", #expr, static_cast<int>(__res)); \
            assert(false);                \
        }                                 \
    } while (0)
```

---

## 7. 注释

- 类和公共接口用中文描述职责
- 关键算法引用来源（如 "Gribb/Hartmann 视锥体提取"）
- 参考 UE 的实现注明 UE 类型名（如 "对标 UE FPackedCluster"）
- 不做的事（Non-goals）在头文件中注明

```cpp
// ============================================================================
// Engine — 生命周期编排 + 主循环驱动
// ============================================================================
// 职责: 按依赖顺序创建/销毁所有子系统，提供依赖注入，驱动主循环。
// 不设单例，由 main() 创建在栈上。
class Engine { /* ... */ };
```

---

## 8. Vulkan 约定

- 句柄成员不参与 C++ 析构——必须在 `Shutdown()` 中显式销毁
- 初始化时保存所有需要的 Queue Family Index，不在运行时重复查询
- 图像布局转换通过 `CommandManager::TransitionImageLayout()` 统一处理
- 同步原语（Semaphore/Fence）由调用方管理，子系统不内部创建长期同步对象
- VMA 分配必须通过 `RenderDevice` 接口，不直接调 `vmaCreateBuffer`

---

## 9. 测试

- 测试文件放在 `tests/unit/`，与源码路径对应
- 单元测试无需 GPU（链接 TumblerCore），可使用 `TumblerUnitTests` 目标
- 图形测试放在 `tests/unit/Graphics/`（需要 Vulkan 环境）
- 测试命名：`ModuleNameTest.SubTest`
- 测试属性用 `LABELS "unit"` 标记

---

## 10. Git

- 分支：功能分支 → `nanite-integration` → `main`
- Commit message: 英文，格式 `<type>: <description>`（如 `feat:`、`fix:`、`refactor:`）
- 每个 commit 末尾加 `Co-Authored-By: Claude <noreply@anthropic.com>`
- `cooked/` 目录在 `.gitignore`，不提交烘焙资产
