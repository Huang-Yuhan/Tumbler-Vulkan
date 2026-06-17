#pragma once

#include <string>
#include <vector>

// ECS 类型在全局命名空间
class FScene;
class FActor;

namespace Tumbler {

class AssetDatabase;

// ============================================================================
// SceneLoader — Scene JSON → FScene 运行时实例
// ============================================================================
class SceneLoader {
public:
    struct Result {
        ::FActor* CameraActor = nullptr;    // 场景主相机
        std::vector<::FActor*> MeshActors;  // 所有网格对象
        std::vector<::FActor*> LightActors; // 所有光源
    };

    SceneLoader() = default;
    ~SceneLoader() = default;

    bool LoadFromFile(::FScene& scene, const std::string& jsonPath, const AssetDatabase& assetDb, Result& outResult);
};

} // namespace Tumbler
