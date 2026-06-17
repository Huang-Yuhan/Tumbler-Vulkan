#include "MeshImporter.h"
#include "Core/AssetFormats.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <iostream>
#include <tiny_obj_loader.h>

namespace Tumbler {

bool MeshImporter::Load(const std::string& objPath, ImportResult& outResult) {
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    bool ok = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, objPath.c_str());
    if (!warn.empty()) {
        std::cerr << "[MeshImporter] Warning: " << warn << std::endl;
    }
    if (!err.empty()) {
        std::cerr << "[MeshImporter] Error: " << err << std::endl;
    }
    if (!ok) {
        return false;
    }

    outResult.Vertices.clear();
    outResult.Indices.clear();
    outResult.SubMeshes.clear();

    // 整体 AABB 初始化为极端值
    float overallMin[3] = {1e30f, 1e30f, 1e30f};
    float overallMax[3] = {-1e30f, -1e30f, -1e30f};

    // 每个 shape = 一个 SubMesh
    for (const auto& shape : shapes) {
        SubMesh subMesh;
        subMesh.FirstIndex = static_cast<uint32_t>(outResult.Indices.size());
        subMesh.FirstVertex = static_cast<uint32_t>(outResult.Vertices.size());
        subMesh.MaterialIndex = static_cast<int32_t>(shape.mesh.material_ids.empty() ? -1 : shape.mesh.material_ids[0]);

        float subMin[3] = {1e30f, 1e30f, 1e30f};
        float subMax[3] = {-1e30f, -1e30f, -1e30f};

        // 遍历 shape 的所有面
        // tinyobjloader: 每个 face 有 3 个 index (三角面)
        // 我们需要创建交织的顶点
        uint32_t indexOffset = static_cast<uint32_t>(outResult.Indices.size());
        std::map<std::tuple<int, int, int>, uint32_t> vertexCache; // (pos, norm, uv) → new vertex index
        uint32_t localVertexCount = 0;

        for (size_t faceIdx = 0; faceIdx < shape.mesh.num_face_vertices.size(); faceIdx++) {
            int fv = shape.mesh.num_face_vertices[faceIdx];
            if (fv != 3) {
                std::cerr << "[MeshImporter] Warning: non-triangle face (" << fv << " vertices) in shape '"
                          << shape.name << "', skipping" << std::endl;
                continue;
            }

            // 每个三角面收集 3 个顶点的 (positionIdx, normalIdx, texcoordIdx)
            for (int v = 0; v < 3; v++) {
                tinyobj::index_t idx = shape.mesh.indices[faceIdx * 3 + v];

                auto cacheKey = std::make_tuple(idx.vertex_index, idx.normal_index, idx.texcoord_index);
                auto cacheIt = vertexCache.find(cacheKey);

                if (cacheIt != vertexCache.end()) {
                    // 顶点已存在，复用
                    outResult.Indices.push_back(indexOffset + cacheIt->second);
                } else {
                    // 创建新顶点
                    Vertex vertex{};

                    // Position
                    if (idx.vertex_index >= 0 && idx.vertex_index * 3 + 2 < (int)attrib.vertices.size()) {
                        vertex.PositionX = attrib.vertices[idx.vertex_index * 3 + 0];
                        vertex.PositionY = attrib.vertices[idx.vertex_index * 3 + 1];
                        vertex.PositionZ = attrib.vertices[idx.vertex_index * 3 + 2];
                    }

                    // Normal
                    if (idx.normal_index >= 0 && idx.normal_index * 3 + 2 < (int)attrib.normals.size()) {
                        vertex.NormalX = attrib.normals[idx.normal_index * 3 + 0];
                        vertex.NormalY = attrib.normals[idx.normal_index * 3 + 1];
                        vertex.NormalZ = attrib.normals[idx.normal_index * 3 + 2];
                    }

                    // UV
                    if (idx.texcoord_index >= 0 && idx.texcoord_index * 2 + 1 < (int)attrib.texcoords.size()) {
                        vertex.UVU = attrib.texcoords[idx.texcoord_index * 2 + 0];
                        vertex.UVV = attrib.texcoords[idx.texcoord_index * 2 + 1];
                    }

                    uint32_t newIndex = localVertexCount++;
                    vertexCache[cacheKey] = newIndex;
                    outResult.Vertices.push_back(vertex);
                    outResult.Indices.push_back(indexOffset + newIndex);

                    // 更新 SubMesh AABB
                    subMin[0] = std::min(subMin[0], vertex.PositionX);
                    subMin[1] = std::min(subMin[1], vertex.PositionY);
                    subMin[2] = std::min(subMin[2], vertex.PositionZ);
                    subMax[0] = std::max(subMax[0], vertex.PositionX);
                    subMax[1] = std::max(subMax[1], vertex.PositionY);
                    subMax[2] = std::max(subMax[2], vertex.PositionZ);
                }
            }
        }

        subMesh.IndexCount = static_cast<uint32_t>(outResult.Indices.size()) - subMesh.FirstIndex;
        subMesh.VertexCount = localVertexCount;
        subMesh.AABBMin[0] = subMin[0];
        subMesh.AABBMin[1] = subMin[1];
        subMesh.AABBMin[2] = subMin[2];
        subMesh.AABBMax[0] = subMax[0];
        subMesh.AABBMax[1] = subMax[1];
        subMesh.AABBMax[2] = subMax[2];

        outResult.SubMeshes.push_back(subMesh);

        // 更新整体 AABB
        overallMin[0] = std::min(overallMin[0], subMin[0]);
        overallMin[1] = std::min(overallMin[1], subMin[1]);
        overallMin[2] = std::min(overallMin[2], subMin[2]);
        overallMax[0] = std::max(overallMax[0], subMax[0]);
        overallMax[1] = std::max(overallMax[1], subMax[1]);
        overallMax[2] = std::max(overallMax[2], subMax[2]);
    }

