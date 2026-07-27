#include "MeshLoader.h"
#include "Core/Utils/Log.h"

#include <tiny_obj_loader.h>

#include <unordered_map>

namespace Tumbler {

// Hash combiner for vertex dedup
struct VertexKey {
    int vi, ni, ti;
    bool operator==(const VertexKey& o) const {
        return vi == o.vi && ni == o.ni && ti == o.ti;
    }
};

struct VertexKeyHash {
    size_t operator()(const VertexKey& k) const {
        return (static_cast<uint64_t>(k.vi) << 32) |
               (static_cast<uint64_t>(k.ni) << 16) |
               static_cast<uint64_t>(k.ti);
    }
};

std::expected<MeshData, MeshLoadError> LoadObj(const std::string& path) {
    tinyobj::ObjReaderConfig config;
    config.triangulate = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(path, config)) {
        if (!reader.Error().empty()) LOG_ERROR("tinyobj: {}", reader.Error());
        return std::unexpected(MeshLoadError::ParseError);
    }

    const auto& attrib = reader.GetAttrib();
    const auto& shapes = reader.GetShapes();

    if (shapes.empty() || attrib.vertices.empty()) {
        return std::unexpected(MeshLoadError::EmptyMesh);
    }

    MeshData mesh;
    std::unordered_map<VertexKey, uint32_t, VertexKeyHash> vertexMap;

    for (const auto& shape : shapes) {
        for (const auto& idx : shape.mesh.indices) {
            VertexKey key{
                idx.vertex_index,
                idx.normal_index,
                idx.texcoord_index,
            };

            auto it = vertexMap.find(key);
            if (it != vertexMap.end()) {
                mesh.indices.push_back(it->second);
                continue;
            }

            Vertex v{};
            v.pos.x = attrib.vertices[3 * key.vi + 0];
            v.pos.y = attrib.vertices[3 * key.vi + 1];
            v.pos.z = attrib.vertices[3 * key.vi + 2];

            if (key.ni >= 0) {
                v.normal.x = attrib.normals[3 * key.ni + 0];
                v.normal.y = attrib.normals[3 * key.ni + 1];
                v.normal.z = attrib.normals[3 * key.ni + 2];
            }

            if (key.ti >= 0) {
                v.uv.x = attrib.texcoords[2 * key.ti + 0];
                v.uv.y = 1.0f - attrib.texcoords[2 * key.ti + 1];  // flip V
            }

            uint32_t newIdx = static_cast<uint32_t>(mesh.vertices.size());
            mesh.vertices.push_back(v);
            mesh.indices.push_back(newIdx);
            vertexMap[key] = newIdx;
        }
    }

    LOG_INFO("Loaded '{}': {} vertices, {} triangles",
             path, mesh.vertices.size(), mesh.indices.size() / 3);
    return mesh;
}

} // namespace Tumbler
