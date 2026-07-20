# 编码规范

C++26 标准（GCC 16+），GNU 扩展关闭。

## 命名

- **类型**：PascalCase — `AppWindow`, `GpuBuffer`, `DeletionQueue`
- **函数**：PascalCase — `Init()`, `ShouldClose()`, `AcquireNextImage()`
- **成员变量**：`m_` 前缀 + PascalCase — `m_Window`, `m_Device`
- **局部变量**：camelCase — `imageCount`, `queueFamily`
- **常量**：`k` 前缀 + PascalCase 或 `UPPER_SNAKE_CASE` — `kMaxFramesInFlight`

## 文件组织

每个模块一个 `.h` + `.cpp`，文件名与主类名一致（`AppWindow.h` / `AppWindow.cpp`）。目录按架构分层：

```
src/Core/      — 平台无关基础设施
src/Gfx/       — Vulkan 基础设施
src/Render/    — GPU-Driven 渲染管线
src/Assets/    — 场景和资产加载
src/UI/        — ImGui 调试面板
```

## 错误处理：`std::expected<T, E>` (C++23)

所有可能失败的函数返回 `std::expected`，不使用 `bool` + 输出参数：

```cpp
enum class WindowError { GLFWInitFailed, WindowCreationFailed };

std::expected<void, WindowError> Init(int width, int height, const char* title);
std::expected<VkSurfaceKHR, WindowError> CreateSurface(VkInstance instance) const;
```

调用端：

```cpp
auto result = window.Init(1920, 1080, "Tumbler");
if (!result) {
    LOG_ERROR("Window init failed: {}", static_cast<int>(result.error()));
    return 1;
}
```

## 数据传递：`std::span<T>` (C++20)

传递连续数据时使用 `std::span`，不传裸指针 + 长度：

```cpp
void UploadVertices(std::span<const Vertex> vertices);
void UploadIndices(std::span<const uint32_t> indices);
```

## 位重解释：`std::bit_cast<T>` (C++20)

```cpp
float depth = 0.5f;
uint32_t depthInt = std::bit_cast<uint32_t>(depth);
// 禁止：reinterpret_cast, union 类型双关
```

## Vulkan 结构体：designated initializers (C++20)

```cpp
VkCommandBufferAllocateInfo allocInfo{
    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
    .commandPool = pool,
    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
    .commandBufferCount = 1,
};
// 禁止：memset 清零 + 逐字段赋值
```

## 可选值：`std::optional<T>` (C++17)

```cpp
std::optional<MeshHandle> FindMesh(const std::string& name);
// 禁止：nullptr 表示"不存在"
```

## 编译期常量：`constexpr` 替代 `#define`

```cpp
static constexpr uint32_t kMaxFramesInFlight = 2;
// 字符串常量仍可用宏
```

## 日志

```cpp
LOG_TRACE("...")   // 调试细节
LOG_INFO("...")    // 常规信息
LOG_WARN("...")    // 非致命警告
LOG_ERROR("...")   // 错误
```

禁止 `std::cout` / `std::cerr` / `printf`。

## 格式化

clang-format，LLVM 风格，4 空格缩进，120 列宽。
