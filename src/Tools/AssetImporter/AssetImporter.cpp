#include "AssetImporter.h"
#include "MeshImporter.h"
#include "SceneSerializer.h"
#include "TextureImporter.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;

namespace Tumbler {

// ============================================================================
// 辅助：从源文件路径派生 cooked 输出路径
// ============================================================================
static std::string DeriveCookedPath(const std::string& sourcePath, const std::string& outputDir,
                                    const std::string& cookedExtension) {
    fs::path src(sourcePath);
    std::string stem = src.stem().string(); // 去掉扩展名

    fs::path cookedDir = fs::path(outputDir) / "meshes";
    if (cookedExtension == ".ttex") {
        cookedDir = fs::path(outputDir) / "textures";
    } else if (cookedExtension == ".tmat") {
        // .tmat 保持原有相对路径结构，只是可能复制
        return sourcePath; // Material JSON 不转换（已经是 JSON）
    }

    fs::create_directories(cookedDir);
    return (cookedDir / (stem + cookedExtension)).string();
}

// 简单的 DJB2 hash
static uint32_t FileHash(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open())
        return 0;
    uint32_t h = 5381;
    char c;
    while (f.get(c))
        h = ((h << 5) + h) + static_cast<uint8_t>(c);
    return h;
}

// ============================================================================
// AssetImporter 实现
// ============================================================================

void AssetImporter::Init(const Config& cfg) {
    m_Config = cfg;
    fs::create_directories(cfg.OutputDir);
    fs::create_directories(fs::path(cfg.OutputDir) / "meshes");
    fs::create_directories(fs::path(cfg.OutputDir) / "textures");
    fs::create_directories(fs::path(cfg.OutputDir) / "materials");
}

void AssetImporter::Shutdown() {}

bool AssetImporter::ImportScene(const std::string& sceneJsonPath) {
    SceneSerializer serializer;

    // 1. 加载 Scene JSON
    if (!serializer.LoadScene(sceneJsonPath)) {
        return false;
    }

    // 2. 加载已有 asset_map（增量构建用）
    std::string assetMapPath = (fs::path(m_Config.OutputDir) / "asset_map.json").string();
    serializer.LoadAssetMap(assetMapPath);

    // 3. 收集依赖
    auto deps = serializer.CollectDependencies();
    std::cout << "[AssetImporter] Found " << deps.size() << " dependencies in scene." << std::endl;

    // 4. 处理每个依赖
    const std::string& baseDir = m_Config.OutputDir;

    for (const auto& dep : deps) {
        // 增量构建：检查 hash
        if (m_Config.bIncremental) {
            uint32_t currentHash = FileHash(dep.SourcePath);
            uint32_t storedHash = serializer.GetStoredHash(dep.SourcePath, dep.Type);
            if (currentHash == storedHash && storedHash != 0) {
                std::cout << "[AssetImporter] Skipping (unchanged): " << dep.SourcePath << std::endl;
                continue;
            }
        }

        if (m_Config.bDryRun) {
            std::cout << "[AssetImporter] [DRY RUN] Would process: " << dep.SourcePath << " (type: " << dep.Type << ")"
                      << std::endl;
            continue;
        }

        if (dep.Type == "mesh") {
            std::string cookedPath = DeriveCookedPath(dep.SourcePath, baseDir, ".tmesh");

            MeshImporter meshImporter;
            MeshImporter::ImportResult result;
            if (!meshImporter.Load(dep.SourcePath, result)) {
                std::cerr << "[AssetImporter] Failed to import mesh: " << dep.SourcePath << std::endl;
                continue;
            }
            if (!MeshImporter::WriteTMesh(cookedPath, result)) {
                std::cerr << "[AssetImporter] Failed to write mesh: " << cookedPath << std::endl;
                continue;
            }

            SceneSerializer::AssetMapEntry entry;
            entry.CookedPath = cookedPath;
            entry.SubMeshCount = static_cast<uint32_t>(result.SubMeshes.size());
            entry.SourceHash = FileHash(dep.SourcePath);
            serializer.UpdateAssetMap(dep.SourcePath, "meshes", entry);

        } else if (dep.Type == "texture") {
            std::string cookedPath = DeriveCookedPath(dep.SourcePath, baseDir, ".ttex");

            TextureImporter texImporter;
            TextureImporter::ImportResult result;
            if (!texImporter.Load(dep.SourcePath, result)) {
                std::cerr << "[AssetImporter] Failed to import texture: " << dep.SourcePath << std::endl;
                continue;
            }
            if (!TextureImporter::WriteTTex(cookedPath, result)) {
                std::cerr << "[AssetImporter] Failed to write texture: " << cookedPath << std::endl;
                continue;
            }

            SceneSerializer::AssetMapEntry entry;
            entry.CookedPath = cookedPath;
            entry.Format = std::to_string(result.Format);
            entry.MipLevels = result.MipLevels;
            entry.SourceHash = FileHash(dep.SourcePath);
            serializer.UpdateAssetMap(dep.SourcePath, "textures", entry);

        } else if (dep.Type == "material") {
            // Material 是 JSON 文件，本身不转换，但记录映射和纹理依赖
            SceneSerializer::AssetMapEntry entry;
            entry.CookedPath = dep.SourcePath; // 材质路径不变
            entry.SourceHash = FileHash(dep.SourcePath);

            // 收集材质引用的纹理
            std::ifstream matFile(dep.SourcePath);
            if (matFile.is_open()) {
                nlohmann::json matDoc = nlohmann::json::parse(matFile);
                for (const char* key : {"albedo", "normal", "metallicRoughness"}) {
                    if (matDoc.contains(key) && matDoc[key].is_string() && !matDoc[key].get<std::string>().empty()) {
                        entry.DependsOn.push_back(matDoc[key].get<std::string>());
                    }
                }
            }
            serializer.UpdateAssetMap(dep.SourcePath, "materials", entry);
        }
    }

    // 5. 写入 asset_map.json
    if (!m_Config.bDryRun) {
        serializer.WriteAssetMap(assetMapPath);
    }

    return true;
}

bool AssetImporter::ImportMesh(const std::string& sourcePath) {
    MeshImporter importer;
    MeshImporter::ImportResult result;
    if (!importer.Load(sourcePath, result))
        return false;

    std::string cookedPath = DeriveCookedPath(sourcePath, m_Config.OutputDir, ".tmesh");
    return MeshImporter::WriteTMesh(cookedPath, result);
}

bool AssetImporter::ImportTexture(const std::string& sourcePath) {
    TextureImporter importer;
    TextureImporter::ImportResult result;
    if (!importer.Load(sourcePath, result))
        return false;

    std::string cookedPath = DeriveCookedPath(sourcePath, m_Config.OutputDir, ".ttex");
    return TextureImporter::WriteTTex(cookedPath, result);
}

} // namespace Tumbler
