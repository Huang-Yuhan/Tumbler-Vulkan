#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <type_traits> // 需要这个头文件
#include "Core/Utils/Log.h"
#include <stdexcept>

namespace VkUtils {

    inline const char* VkResultToString(const VkResult result) {
        switch (result) {
            // 成功状态
            case VK_SUCCESS: return "VK_SUCCESS";
            case VK_NOT_READY: return "VK_NOT_READY";
            case VK_TIMEOUT: return "VK_TIMEOUT";

                // 常见错误
            case VK_ERROR_OUT_OF_HOST_MEMORY: return "VK_ERROR_OUT_OF_HOST_MEMORY";
            case VK_ERROR_OUT_OF_DEVICE_MEMORY: return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
            case VK_ERROR_INITIALIZATION_FAILED: return "VK_ERROR_INITIALIZATION_FAILED";
            case VK_ERROR_DEVICE_LOST: return "VK_ERROR_DEVICE_LOST";
            case VK_ERROR_MEMORY_MAP_FAILED: return "VK_ERROR_MEMORY_MAP_FAILED";
            case VK_ERROR_LAYER_NOT_PRESENT: return "VK_ERROR_LAYER_NOT_PRESENT";
            case VK_ERROR_EXTENSION_NOT_PRESENT: return "VK_ERROR_EXTENSION_NOT_PRESENT";
            case VK_ERROR_FEATURE_NOT_PRESENT: return "VK_ERROR_FEATURE_NOT_PRESENT";
            case VK_ERROR_INCOMPATIBLE_DRIVER: return "VK_ERROR_INCOMPATIBLE_DRIVER";
            case VK_ERROR_TOO_MANY_OBJECTS: return "VK_ERROR_TOO_MANY_OBJECTS";
            case VK_ERROR_FORMAT_NOT_SUPPORTED: return "VK_ERROR_FORMAT_NOT_SUPPORTED";
            case VK_ERROR_SURFACE_LOST_KHR: return "VK_ERROR_SURFACE_LOST_KHR";
            case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR: return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
            case VK_SUBOPTIMAL_KHR: return "VK_SUBOPTIMAL_KHR";
            case VK_ERROR_OUT_OF_DATE_KHR: return "VK_ERROR_OUT_OF_DATE_KHR";
            case VK_ERROR_INCOMPATIBLE_DISPLAY_KHR: return "VK_ERROR_INCOMPATIBLE_DISPLAY_KHR";
            case VK_ERROR_VALIDATION_FAILED_EXT: return "VK_ERROR_VALIDATION_FAILED_EXT";

            default: return "UNKNOWN_ERROR";
        }
    }


#define VK_CHECK(x)                                                                 \
    do {                                                                            \
        VkResult err = x;                                                           \
        if (err) {                                                                  \
            /* 如果是未知的错误码，spdlog 会直接打印 "UNKNOWN_ERROR" */                 \
            LOG_CRITICAL("[Vulkan Error] Detected: {} ({})\n \tFile: {}\n \tLine: {}", \
                         VkUtils::VkResultToString(err), (int)err, __FILE__, __LINE__);      \
            abort();                                                                \
        }                                                                           \
    } while (0)

