#include "Core/Engine/Engine.h"
#include "Core/Engine/EngineConfig.h"
#include "Core/Scene/SceneLoader.h"
#include "Core/Assets/AssetDatabase.h"
#include "Core/GameSystem/Components/CStaticMesh.h"
#include "Core/GameSystem/FScene.h"
#include "Core/Utils/Log.h"

using namespace Tumbler;

int main() {
    LogInit();

    // ---- 1. 加载引擎配置 ----
    EngineConfig config;
    if (!config.LoadFromFile("engine.json")) {
        LOG_CRITICAL("Failed to load engine.json");
        return 1;
    }

    // ---- 2. 初始化引擎 ----
    Engine engine;
    if (!engine.Init(config)) {
        LOG_CRITICAL("Engine initialization failed");
        return 1;
    }

    // ---- 3. 加载场景 ----
    FScene scene;
    SceneLoader::Result result;
    SceneLoader loader;

    if (!loader.LoadFromFile(scene, "assets/scenes/demo.tscene", *engine.GetAssetDatabase(), result)) {
        LOG_ERROR("Failed to load scene");
    } else {
        LOG_INFO("Scene loaded: {} mesh actors, {} light actors", result.MeshActors.size(), result.LightActors.size());

        // 输出每个 Mesh Actor 的信息
        for (const auto* actor : result.MeshActors) {
            auto* mesh = actor->GetComponent<CStaticMesh>();
            if (mesh) {
                LOG_INFO("  Mesh: '{}' source='{}' cooked='{}'", actor->Name, mesh->MeshSourcePath,
                          mesh->CookedMeshPath);
            }
        }
    }

    // ---- 4. 主循环 ----
    LOG_INFO("Entering main loop...");
    engine.Run();

    // ---- 5. 关闭 ----
    engine.Shutdown();
    LogShutdown();
    return 0;
}
