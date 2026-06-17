#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace Tumbler {

// ============================================================================
// TextureImporter — PNG/JPG → .ttex (CPU mipmap)
// ============================================================================
class TextureImporter {
public:
    struct ImportResult {
        uint32_t Width = 0;
        uint32_t Height = 0;
        uint32_t MipLevels = 0;
        uint32_t Format = 0;          // ETextureFormat 值
        std::vector<uint8_t> MipData; // 所有 mip 数据连续存储
    };

    TextureImporter() = default;
    ~TextureImporter() = default;

    // 从 PNG/JPG 加载纹理，生成 mipmap
    bool Load(const std::string& imagePath, ImportResult& outResult);

    // 将已加载的数据写入 .ttex 二进制文件
    static bool WriteTTex(const std::string& ttexPath, const ImportResult& result);
};

} // namespace Tumbler
