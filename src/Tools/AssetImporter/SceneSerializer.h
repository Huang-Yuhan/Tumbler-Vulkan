#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Tumbler {

// ============================================================================
// SceneSerializer — Scene JSON 处理 + asset_map 生成
// ============================================================================
class SceneSerializer {
public:
    struct Dependency {
        std::string Type;       // "mesh", "texture", "material"
        std::string SourcePath; // 源文件路径
    };

    struct AssetMapEntry {
        std::string CookedPath;
        // Mesh 专属
        uint32_t SubMeshCount = 0;
        // Texture 专属
        std::string Format = "";
        uint32_t MipLevels = 0;
        // 增量构建
        uint32_t SourceHash = 0;
        // Material 专属
        std::vector<std::string> DependsOn;
    };

    SceneSerializer() = default;
    ~SceneSerializer() = default;

    // 读取 Scene JSON 文件
    bool LoadScene(const std::string& sceneJsonPath);

    // 收集所有依赖（mesh + texture + material）
    std::vector<Dependency> CollectDependencies() const;

    // 加载已有的 asset_map.json（增量构建用）
    bool LoadAssetMap(const std::string& assetMapPath);

    // 更新 asset_map（合并新条目，保留未变更的旧条目）
    void UpdateAssetMap(const std::string& sourcePath, const std::string& type, const AssetMapEntry& entry);

    // 写入 asset_map.json 到磁盘
    bool WriteAssetMap(const std::string& assetMapPath) const;

    // 获取 asset map 中某个条目的 hash（增量构建判断用）
    uint32_t GetStoredHash(const std::string& sourcePath, const std::string& type) const;

private:
    std::string m_SceneName;
    // 内部保存的 JSON 对象，用于遍历 objects / lights 等
    // nlohmann::json 在 .cpp 中使用
    void* m_JsonDoc = nullptr; // opaque pointer，避免头文件暴露 nlohmann/json

    // 按类型分组: m_AssetMap["meshes"]["path"] = AssetMapEntry
    std::map<std::string, std::map<std::string, AssetMapEntry>> m_AssetMap;
};

} // namespace Tumbler
