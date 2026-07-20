#pragma once

#include <cstdint>
#include <expected>
#include <string>
#include <vector>

namespace Tumbler {

struct Vertex {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};
static_assert(sizeof(Vertex) == 8 * sizeof(float));

enum class MeshLoadError {
    FileNotFound,
    ParseError,
    EmptyMesh,
};

struct MeshData {
    std::vector<Vertex>   vertices;
    std::vector<uint32_t> indices;
};

// Load .obj file via tinyobjloader
std::expected<MeshData, MeshLoadError> LoadObj(const std::string& path);

} // namespace Tumbler
