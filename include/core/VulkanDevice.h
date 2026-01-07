#pragma once

#include <array>
#include <vulkan/vulkan.h>
#include <vector>
#include <optional>

namespace Tumbler {
    /**
     * @brief 用于存储从物理设备中筛选出的队列族索引。
     * * 在 Vulkan 中，我们需要查找特定的队列族来支持绘图 (Graphics) 和显示 (Present)。
     * 这两个功能可能由同一个队列族支持，也可能分开。
     * 使用 std::optional 是为了区分“索引为0”和“尚未找到索引”的情况。
     */
    struct QueueFamilyIndices {
        /** * @brief 支持图形指令 (VK_QUEUE_GRAPHICS_BIT) 的队列族索引。
         */
        std::optional<uint32_t> graphicsFamily;

        /** * @brief 支持将图像呈现到窗口表面 (Surface) 的队列族索引。
         */
        std::optional<uint32_t> presentFamily;

        /**
         * @brief 检查是否找到了所有必需的队列族。
         * @return 如果 Graphics 和 Present 都有值，则返回 true。
         */
        [[nodiscard]]
        bool isComplete() const {
            return graphicsFamily.has_value() && presentFamily.has_value();
        }
    };

/**
 * @brief 封装 Vulkan 的物理设备与逻辑设备管理。
 * * @details
 * 核心职责：
 * 1. **物理设备 (Physical Device)**: 代表实体显卡硬件。只读，用于查询功能。
 * 2. **逻辑设备 (Logical Device)**: 与物理设备交互的软件接口，用于创建所有资源。
 * 3. **队列管理**: 自动查找并初始化 Graphics 和 Present 队列。
 * 4. **指令池**: 预创建一个主 CommandPool 用于后续的 Buffer/Image 操作。
 * * 通常在整个应用程序生命周期内只需要一个 VulkanDevice 实例。
 */
class VulkanDevice {
public:
    /**
     * @brief 初始化 Vulkan 设备。
     * * @param instance Vulkan 实例，用于枚举系统中的物理显卡。
     * @param surface  窗口表面，用于检查显卡是否支持将图像呈现到该窗口 (Presentation Support)。
     * @throw std::runtime_error 如果找不到合适的显卡。
     */
    VulkanDevice(VkInstance instance, VkSurfaceKHR surface);

    /**
     * @brief 析构函数。自动销毁逻辑设备和命令池。
     * @note 物理设备句柄不需要销毁。
     */
    ~VulkanDevice() noexcept;

    // =========================================================================
    // 禁止拷贝 (RAII 资源管理原则)
    // =========================================================================
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;

    // =========================================================================
    // Getters (访问器)
    // =========================================================================

    /// @brief 获取逻辑设备句柄 (核心接口)
    [[nodiscard]]
    VkDevice getDevice() const { return logicalDevice; }

    /// @brief 获取物理设备句柄 (硬件信息)
    [[nodiscard]]
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }

    /// @brief 获取图形队列 (用于提交绘图指令)
    [[nodiscard]]
    VkQueue getGraphicsQueue() const { return graphicsQueue; }

    /// @brief 获取呈现队列 (用于交换链显示)
    [[nodiscard]]
    VkQueue getPresentQueue() const { return presentQueue; }

    /// @brief 获取主命令池 (用于分配 CommandBuffer)
    [[nodiscard]]
    VkCommandPool getCommandPool() const { return commandPool; }

    /// @brief 获取队列族索引信息
    [[nodiscard]]
    QueueFamilyIndices getQueueFamilies() const { return indices; }

    // =========================================================================
    // 核心工具函数
    // =========================================================================

    /**
     * @brief 查找适合特定用途的内存类型索引。
     * * 在创建 Buffer 或 Image 时，需要请求特定属性的内存（如 HOST_VISIBLE 用于 CPU 写入，DEVICE_LOCAL 用于 GPU 高效访问）。
     * 此函数会将需求映射到物理设备实际支持的内存堆索引。
     * * @param typeFilter 内存类型位掩码 (通常来自 VkMemoryRequirements.memoryTypeBits)
     * @param properties 所需的内存属性 (如 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT)
     * @return 符合条件的内存类型索引
     * @throw std::runtime_error 如果找不到满足条件的内存类型
     */
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    /// @brief 从系统中筛选出最适合的物理设备 (GPU)
    void pickPhysicalDevice(VkInstance instance);

    /// @brief 创建逻辑设备并获取队列句柄
    void createLogicalDevice();

    /// @brief 创建默认的命令池 (Command Pool)
    void createCommandPool();

    // =========================================================================
    // 内部检查函数
    // =========================================================================

    /// @brief 检查设备是否满足所有需求 (队列族、扩展、Swapchain支持、各向异性过滤等)
    bool isDeviceSuitable(VkPhysicalDevice device);

    /// @brief 查找设备支持的队列族索引 (Graphics & Present)
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

    /// @brief 检查设备是否支持所需的扩展 (如 VK_KHR_swapchain)
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    // =========================================================================
    // 成员变量
    // =========================================================================

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; /**< 物理设备句柄 (不需销毁) */
    VkDevice logicalDevice = VK_NULL_HANDLE;          /**< 逻辑设备句柄 (需销毁) */
    VkSurfaceKHR surface;                             /**< 窗口表面引用 (不拥有，由 Window 管理) */

    VkQueue graphicsQueue;                            /**< 图形队列句柄 */
    VkQueue presentQueue;                             /**< 呈现队列句柄 */
    VkCommandPool commandPool;                        /**< 主绘图命令池 */

    QueueFamilyIndices indices;                       /**< 缓存的队列族索引 */

    /// @brief 设备必须支持的扩展列表 (主要是 Swap chain)
    static constexpr std::array<const char*, 1> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
};

}