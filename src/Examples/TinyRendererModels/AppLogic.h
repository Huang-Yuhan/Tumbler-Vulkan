#pragma once
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/GameSystem/FScene.h"
#include <memory>
#include <string>
#include <vector>

class FMesh;
class FAssetManager;
class InputManager;
class CFirstPersonCamera;

class AppLogic
{
private:
    std::unique_ptr<FScene> Scene;
    FAssetManager* AssetMgr = nullptr;
    InputManager* InputMgr = nullptr;

    CFirstPersonCamera* MainCamera = nullptr;

    // 选中的物体
    FActor* SelectedActor = nullptr;

    // 性能统计数据
    static constexpr int FRAME_TIME_HISTORY_SIZE = 100;
    struct PerformanceStats {
        float FPS = 0.0f;
        float FrameTimeMs = 0.0f;
        int DrawCallCount = 0;
        float FrameTimeHistory[FRAME_TIME_HISTORY_SIZE] = {};
        int HistoryIndex = 0;
    } Stats;

    void InitializeScene();
    void LoadTinyRendererModels() const;

    void DrawPerformancePanel();
    void DrawLightPanel();
    void DrawCameraPanel();
    void DrawMaterialPanel();
    void DrawSceneHierarchyPanel();

public:
    AppLogic() = default;
    ~AppLogic();

    void Init(VulkanRenderer* renderer, FAssetManager* assetMgr, InputManager* inputMgr);
    void Tick(float deltaTime);

    void DrawEditorUI();
    void UpdatePerformanceStats(float frameTime, int drawCallCount);

    [[nodiscard]] FScene* GetScene();
    [[nodiscard]] const FScene* GetScene() const;
    [[nodiscard]] CFirstPersonCamera* GetMainCamera() const { return MainCamera; }
};
