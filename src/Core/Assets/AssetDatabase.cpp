#include "AssetDatabase.h"

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace Tumbler {

bool AssetDatabase::LoadAssetMap(const std::string& assetMapPath) {
    std::ifstream file(assetMapPath);
    if (!file.is_open()) {
        std::cerr << "[AssetDatabase] Failed to open asset_map: " << assetMapPath << std::endl;
        return false;
    }

    try {
        json doc = json::parse(file);

        // 清空旧数据
        m_Meshes.clear();
        m_Textures.clear();
        m_Materials.clear();

        // 解析 meshes
        if (doc.contains("meshes") && doc["meshes"].is_object()) {
            for (const auto& [sourcePath, entryJson] : doc["meshes"].items()) {
                MeshMeta meta;
                meta.CookedPath = entryJson.value("cooked", "");
                meta.SubMeshCount = entryJson.value("subMeshCount", 0u);
                meta.SourceHash = entryJson.value("sourceHash", 0u);
                m_Meshes[sourcePath] = std::move(meta);
            }
        }

        // 解析 textures
        if (doc.contains("textures") && doc["textures"].is_object()) {
            for (const auto& [sourcePath, entryJson] : doc["textures"].items()) {
                TextureMeta meta;
                meta.CookedPath = entryJson.value("cooked", "");
                meta.Format = entryJson.value("format", "");
                meta.MipLevels = entryJson.value("mipLevels", 0u);
                meta.SourceHash = entryJson.value("sourceHash", 0u);
                m_Textures[sourcePath] = std::move(meta);
            }
        }

        // 解析 materials
        if (doc.contains("materials") && doc["materials"].is_object()) {
            for (const auto& [sourcePath, entryJson] : doc["materials"].items()) {
                MaterialMeta meta;
                meta.CookedPath = entryJson.value("cooked", "");
                meta.SourceHash = entryJson.value("sourceHash", 0u);
                if (entryJson.contains("dependsOn") && entryJson["dependsOn"].is_array()) {
                    for (const auto& dep : entryJson["dependsOn"]) {
                        meta.DependsOn.push_back(dep.get<std::string>());
                    }
                }
                m_Materials[sourcePath] = std::move(meta);
            }
        }

        m_bLoaded = true;
        std::cout << "[AssetDatabase] Loaded asset_map: " << m_Meshes.size() << " meshes, " << m_Textures.size()
                  << " textures, " << m_Materials.size() << " materials" << std::endl;
        return true;
    } catch (const json::exception& e) {
        std::cerr << "[AssetDatabase] JSON parse error: " << e.what() << std::endl;
        return false;
    }
}

std::string AssetDatabase::GetCookedPath(const std::string& sourcePath, const std::string& type) const {
    if (type == "meshes") {
        auto it = m_Meshes.find(sourcePath);
        return it != m_Meshes.end() ? it->second.CookedPath : "";
    }
    if (type == "textures") {
        auto it = m_Textures.find(sourcePath);
        return it != m_Textures.end() ? it->second.CookedPath : "";
    }
    if (type == "materials") {
        auto it = m_Materials.find(sourcePath);
        return it != m_Materials.end() ? it->second.CookedPath : "";
    }
    return "";
}

std::vector<std::string> AssetDatabase::GetAllSources(const std::string& type) const {
    std::vector<std::string> result;
    if (type == "meshes") {
        for (const auto& [path, meta] : m_Meshes)
            result.push_back(path);
    } else if (type == "textures") {
        for (const auto& [path, meta] : m_Textures)
            result.push_back(path);
    } else if (type == "materials") {
        for (const auto& [path, meta] : m_Materials)
            result.push_back(path);
    }
    return result;
}

const AssetDatabase::MeshMeta* AssetDatabase::GetMeshMeta(const std::string& sourcePath) const {
    auto it = m_Meshes.find(sourcePath);
    return it != m_Meshes.end() ? &it->second : nullptr;
}

const AssetDatabase::TextureMeta* AssetDatabase::GetTextureMeta(const std::string& sourcePath) const {
    auto it = m_Textures.find(sourcePath);
    return it != m_Textures.end() ? &it->second : nullptr;
}

const AssetDatabase::MaterialMeta* AssetDatabase::GetMaterialMeta(const std::string& sourcePath) const {
    auto it = m_Materials.find(sourcePath);
    return it != m_Materials.end() ? &it->second : nullptr;
}

} // namespace Tumbler
