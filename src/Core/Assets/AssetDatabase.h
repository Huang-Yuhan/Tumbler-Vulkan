#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Tumbler {

// ============================================================================
// AssetDatabase — 运行时资产映射层
// ============================================================================
// 加载 cooked/asset_map.json，提供 源路径 → cooked 路径 的查询能力。
// 普通 RAII 类，不设单例。以后由 Engine 创建并传给需要的子系统。
class AssetDatabase {
public:
    // ---- 查询结果结构体 ----

    struct MeshMeta {
        std::string CookedPath; // "cooked/meshes/bunny.tmesh"
        uint32_t SubMeshCount = 0;
        uint32_t SourceHash = 0;
    };

    struct TextureMeta {
        std::string CookedPath; // "cooked/textures/wood.ttex"
        std::string Format;     // "R8G8B8A8_SRGB" 等
        uint32_t MipLevels = 0;
        uint32_t SourceHash = 0;
    };

    struct MaterialMeta {
        std::string CookedPath; // "cooked/materials/wall.tmat"
        uint32_t SourceHash = 0;
        std::vector<std::string> DependsOn; // 引用的纹理源路径
    };

    // ---- 生命周期 ----

    AssetDatabase() = default;
    ~AssetDatabase() = default;

    // 从 JSON 文件加载映射表
    bool LoadAssetMap(const std::string& assetMapPath);

    // 是否已加载
    [[nodiscard]] bool IsLoaded() const noexcept { return m_bLoaded; }

    // ---- 查询 API ----

    // 按类型和源路径查询 cooked 路径，找不到返回空字符串
    [[nodiscard]] std::string GetCookedPath(const std::string& sourcePath, const std::string& type) const;

    // 按类型获取所有条目的源路径列表（遍历用）
    [[nodiscard]] std::vector<std::string> GetAllSources(const std::string& type) const;

    // 获取具体元数据
    [[nodiscard]] const MeshMeta* GetMeshMeta(const std::string& sourcePath) const;
    [[nodiscard]] const TextureMeta* GetTextureMeta(const std::string& sourcePath) const;
    [[nodiscard]] const MaterialMeta* GetMaterialMeta(const std::string& sourcePath) const;

    // 原始条目数
    [[nodiscard]] size_t GetMeshCount() const noexcept { return m_Meshes.size(); }
    [[nodiscard]] size_t GetTextureCount() const noexcept { return m_Textures.size(); }
    [[nodiscard]] size_t GetMaterialCount() const noexcept { return m_Materials.size(); }

private:
    bool m_bLoaded = false;

    std::map<std::string, MeshMeta> m_Meshes;
    std::map<std::string, TextureMeta> m_Textures;
    std::map<std::string, MaterialMeta> m_Materials;
};

} // namespace Tumbler
