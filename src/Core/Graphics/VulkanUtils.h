#pragma once
#include <Core/Geometry/FMesh.h>
#include <vulkan/vulkan.h>

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
        desc.binding = 0; // 通常只有一个 buffer binding
        desc.location = location++;
        desc.format = GetVulkanFormat(element.Attribute);
        desc.offset = element.Offset;
        descriptions.push_back(desc);
    }
    return descriptions;
}