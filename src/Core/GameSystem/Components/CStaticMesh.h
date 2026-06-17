#pragma once

#include "Core/GameSystem/Components/Component.h"

#include <string>
#include <vector>

namespace Tumbler {

// ============================================================================
// FMaterialRef — 材质引用，逐步解析
// ============================================================================
struct FMaterialRef {
    std::string SourcePath; // "assets/materials/wall.tmat"
    std::string CookedPath; // 查 AssetDatabase 后填充
    int32_t GPUIndex = -1;  // ResourceManager 上传后填充
    bool bResolved = false; // CookedPath 已填充

    bool IsLoaded() const { return bResolved; }
};

// ============================================================================
// CStaticMesh — 静态网格组件
// ============================================================================
// 每个 FActor 最多一个 CStaticMesh，引用一个 mesh 资产 + 按 slot 覆盖材质
class CStaticMesh : public ::Component {
public:
    // 资产路径（源路径，SceneLoader 填入）
    std::string MeshSourcePath; // "assets/models/bunny.obj"
    std::string CookedMeshPath; // 查 AssetDatabase 后填充

    // 材质覆盖，索引对应 mesh 的 materialSlot
    std::vector<FMaterialRef> MaterialOverrides;

    // SceneLoader 用：添加材质引用
    void AddMaterialOverride(const std::string& sourcePath);
};

} // namespace Tumbler