    outResult.OverallAABBMin[0] = overallMin[0];
    outResult.OverallAABBMin[1] = overallMin[1];
    outResult.OverallAABBMin[2] = overallMin[2];
    outResult.OverallAABBMax[0] = overallMax[0];
    outResult.OverallAABBMax[1] = overallMax[1];
    outResult.OverallAABBMax[2] = overallMax[2];

    std::cout << "[MeshImporter] Loaded '" << objPath << "': " << outResult.Vertices.size() << " vertices, "
              << outResult.Indices.size() << " indices, " << outResult.SubMeshes.size() << " sub-meshes" << std::endl;

    return true;
}

bool MeshImporter::WriteTMesh(const std::string& tmeshPath, const ImportResult& result) {
    std::ofstream file(tmeshPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[MeshImporter] Failed to open output file: " << tmeshPath << std::endl;
        return false;
    }

    // 写入 Header
    TMeshHeader header{};
    header.VertexCount = static_cast<uint32_t>(result.Vertices.size());
    header.IndexCount = static_cast<uint32_t>(result.Indices.size());
    header.SubMeshCount = static_cast<uint32_t>(result.SubMeshes.size());
    header.AABBMin[0] = result.OverallAABBMin[0];
    header.AABBMin[1] = result.OverallAABBMin[1];
    header.AABBMin[2] = result.OverallAABBMin[2];
    header.AABBMax[0] = result.OverallAABBMax[0];
    header.AABBMax[1] = result.OverallAABBMax[1];
    header.AABBMax[2] = result.OverallAABBMax[2];

    file.write(reinterpret_cast<const char*>(&header), sizeof(TMeshHeader));

    // 写入 SubMesh 数组
    for (const auto& sub : result.SubMeshes) {
        TMeshSubMeshEntry entry{};
        entry.FirstIndex = sub.FirstIndex;
        entry.IndexCount = sub.IndexCount;
        entry.MaterialIndex = sub.MaterialIndex;
        entry.FirstVertex = sub.FirstVertex;
        entry.VertexCount = sub.VertexCount;
        entry.AABBMin[0] = sub.AABBMin[0];
        entry.AABBMin[1] = sub.AABBMin[1];
        entry.AABBMin[2] = sub.AABBMin[2];
        entry.AABBMax[0] = sub.AABBMax[0];
        entry.AABBMax[1] = sub.AABBMax[1];
        entry.AABBMax[2] = sub.AABBMax[2];
        file.write(reinterpret_cast<const char*>(&entry), sizeof(TMeshSubMeshEntry));
    }

    // 写入顶点数据
    uint32_t vertexDataSize = static_cast<uint32_t>(result.Vertices.size()) * sizeof(Vertex);
    file.write(reinterpret_cast<const char*>(result.Vertices.data()), vertexDataSize);

    // 写入索引数据
    uint32_t indexDataSize = static_cast<uint32_t>(result.Indices.size()) * sizeof(uint32_t);
    file.write(reinterpret_cast<const char*>(result.Indices.data()), indexDataSize);

    file.close();

    size_t fileSize =
        sizeof(TMeshHeader) + result.SubMeshes.size() * sizeof(TMeshSubMeshEntry) + vertexDataSize + indexDataSize;

    std::cout << "[MeshImporter] Written '" << tmeshPath << "' (" << fileSize << " bytes)" << std::endl;

    return true;
}

} // namespace Tumbler
