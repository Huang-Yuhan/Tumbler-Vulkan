#include "TextureImporter.h"
#include "Core/AssetFormats.h"

#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stb_image.h>
#include <stb_image_resize2.h>

namespace Tumbler {

bool TextureImporter::Load(const std::string& imagePath, ImportResult& outResult) {
    int width, height, channels;
    unsigned char* data = stbi_load(imagePath.c_str(), &width, &height, &channels, 0);
    if (!data) {
        std::cerr << "[TextureImporter] Failed to load image: " << imagePath << " ("
                  << (stbi_failure_reason() ? stbi_failure_reason() : "unknown") << ")" << std::endl;
        return false;
    }

    outResult.Width = static_cast<uint32_t>(width);
    outResult.Height = static_cast<uint32_t>(height);

    // 根据通道数决定格式
    // 4 通道 → sRGB (颜色贴图), 1-2 通道 → UNORM (法线/金属度/粗糙度)
    if (channels == 4) {
        outResult.Format = static_cast<uint32_t>(ETextureFormat::R8G8B8A8_SRGB);
    } else if (channels == 3) {
        // stb 不支持 3 通道 mipmap resize，转换为 4 通道
        outResult.Format = static_cast<uint32_t>(ETextureFormat::R8G8B8A8_SRGB);
        std::vector<uint8_t> rgbaData(width * height * 4);
        for (int i = 0; i < width * height; i++) {
            rgbaData[i * 4 + 0] = data[i * 3 + 0];
            rgbaData[i * 4 + 1] = data[i * 3 + 1];
            rgbaData[i * 4 + 2] = data[i * 3 + 2];
            rgbaData[i * 4 + 3] = 255;
        }
        stbi_image_free(data);
        data = new uint8_t[width * height * 4];
        std::memcpy(data, rgbaData.data(), rgbaData.size());
        channels = 4;
    } else if (channels == 2) {
        outResult.Format = static_cast<uint32_t>(ETextureFormat::R8G8_UNORM);
    } else {
        outResult.Format = static_cast<uint32_t>(ETextureFormat::R8_UNORM);
    }

    uint32_t bpp = BytesPerPixel(static_cast<ETextureFormat>(outResult.Format));

    // 计算 mip 级别数
    uint32_t maxDim = std::max(outResult.Width, outResult.Height);
    outResult.MipLevels = 1 + static_cast<uint32_t>(std::floor(std::log2(static_cast<float>(maxDim))));

    outResult.MipData.clear();

    // 逐级生成 mipmap
    uint32_t curWidth = outResult.Width;
    uint32_t curHeight = outResult.Height;
    unsigned char* curData = data;

    for (uint32_t mip = 0; mip < outResult.MipLevels; mip++) {
        uint32_t mipSize = curWidth * curHeight * bpp;

        // 将当前 mip 数据追加到输出 buffer
        size_t offset = outResult.MipData.size();
        outResult.MipData.resize(offset + mipSize);
        std::memcpy(outResult.MipData.data() + offset, curData, mipSize);

        // 生成下一级 mip（如果还有）
        if (mip + 1 < outResult.MipLevels) {
            uint32_t nextWidth = std::max(1u, curWidth / 2);
            uint32_t nextHeight = std::max(1u, curHeight / 2);
            uint32_t nextSize = nextWidth * nextHeight * bpp;
            unsigned char* nextData = new unsigned char[nextSize];

            // 使用 stbir_resize_uint8_linear 做降采样
            stbir_resize_uint8_linear(curData, static_cast<int>(curWidth), static_cast<int>(curHeight),
                                      static_cast<int>(curWidth * bpp), nextData, static_cast<int>(nextWidth),
                                      static_cast<int>(nextHeight), static_cast<int>(nextWidth * bpp),
                                      static_cast<stbir_pixel_layout>(channels - 1)); // STBIR_RGBA / STBIR_R / etc

            if (mip == 0) {
                // mip 0 的数据是 stb 分配的，释放它
                stbi_image_free(curData);
            } else {
                delete[] curData;
            }

            curData = nextData;
            curWidth = nextWidth;
            curHeight = nextHeight;
        } else {
            // 最后一级
            if (mip == 0) {
                stbi_image_free(curData);
            } else {
                delete[] curData;
            }
        }
    }

    std::cout << "[TextureImporter] Loaded '" << imagePath << "': " << outResult.Width << "x" << outResult.Height
              << ", " << outResult.MipLevels << " mips, " << outResult.MipData.size() << " bytes total" << std::endl;

    return true;
}

bool TextureImporter::WriteTTex(const std::string& ttexPath, const ImportResult& result) {
    std::ofstream file(ttexPath, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[TextureImporter] Failed to open output file: " << ttexPath << std::endl;
        return false;
    }

    TTexHeader header{};
    header.Format = static_cast<uint32_t>(result.Format);
    header.Width = result.Width;
    header.Height = result.Height;
    header.MipLevels = result.MipLevels;
    header.MipDataSize = static_cast<uint32_t>(result.MipData.size());

    file.write(reinterpret_cast<const char*>(&header), sizeof(TTexHeader));
    file.write(reinterpret_cast<const char*>(result.MipData.data()),
               static_cast<std::streamsize>(result.MipData.size()));

    file.close();

    size_t fileSize = sizeof(TTexHeader) + result.MipData.size();
    std::cout << "[TextureImporter] Written '" << ttexPath << "' (" << fileSize << " bytes)" << std::endl;

    return true;
}

} // namespace Tumbler
