#include "SceneSerializer.h"

#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Tumbler {

SceneSerializer::~SceneSerializer() = default;

// ============================================================================
// 辅助：计算文件路径的简单 hash（用于增量构建检测）
// ============================================================================
static uint32_t ComputeSimpleFileHash(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open())
        return 0;
    // 简单 hash: 读取首尾各 1KB + 文件大小
    uint32_t hash = 0;
    char buf[1024];
    size_t totalRead = 0;
    while (file.read(buf, sizeof(buf)) || file.gcount() > 0) {
        size_t n = static_cast<size_t>(file.gcount());
        for (size_t i = 0; i < n; i++) {
            hash = hash * 31 + static_cast<uint8_t>(buf[i]);
        }
        totalRead += n;
    }
    hash ^= static_cast<uint32_t>(totalRead);
    return hash;
}

// ============================================================================
// SceneSerializer 实现
// ============================================================================

bool SceneSerializer::LoadScene(const std::string& sceneJsonPath) {
    std::ifstream file(sceneJsonPath);
    if (!file.is_open()) {
        std::cerr << "[SceneSerializer] Failed to open scene file: " << sceneJsonPath << std::endl;
        return false;
    }

    try {
        json doc = json::parse(file);
        // 保存 JSON document
        m_JsonDoc = std::make_unique<json>(std::move(doc));

        m_SceneName = m_JsonDoc->value("name", "Untitled");
        std::cout << "[SceneSerializer] Loaded scene '" << m_SceneName << "'" << std::endl;
        return true;
    } catch (const json::exception& e) {
        std::cerr << "[SceneSerializer] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

std::vector<SceneSerializer::Dependency> SceneSerializer::CollectDependencies() const {
    std::vector<Dependency> deps;
    if (!m_JsonDoc)
        return deps;

    const json& doc = *m_JsonDoc;

    if (!doc.contains("objects") || !doc["objects"].is_array()) {
        return deps;
    }

    for (const auto& obj : doc["objects"]) {
        // Mesh dependency
        if (obj.contains("mesh") && obj["mesh"].is_string()) {
            deps.push_back({"mesh", obj["mesh"].get<std::string>()});
        }

        // Materials dependency (array, 按 materialSlot)
        if (obj.contains("materials") && obj["materials"].is_array()) {
            for (const auto& matEntry : obj["materials"]) {
                if (matEntry.is_null())
                    continue; // null = 使用默认材质

                std::string matPath = matEntry.get<std::string>();
                deps.push_back({"material", matPath});

                // 加载 material JSON 以获取纹理依赖
                std::ifstream matFile(matPath);
                if (matFile.is_open()) {
                    try {
                        json matDoc = json::parse(matFile);
                        for (const char* key : {"albedo", "normal", "metallicRoughness"}) {
                            if (matDoc.contains(key) && matDoc[key].is_string()) {
                                std::string texPath = matDoc[key].get<std::string>();
                                if (!texPath.empty() && !texPath.starts_with("cooked/")) {
                                    deps.push_back({"texture", texPath});
                                }
                            }
                        }
                    } catch (const json::exception& e) {
                        std::cerr << "[SceneSerializer] Failed to parse material: " << matPath << " (" << e.what()
                                  << ")" << std::endl;
                    }
                }
            }
        }
    }

    return deps;
}

bool SceneSerializer::LoadAssetMap(const std::string& assetMapPath) {
    std::ifstream file(assetMapPath);
    if (!file.is_open()) {
        // 文件不存在 = 全新的导入，不是错误
        std::cout << "[SceneSerializer] No existing asset_map.json found, starting fresh import." << std::endl;
        return true;
    }

    try {
        json mapDoc = json::parse(file);
        m_AssetMap.clear();

        // 解析分组: "meshes", "textures", "materials"
        for (const auto& [groupName, groupEntries] : mapDoc.items()) {
            if (groupName == "version")
                continue;
            if (!groupEntries.is_object())
                continue;

            std::map<std::string, AssetMapEntry> groupMap;
            for (const auto& [sourcePath, entryJson] : groupEntries.items()) {
                AssetMapEntry entry;
                entry.CookedPath = entryJson.value("cooked", "");
                entry.SubMeshCount = entryJson.value("subMeshCount", 0u);
                entry.Format = entryJson.value("format", "");
                entry.MipLevels = entryJson.value("mipLevels", 0u);
                entry.SourceHash = entryJson.value("sourceHash", 0u);
                if (entryJson.contains("dependsOn") && entryJson["dependsOn"].is_array()) {
                    for (const auto& dep : entryJson["dependsOn"]) {
                        entry.DependsOn.push_back(dep.get<std::string>());
                    }
                }
                groupMap[sourcePath] = std::move(entry);
            }
            m_AssetMap[groupName] = std::move(groupMap);
        }

        std::cout << "[SceneSerializer] Loaded asset_map.json" << std::endl;
        return true;
    } catch (const json::exception& e) {
        std::cerr << "[SceneSerializer] Failed to parse asset_map.json: " << e.what() << std::endl;
        return false;
    }
}

void SceneSerializer::UpdateAssetMap(const std::string& sourcePath, const std::string& type,
                                     const AssetMapEntry& entry) {
    m_AssetMap[type][sourcePath] = entry;
}

uint32_t SceneSerializer::GetStoredHash(const std::string& sourcePath, const std::string& type) const {
    auto typeIt = m_AssetMap.find(type);
    if (typeIt == m_AssetMap.end())
        return 0;
    auto pathIt = typeIt->second.find(sourcePath);
    if (pathIt == typeIt->second.end())
        return 0;
    return pathIt->second.SourceHash;
}

bool SceneSerializer::WriteAssetMap(const std::string& assetMapPath) const {
    json doc;
    doc["version"] = 1;

    for (const auto& [groupName, groupEntries] : m_AssetMap) {
        json groupJson = json::object();
        for (const auto& [sourcePath, entry] : groupEntries) {
            json entryJson;
            entryJson["cooked"] = entry.CookedPath;
            if (entry.SubMeshCount > 0)
                entryJson["subMeshCount"] = entry.SubMeshCount;
            if (!entry.Format.empty())
                entryJson["format"] = entry.Format;
            if (entry.MipLevels > 0)
                entryJson["mipLevels"] = entry.MipLevels;
            entryJson["sourceHash"] = entry.SourceHash;
            if (!entry.DependsOn.empty()) {
                entryJson["dependsOn"] = entry.DependsOn;
            }
            groupJson[sourcePath] = entryJson;
        }
        doc[groupName] = groupJson;
    }

    std::ofstream file(assetMapPath);
    if (!file.is_open()) {
        std::cerr << "[SceneSerializer] Failed to write asset_map: " << assetMapPath << std::endl;
        return false;
    }

    file << doc.dump(2) << std::endl;
    file.close();

    std::cout << "[SceneSerializer] Written asset_map.json" << std::endl;
    return true;
}

} // namespace Tumbler
