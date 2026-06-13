// VulkanUtils.h — Vulkan 工具 (Header-only)
//
// 职责:
//   - VK_CHECK 宏 (Phase 2 新增): 统一 Vulkan 错误检查
//   - GetVulkanFormat / GenerateAttributeDescriptions (旧引擎): 顶点格式映射
//
// 依赖: Core/Utils/Log.h, Core/Geometry/FMesh.h
// 层级: 图形基础设施

#pragma once

#include "Core/Geometry/FMesh.h"
#include "Core/Utils/Log.h"
#include <cassert>
#include <vulkan/vulkan.h>
#include <vector>

// ==========================================
// VK_CHECK 宏 (gpu-driven-rewrite Phase 2 引入)
// ==========================================

#define VK_CHECK(expr)                                                                                                 \
    do {                                                                                                               \
        VkResult __res = (expr);                                                                                       \
        if (__res != VK_SUCCESS) {                                                                                     \
            LOG_ERROR("Vulkan error: {} returned {}", #expr, static_cast<int>(__res));                                 \
            assert(false);                                                                                             \
        }                                                                                                              \
    } while (0)

// ==========================================
// 顶点格式映射 (旧引擎保留)
// ==========================================

inline VkFormat GetVulkanFormat(EVertexAttribute attribute)
{
    switch (attribute)
    {
        case EVertexAttribute::Position: return VK_FORMAT_R32G32B32_SFLOAT;
        case EVertexAttribute::Normal:   return VK_FORMAT_R32G32B32_SFLOAT;
        case EVertexAttribute::Tangent:  return VK_FORMAT_R32G32B32A32_SFLOAT;
        case EVertexAttribute::Color:    return VK_FORMAT_R8G8B8A8_UNORM;
        case EVertexAttribute::UV0:      return VK_FORMAT_R32G32_SFLOAT;
        case EVertexAttribute::UV1:      return VK_FORMAT_R32G32_SFLOAT;
        default:                         return VK_FORMAT_UNDEFINED;
    }
}

inline std::vector<VkVertexInputAttributeDescription> GenerateAttributeDescriptions(const FVertexLayout& layout) {
    std::vector<VkVertexInputAttributeDescription> descriptions;
    uint32_t location = 0;

    for (const auto& element : layout.Elements) {
        VkVertexInputAttributeDescription desc{};
        desc.binding = 0;
        desc.location = location++;
        desc.format = GetVulkanFormat(element.Attribute);
        desc.offset = element.Offset;
        descriptions.push_back(desc);
    }
    return descriptions;
}
