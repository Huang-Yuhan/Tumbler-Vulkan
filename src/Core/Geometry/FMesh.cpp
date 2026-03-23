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

    // 定义顶点布局 (现在包含 Tangent)
    plane.VertexLayout.AddElement(EVertexAttribute::Position, sizeof(float) * 3);
    plane.VertexLayout.AddElement(EVertexAttribute::Normal, sizeof(float) * 3);
    plane.VertexLayout.AddElement(EVertexAttribute::Tangent, sizeof(float) * 3);
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
        glm::vec3 Tangent;
        glm::vec2 UV;
    };
    #pragma pack(pop)

    static_assert(sizeof(FTempVertex) == sizeof(glm::vec3) * 3 + sizeof(glm::vec2), "FTempVertex size mismatch");

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

            auto& vertex = tempPtr[y * vertexCountX + x];
            vertex.Position = glm::vec3(posX, posY, posZ);
            vertex.Normal = glm::vec3(0.0f, 1.0f, 0.0f);
            vertex.Tangent = glm::vec3(1.0f, 0.0f, 0.0f);
            vertex.UV = glm::vec2(u, v);
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
    // 2. 准备输出 Mesh，布局现在包含 Tangent
    // --------------------------------------------------------
    FMesh mesh;
    mesh.VertexLayout.AddElement(EVertexAttribute::Position, sizeof(float) * 3);
    mesh.VertexLayout.AddElement(EVertexAttribute::Normal,   sizeof(float) * 3);
    mesh.VertexLayout.AddElement(EVertexAttribute::Tangent,  sizeof(float) * 3);
    mesh.VertexLayout.AddElement(EVertexAttribute::UV0,      sizeof(float) * 2);

    #pragma pack(push, 1)
    struct FTempVertex
    {
        glm::vec3 Position;
        glm::vec3 Normal;
        glm::vec3 Tangent;
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
            size_t seed = 0;
            const float* data = &v.Position.x;
            for (int i = 0; i < 11; ++i)
            {
                uint32_t bits;
                memcpy(&bits, data + i, sizeof(uint32_t));
                seed ^= std::hash<uint32_t>{}(bits) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            }
            return seed;
        }
    };

    std::vector<FTempVertex> tempVertices;
    std::vector<glm::vec3> tempTangents;
    std::unordered_map<FTempVertex, uint32_t, FTempVertexHash> uniqueVertices;
    std::vector<uint32_t> indexMapping;

    // --------------------------------------------------------
    // 3. 遍历所有 Shape 的所有 Face，先展开顶点去重并收集索引
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
            
            vertex.Tangent = glm::vec3(0.0f);

            // UV（OBJ 可能没有 UV，此时 texcoord_index == -1）
            if (index.texcoord_index >= 0)
            {
                vertex.UV = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }
            else
            {
                vertex.UV = glm::vec2(0.0f, 0.0f);
            }

            // 去重：如果这个顶点出现过，直接复用其索引
            uint32_t finalIndex;
            if (uniqueVertices.find(vertex) == uniqueVertices.end())
            {
                finalIndex = static_cast<uint32_t>(tempVertices.size());
                uniqueVertices[vertex] = finalIndex;
                tempVertices.push_back(vertex);
                tempTangents.push_back(glm::vec3(0.0f));
            }
            else
            {
                finalIndex = uniqueVertices[vertex];
            }

            mesh.Indices.push_back(finalIndex);
            indexMapping.push_back(finalIndex);
        }
    }

    // --------------------------------------------------------
    // 4. 计算切线空间 (Tangent Space) - 按三角形计算
    // --------------------------------------------------------
    for (size_t i = 0; i < mesh.Indices.size(); i += 3)
    {
        uint32_t i0 = mesh.Indices[i + 0];
        uint32_t i1 = mesh.Indices[i + 1];
        uint32_t i2 = mesh.Indices[i + 2];

        const glm::vec3& v0 = tempVertices[i0].Position;
        const glm::vec3& v1 = tempVertices[i1].Position;
        const glm::vec3& v2 = tempVertices[i2].Position;

        const glm::vec2& uv0 = tempVertices[i0].UV;
        const glm::vec2& uv1 = tempVertices[i1].UV;
        const glm::vec2& uv2 = tempVertices[i2].UV;

        glm::vec3 deltaPos1 = v1 - v0;
        glm::vec3 deltaPos2 = v2 - v0;

        glm::vec2 deltaUV1 = uv1 - uv0;
        glm::vec2 deltaUV2 = uv2 - uv0;

        float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
        glm::vec3 tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y) * r;

        tempTangents[i0] += tangent;
        tempTangents[i1] += tangent;
        tempTangents[i2] += tangent;
    }

    // --------------------------------------------------------
    // 5. 正交化并归一化切线
    // --------------------------------------------------------
    for (size_t i = 0; i < tempVertices.size(); ++i)
    {
        glm::vec3& n = tempVertices[i].Normal;
        glm::vec3& t = tempTangents[i];

        t = glm::normalize(t - n * glm::dot(n, t));
        tempVertices[i].Tangent = t;
    }

    // --------------------------------------------------------
    // 6. 把去重后的顶点写入裸字节流
    // --------------------------------------------------------
    mesh.RawVertexData.resize(tempVertices.size() * mesh.VertexLayout.Stride);
    memcpy(mesh.RawVertexData.data(), tempVertices.data(), mesh.RawVertexData.size());

    return mesh;
}
