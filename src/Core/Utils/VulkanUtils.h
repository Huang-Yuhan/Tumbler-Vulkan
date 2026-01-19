#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <type_traits> // 需要这个头文件

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
            VkResult res = func(args..., &count, nullptr);
            if (res != VK_SUCCESS && res != VK_INCOMPLETE) return {};

            if (count == 0) return {};

            result.resize(count);

            // 第二次调用
            res = func(args..., &count, result.data());
            if (res != VK_SUCCESS && res != VK_INCOMPLETE) return {};
        }
        else
        {
            throw std::runtime_error("Unsupported return type");
        }

        return result;
    }
}