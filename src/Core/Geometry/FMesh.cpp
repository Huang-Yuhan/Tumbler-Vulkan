#include "FMesh.h"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <stdexcept>

// tinyobjloader：只在一个 .cpp 里定义实现
#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

// FVertexLayout 实现
void FVertexLayout::AddElement(const EVertexAttribute attribute, const uint32_t formatSize)
{
    FVertexElement element{};
    element.Attribute = attribute;
    element.Offset = Stride;
    element.FormatSize = formatSize;
    Elements.push_back(element);
    Stride += formatSize;
}

// =====================================================================
// FMesh 实现
// =====================================================================
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

FMesh FMesh::LoadFromOBJ(const std::string& filePath)
{
    // --------------------------------------------------------
    // 1. 用 tinyobjloader 解析文件
    // --------------------------------------------------------
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool success = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filePath.c_str());

    if (!success)
    {
        throw std::runtime_error("Failed to load OBJ: " + filePath + "\n  Error: " + err);
    }

    // --------------------------------------------------------
    // 2. 准备输出 Mesh，布局与 CreatePlane 完全一致
    // --------------------------------------------------------
    FMesh mesh;
    mesh.VertexLayout.AddElement(EVertexAttribute::Position, sizeof(float) * 3);
    mesh.VertexLayout.AddElement(EVertexAttribute::Normal,   sizeof(float) * 3);
    mesh.VertexLayout.AddElement(EVertexAttribute::UV0,      sizeof(float) * 2);

    #pragma pack(push, 1)
    struct FTempVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec2 UV;

        bool operator==(const FTempVertex& o) const
        {
            return Position == o.Position && Normal == o.Normal && UV == o.UV;
        }
    };
    #pragma pack(pop)

    // 用于去重顶点：key=顶点数据哈希, value=在顶点数组中的索引
    struct FTempVertexHash
    {
        size_t operator()(const FTempVertex& v) const
        {
            // 把 8 个 float 拼成哈希（简单但够用）
            size_t seed = 0;
            const float* data = &v.Position.x;
            for (int i = 0; i < 8; ++i)
            {
                uint32_t bits;
                memcpy(&bits, data + i, sizeof(uint32_t));
                seed ^= std::hash<uint32_t>{}(bits) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };

    std::vector<FTempVertex> tempVertices;
    std::unordered_map<FTempVertex, uint32_t, FTempVertexHash> uniqueVertices;

    // --------------------------------------------------------
    // 3. 遍历所有 Shape 的所有 Face，展开顶点并去重
    // --------------------------------------------------------
    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            FTempVertex vertex{};

            // Position
            vertex.Position = {
                attrib.vertices[3 * index.vertex_index + 0],
                attrib.vertices[3 * index.vertex_index + 1],
                attrib.vertices[3 * index.vertex_index + 2]
            };

            // Normal（OBJ 可能没有法线，此时 normal_index == -1）
            if (index.normal_index >= 0)
            {
                vertex.Normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2]
                };
            }
            else
            {
                vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f); // 备用向上法线
            }

            // UV（OBJ 可能没有 UV，此时 texcoord_index == -1）
            if (index.texcoord_index >= 0)
            {
                vertex.UV = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    // OBJ 的 V 轴与 Vulkan/DirectX 相反，翻转修正
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            else
            {
                vertex.UV = glm::vec2(0.0f, 0.0f);
            }

            // 去重：如果这个顶点出现过，直接复用其索引
            if (uniqueVertices.find(vertex) == uniqueVertices.end())
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(tempVertices.size());
                tempVertices.push_back(vertex);
            }

            mesh.Indices.push_back(uniqueVertices[vertex]);
        }
    }

    // --------------------------------------------------------
    // 4. 把去重后的顶点写入裸字节流
    // --------------------------------------------------------
    mesh.RawVertexData.resize(tempVertices.size() * mesh.VertexLayout.Stride);
    memcpy(mesh.RawVertexData.data(), tempVertices.data(), mesh.RawVertexData.size());

    return mesh;
}
