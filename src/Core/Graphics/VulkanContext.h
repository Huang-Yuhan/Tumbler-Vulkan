#pragma once

#include <vulkan/vulkan.h>
#include <vk_mem_alloc.h>
#include <vector>
#include <optional>

// 前置声明，避免循环引用
class AppWindow;


struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    [[nodiscard]] bool isComplete() const
    {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};
/**
 * @brief VulkanContext 类
 * * 核心职责：管理 Vulkan 的基础上下文环境。
 * 它负责初始化 Vulkan 的“全局”对象（Instance, Device, PhysicalDevice）
 * 以及内存分配器（VMA）。它是整个渲染引擎的地基。
 */
class VulkanContext
{
public:
    VulkanContext() = default;

    // 析构时确保资源释放，遵循 RAII 原则，但在 Vulkan 中通常需要显式调用 Cleanup
    ~VulkanContext() { Cleanup(); }

    /**
     * @brief 初始化 Vulkan 上下文
     * @param window 指向应用程序窗口的指针，用于创建 Surface (WSI)
     */
    void Init(AppWindow* window);

    /**
     * @brief 销毁所有 Vulkan 资源
     * 必须按与创建相反的顺序销毁资源 (Allocator -> Device -> Surface -> Instance)
     */
    void Cleanup();

    // ==========================================
    // Getter (访问器)
    // ==========================================

    // 获取 Vulkan 实例 (所有 Vulkan 操作的根节点)
    [[nodiscard]] VkInstance GetInstance() const { return Instance; }

    // 获取逻辑设备 (用于创建 Buffer, Image, Shader 等资源的主要接口)
    [[nodiscard]] VkDevice GetDevice() const { return Device; }

    // 获取物理设备 (代表真实的 GPU 硬件信息)
    [[nodiscard]] VkPhysicalDevice GetPhysicalDevice() const { return PhysicalDevice; }

    // 获取图形队列 (用于提交绘制命令)
    [[nodiscard]] VkQueue GetGraphicsQueue() const { return GraphicsQueue; }
    [[nodiscard]] VkQueue GetPresentQueue() const { return PresentQueue; }

    // 获取图形队列族索引 (在创建 CommandPool 时需要用到)
    [[nodiscard]] uint32_t GetGraphicsQueueFamily() const { return GraphicsQueueFamilyIndex; }

    [[nodiscard]] uint32_t GetPresentQueueFamily() const { return PresentQueueFamilyIndex; }

    // 获取 VMA 内存分配器 (用于高效分配显存)
    [[nodiscard]] VmaAllocator GetAllocator() const { return Allocator; }

    // 获取渲染表面 (连接 Vulkan 和显示器的桥梁)
    [[nodiscard]] VkSurfaceKHR GetSurface() const { return Surface; }

private:
    // 1. 实例：连接应用程序和 Vulkan 库，加载 Validation Layers
    VkInstance Instance = VK_NULL_HANDLE;

    // 2. 调试信使：用于接收 Validation Layers 的报错和警告回调
    VkDebugUtilsMessengerEXT DebugMessenger = VK_NULL_HANDLE;

    // 3. 表面：窗口系统集成 (WSI)，由 GLFW/SDL 等库协助创建，代表屏幕上的绘图区域
    VkSurfaceKHR Surface = VK_NULL_HANDLE;

    // 4. 物理设备：选择的显卡硬件 (如 RTX 4070 Ti)
    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;

    // 5. 逻辑设备：应用程序看到的 GPU 接口，开启了特定特性 (Features) 和 扩展 (Extensions)
    VkDevice Device = VK_NULL_HANDLE;

    // 6. 图形队列：从逻辑设备中获取的句柄，用于执行渲染命令
    VkQueue GraphicsQueue = VK_NULL_HANDLE;
    VkQueue PresentQueue = VK_NULL_HANDLE;

    // 队列族索引：记录图形队列属于哪个家族 (Family)
    uint32_t GraphicsQueueFamilyIndex = 0;

    uint32_t PresentQueueFamilyIndex = 0;

    // 7. VMA 分配器：Vulkan Memory Allocator 库的句柄，用来简化极其复杂的显存管理
    VmaAllocator Allocator = VK_NULL_HANDLE;

    // ==========================================
    // 内部初始化流程 (按调用顺序排列)
    // ==========================================

    // 创建 VkInstance，启用全局扩展和校验层
    void CreateInstance();

    // 配置并创建 DebugMessenger (仅在 Debug 模式下生效)
    void SetupDebugMessenger();

    // 遍历所有可用 GPU，根据评分机制选择最合适的一块 (通常是独显)
    void PickPhysicalDevice();

    // 基于选择的物理设备，创建逻辑设备 (Logical Device) 和 队列 (Queues)
    void CreateLogicalDevice();

    // 初始化 VMA 库，准备进行内存分配
    void InitVMA();

    // ==========================================
    // 工具函数
    // ==========================================

    // 检查请求的 Validation Layers (如 VK_LAYER_KHRONOS_validation) 是否可用
    static bool CheckValidationLayerSupport();

    // 获取创建 Instance 所需的所有扩展 (包括 GLFW 需要的表面扩展 + Debug 扩展)
    static std::vector<const char*> GetRequiredExtensions();

    static VkDebugUtilsMessengerCreateInfoEXT PopulateDebugMessengerCreateInfo();

    static QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface);
};
