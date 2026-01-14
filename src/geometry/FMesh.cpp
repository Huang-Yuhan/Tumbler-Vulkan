#include "FMesh.h"
#include <glm/glm.hpp> // 在 cpp 里包含真正的 glm

namespace Tumbler
{
    // 这是一个内部辅助函数，不需要暴露给 .h
    // 放在这里，static 表示只在当前 cpp 文件可见
    static VkFormat GetVertexFormat(const EVertexAttribute attribute)
    {
        switch (attribute)
        {
        case EVertexAttribute::Position:  return VK_FORMAT_R32G32B32_SFLOAT;
        case EVertexAttribute::Normal:    return VK_FORMAT_R32G32B32_SFLOAT;
        case EVertexAttribute::Tangent:   return VK_FORMAT_R32G32B32A32_SFLOAT;
        case EVertexAttribute::Color:     return VK_FORMAT_R32G32B32A32_SFLOAT;
        case EVertexAttribute::UV0:       
        case EVertexAttribute::UV1:       return VK_FORMAT_R32G32_SFLOAT;
        default:                          return VK_FORMAT_UNDEFINED;
        }
    }

    // FVertexLayout 实现
    void FVertexLayout::AddElement(const EVertexAttribute attribute, const uint32_t formatSize)
    {
        FVertexElement element;
        element.Attribute = attribute;
        element.Offset = Stride;
        element.FormatSize = formatSize;
        Elements.push_back(element);
        Stride += formatSize;
    }

    std::vector<VkVertexInputAttributeDescription> FVertexLayout::GetAttributeDescriptions() const
    {
        std::vector<VkVertexInputAttributeDescription> descriptions;
        uint32_t location = 0;
        for (const auto& element : Elements)
        {
            VkVertexInputAttributeDescription desc{};
            desc.binding = 0;
            desc.location = location++;
            desc.format = GetVertexFormat(element.Attribute); // 调用内部辅助函数
            desc.offset = element.Offset;
            descriptions.push_back(desc);
        }
        return descriptions;
    }

    // FMesh 实现
    FMesh FMesh::CreatePlane(const float width, const float height, const uint32_t subdivisionsWidth, const uint32_t subdivisionsHeight)
    {
        FMesh plane;

        // 定义顶点布局
        plane.VertexLayout.AddElement(EVertexAttribute::Position, sizeof(float) * 3);
        plane.VertexLayout.AddElement(EVertexAttribute::Normal, sizeof(float) * 3);
        plane.VertexLayout.AddElement(EVertexAttribute::UV0, sizeof(float) * 2);

        const uint32_t vertexCountX = subdivisionsWidth + 1;
        const uint32_t vertexCountY = subdivisionsHeight + 1;
        const uint32_t totalVertices = vertexCountX * vertexCountY;

        plane.RawVertexData.resize(totalVertices * plane.VertexLayout.Stride);
        plane.Indices.reserve(subdivisionsWidth * subdivisionsHeight * 6);

        // 使用 #pragma pack 确保对齐安全 (防止编译器偷偷加 padding)
        #pragma pack(push, 1)
        struct FTempVertex
        {
            glm::vec3 Position;
            glm::vec3 Normal;
            glm::vec2 UV;
        };
        #pragma pack(pop)

        static_assert(sizeof(FTempVertex) == sizeof(glm::vec3) * 2 + sizeof(glm::vec2), "FTempVertex size mismatch");

        auto tempPtr = reinterpret_cast<FTempVertex*>(plane.RawVertexData.data());

        // 填充顶点
        for (uint32_t y = 0; y < vertexCountY; ++y)
        {
            for (uint32_t x = 0; x < vertexCountX; ++x)
            {
                const float posX = (float(x) / subdivisionsWidth - 0.5f) * width;
                const float posY = 0.0f;
                const float posZ = (float(y) / subdivisionsHeight - 0.5f) * height;
                const float u = float(x) / subdivisionsWidth;
                const float v = float(y) / subdivisionsHeight;

                auto& [Position, Normal, UV] = tempPtr[y * vertexCountX + x];
                Position = glm::vec3(posX, posY, posZ);
                Normal = glm::vec3(0.0f, 1.0f, 0.0f);
                UV = glm::vec2(u, v);
            }
        }

        // 填充索引
        for (uint32_t y = 0; y < subdivisionsHeight; ++y)
        {
            for (uint32_t x = 0; x < subdivisionsWidth; ++x)
            {
                uint32_t topLeft = y * vertexCountX + x;
                uint32_t topRight = topLeft + 1;
                uint32_t bottomLeft = (y + 1) * vertexCountX + x;
                uint32_t bottomRight = bottomLeft + 1;

                plane.Indices.push_back(topLeft);
                plane.Indices.push_back(bottomLeft);
                plane.Indices.push_back(topRight);

                plane.Indices.push_back(topRight);
                plane.Indices.push_back(bottomLeft);
                plane.Indices.push_back(bottomRight);
            }
        }

        return plane;
    }
}