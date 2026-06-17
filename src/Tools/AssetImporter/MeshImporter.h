#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Tumbler {

// ============================================================================
// MeshImporter — OBJ → .tmesh
// ============================================================================
class MeshImporter {
public:
    struct Vertex {
        float PositionX, PositionY, PositionZ;
        float NormalX, NormalY, NormalZ;
        float UVU, UVV;
    };

    struct SubMesh {
        uint32_t FirstIndex = 0;
        uint32_t IndexCount = 0;
        int32_t MaterialIndex = -1;
        uint32_t FirstVertex = 0;
        uint32_t VertexCount = 0;
        float AABBMin[3] = {0, 0, 0};
        float AABBMax[3] = {0, 0, 0};
    };

    struct ImportResult {
        std::vector<Vertex> Vertices;
        std::vector<uint32_t> Indices;
        std::vector<SubMesh> SubMeshes;
        float OverallAABBMin[3] = {0, 0, 0};
        float OverallAABBMax[3] = {0, 0, 0};
    };

    MeshImporter() = default;
    ~MeshImporter() = default;

    // 从 OBJ 文件加载网格数据
    bool Load(const std::string& objPath, ImportResult& outResult);

    // 将已加载的数据写入 .tmesh 二进制文件
    static bool WriteTMesh(const std::string& tmeshPath, const ImportResult& result);
};

} // namespace Tumbler