    /**
     * @brief 智能封装 Vulkan 的 "count then data" 模式
     * 自动处理返回 void 的函数 (如 vkGetPhysicalDeviceQueueFamilyProperties)
     * 和返回 VkResult 的函数 (如 vkEnumeratePhysicalDevices)
     */
    template <typename T, typename Func, typename... Args>
    std::vector<T> GetVector(Func func, Args... args) {
        // 1. 获取函数的返回类型
        // 我们模拟调用 func(args..., uint32_t*, T*) 来看看它返回什么
        using ReturnType = std::invoke_result_t<Func, Args..., uint32_t*, T*>;

        uint32_t count = 0;
        std::vector<T> result;

        // ============================================
        // 分支 A: 函数返回 void (例如 QueueFamilyProperties)
        // ============================================
        if constexpr (std::is_void_v<ReturnType>) {
            // 第一次调用：获取数量
            func(args..., &count, nullptr);

            if (count == 0) return {};

            result.resize(count);

            // 第二次调用：获取数据
            func(args..., &count, result.data());
        }
        // ============================================
        // 分支 B: 函数返回 VkResult (绝大多数情况)
        // ============================================
        else if constexpr (std::is_same_v<ReturnType,VkResult>) {
            // 第一次调用

            VK_CHECK(func(args..., &count, nullptr));
            //VkResult res = func(args..., &count, nullptr);
            //if (res != VK_SUCCESS && res != VK_INCOMPLETE) return {};

            if (count == 0) return {};

            result.resize(count);

            // 第二次调用
            //res = func(args..., &count, result.data());
            //if (res != VK_SUCCESS && res != VK_INCOMPLETE) return {};
            VK_CHECK(func(args..., &count, result.data()));
        }
        else
        {
            throw std::runtime_error("Unsupported return type");
        }

        return result;
    }

// ============================================================================
// 1. VkFormat 转字符串 (只列出常用的，避免几千行 switch)
// ============================================================================
inline const char* VkFormatToString(VkFormat format)
{
    switch (format) {
        case VK_FORMAT_UNDEFINED: return "VK_FORMAT_UNDEFINED";

        // --- 常见的 Swapchain / 颜色附件格式 ---
        case VK_FORMAT_B8G8R8A8_UNORM: return "VK_FORMAT_B8G8R8A8_UNORM";
        case VK_FORMAT_B8G8R8A8_SRGB:  return "VK_FORMAT_B8G8R8A8_SRGB (Preferred)";
        case VK_FORMAT_R8G8B8A8_UNORM: return "VK_FORMAT_R8G8B8A8_UNORM";
        case VK_FORMAT_R8G8B8A8_SRGB:  return "VK_FORMAT_R8G8B8A8_SRGB";

        // --- 深度 / 模板格式 ---
        case VK_FORMAT_D32_SFLOAT:          return "VK_FORMAT_D32_SFLOAT";
        case VK_FORMAT_D32_SFLOAT_S8_UINT:  return "VK_FORMAT_D32_SFLOAT_S8_UINT";
        case VK_FORMAT_D24_UNORM_S8_UINT:   return "VK_FORMAT_D24_UNORM_S8_UINT";
        case VK_FORMAT_D16_UNORM:           return "VK_FORMAT_D16_UNORM";

        // --- 高动态范围 (HDR) ---
        case VK_FORMAT_R16G16B16A16_SFLOAT: return "VK_FORMAT_R16G16B16A16_SFLOAT";
        case VK_FORMAT_R32G32B32A32_SFLOAT: return "VK_FORMAT_R32G32B32A32_SFLOAT";

        default: return "VK_FORMAT_OTHER (Unknown/Uncommon)";
    }
}

// ============================================================================
// 2. VkColorSpaceKHR 转字符串
// ============================================================================
inline const char* VkColorSpaceToString(VkColorSpaceKHR color_space)
{
    switch (color_space) {
        // 最标准，覆盖 99% 的情况
        case VK_COLOR_SPACE_SRGB_NONLINEAR_KHR:       return "VK_COLOR_SPACE_SRGB_NONLINEAR_KHR";

        // 广色域 (Display P3)
        case VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT: return "VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT";

        // HDR 相关
        case VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT: return "VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT";
        case VK_COLOR_SPACE_HDR10_ST2084_EXT:         return "VK_COLOR_SPACE_HDR10_ST2084_EXT";
        case VK_COLOR_SPACE_DOLBYVISION_EXT:          return "VK_COLOR_SPACE_DOLBYVISION_EXT";
        case VK_COLOR_SPACE_HDR10_HLG_EXT:            return "VK_COLOR_SPACE_HDR10_HLG_EXT";

        // Adobe RGB
        case VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT:      return "VK_COLOR_SPACE_ADOBERGB_LINEAR_EXT";
        case VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT:   return "VK_COLOR_SPACE_ADOBERGB_NONLINEAR_EXT";

        default: return "VK_COLOR_SPACE_OTHER";
    }
}

// ============================================================================
// 3. VkPresentModeKHR 转字符串
// ============================================================================
inline const char* VkPresentModeToString(VkPresentModeKHR present_mode)
{
    switch (present_mode) {
        // 立即呈现 (可能撕裂)
        case VK_PRESENT_MODE_IMMEDIATE_KHR:    return "VK_PRESENT_MODE_IMMEDIATE_KHR (No VSync)";

        // 三重缓冲 (低延迟，无撕裂) - 首选
        case VK_PRESENT_MODE_MAILBOX_KHR:      return "VK_PRESENT_MODE_MAILBOX_KHR (Triple Buffering)";

        // 垂直同步 (标准，有延迟) - 保底
        case VK_PRESENT_MODE_FIFO_KHR:         return "VK_PRESENT_MODE_FIFO_KHR (VSync)";

        // 垂直同步松弛版 (如果卡顿了，下一帧会立刻撕裂显示以追赶进度)
        case VK_PRESENT_MODE_FIFO_RELAXED_KHR: return "VK_PRESENT_MODE_FIFO_RELAXED_KHR";

        // 移动端常用的模式
        case VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR: return "VK_PRESENT_MODE_SHARED_DEMAND_REFRESH_KHR";
        case VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR: return "VK_PRESENT_MODE_SHARED_CONTINUOUS_REFRESH_KHR";

        default: return "VK_PRESENT_MODE_OTHER";
    }
}
}
