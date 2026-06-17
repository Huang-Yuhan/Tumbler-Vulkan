#pragma once

#include <string>
#include <vector>

namespace Tumbler {

// ============================================================================
// AssetImporter — 调度器
// ============================================================================
class AssetImporter {
public:
    struct Config {
        std::string OutputDir = "";
        bool bIncremental = false;
        bool bDryRun = false;
    };

    AssetImporter() = default;
    ~AssetImporter() = default;

    void Init(const Config& cfg);
    void Shutdown();

    // 模式 1: 处理完整场景（包括所有引用的网格和纹理）
    bool ImportScene(const std::string& sceneJsonPath);

    // 模式 2: 导入单个网格
    bool ImportMesh(const std::string& sourcePath);

    // 模式 3: 导入单个纹理
    bool ImportTexture(const std::string& sourcePath);

private:
    Config m_Config;
};

} // namespace Tumbler
