// VulkanUtils.h — Vulkan 工具宏 (Header-only)
//
// 职责: VK_CHECK 宏，统一 Vulkan 错误检查。
//       纯宏，无状态。
//
// 依赖: Core/Utils/Log.h
// 层级: 图形基础设施 (Phase 2)

#pragma once

#include "Core/Utils/Log.h"
#include <cassert>
#include <vulkan/vulkan.h>

namespace Tumbler {

#define VK_CHECK(expr)                                                                                                 \
    do {                                                                                                               \
        VkResult __res = (expr);                                                                                       \
        if (__res != VK_SUCCESS) {                                                                                     \
            LOG_ERROR("Vulkan error: {} returned {}", #expr, static_cast<int>(__res));                                 \
            assert(false);                                                                                             \
        }                                                                                                              \
    } while (0)

} // namespace Tumbler
