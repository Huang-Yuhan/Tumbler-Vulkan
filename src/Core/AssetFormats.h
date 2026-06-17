#pragma once

#include <cstdint>

namespace Tumbler {

// ============================================================================
// ETextureFormat — 纹理格式枚举（独立于 Vulkan VkFormat）
// ============================================================================
enum class ETextureFormat : uint8_t {
    R8_UNORM = 0,
    R8G8_UNORM = 1,
    R8G8B8A8_UNORM = 2,
    R8G8B8A8_SRGB = 3,
    R16G16_SFLOAT = 4,
    R16G16B16A16_SFLOAT = 5,
    R32_SFLOAT = 6,
    R32G32_SFLOAT = 7,
    R32G32B32A32_SFLOAT = 8,
    // 后续可加 BC/ETC2 压缩格式
    Count
};

inline constexpr uint32_t BytesPerPixel(ETextureFormat format) {
    switch (format) {
        case ETextureFormat::R8_UNORM:
            return 1;
        case ETextureFormat::R8G8_UNORM:
            return 2;
        case ETextureFormat::R8G8B8A8_UNORM: // fallthrough
        case ETextureFormat::R8G8B8A8_SRGB:
            return 4;
        case ETextureFormat::R16G16_SFLOAT:
            return 4;
        case ETextureFormat::R16G16B16A16_SFLOAT:
            return 8;
        case ETextureFormat::R32_SFLOAT:
            return 4;
        case ETextureFormat::R32G32_SFLOAT:
            return 8;
        case ETextureFormat::R32G32B32A32_SFLOAT:
            return 16;
        default:
            return 0;
    }
}

// ============================================================================
// .tmesh 二进制格式
// ============================================================================
inline constexpr uint32_t TMESH_MAGIC = 0x48534D54; // "TMSH" little-endian
inline constexpr uint32_t TMESH_VERSION = 1;
inline constexpr uint32_t TMESH_HEADER_SIZE = 64;
inline constexpr uint32_t TMESH_VERTEX_STRIDE = 32; // 8 × float32

inline constexpr uint32_t TMESH_SUBMESH_ENTRY_SIZE = 44;

// SubMesh 数据（对应 .tmesh 中每个 SubMesh 的磁盘布局）
struct TMeshSubMeshEntry {
    uint32_t FirstIndex = 0;      // 索引起始偏移（元素个数，非字节）
    uint32_t IndexCount = 0;      // 索引数量
    int32_t MaterialIndex = -1;   // 材质列表索引，-1 表示无材质
    uint32_t FirstVertex = 0;     // 顶点起始偏移（元素个数）
    uint32_t VertexCount = 0;     // 顶点数量
    float AABBMin[3] = {0, 0, 0}; // AABB 最小点
    float AABBMax[3] = {0, 0, 0}; // AABB 最大点
};

static_assert(sizeof(TMeshSubMeshEntry) == TMESH_SUBMESH_ENTRY_SIZE, "TMeshSubMeshEntry must be 44 bytes");

// .tmesh 文件头（64 字节）
struct TMeshHeader {
    uint32_t Magic = TMESH_MAGIC;
    uint32_t Version = TMESH_VERSION;
    uint32_t VertexCount = 0;
    uint32_t IndexCount = 0;
    uint32_t VertexStride = TMESH_VERTEX_STRIDE;
    uint32_t SubMeshCount = 0;
    float AABBMin[3] = {0, 0, 0};
    float AABBMax[3] = {0, 0, 0};
    uint8_t Reserved[16] = {};
};

static_assert(sizeof(TMeshHeader) == TMESH_HEADER_SIZE, "TMeshHeader must be 64 bytes");

// ============================================================================
// .ttex 二进制格式
// ============================================================================
inline constexpr uint32_t TTEX_MAGIC = 0x58455454; // "TTEX" little-endian
inline constexpr uint32_t TTEX_VERSION = 1;
inline constexpr uint32_t TTEX_HEADER_SIZE = 32;

// .ttex 文件头（32 字节）
struct TTexHeader {
    uint32_t Magic = TTEX_MAGIC;
    uint32_t Version = TTEX_VERSION;
    uint32_t Format = 0; // ETextureFormat 值
    uint32_t Width = 0;
    uint32_t Height = 0;
    uint32_t MipLevels = 0;
    uint32_t MipDataSize = 0; // 所有 mip 数据总字节数
    uint32_t Reserved = 0;
};

static_assert(sizeof(TTexHeader) == TTEX_HEADER_SIZE, "TTexHeader must be 32 bytes");

} // namespace Tumbler
