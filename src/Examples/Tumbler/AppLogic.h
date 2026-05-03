#pragma once
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/GameSystem/FScene.h"
#include <memory>
#include <string>
#include <vector>

class FMesh; // 前置声明
class FAssetManager;
class InputManager;
class CFirstPersonCamera;
class FActor;
class CMeshRenderer;
struct EditorSessionState;

class AppLogic
{
private:
    std::unique_ptr<FScene> Scene;
    FAssetManager* AssetMgr = nullptr;
    InputManager* InputMgr = nullptr;
    EditorSessionState* SessionState = nullptr;
    VulkanRenderer* Renderer = nullptr;

    // 缓存第一人称漫游相机组件
    CFirstPersonCamera* MainCamera = nullptr;

    // 性能统计数据
    static constexpr int FRAME_TIME_HISTORY_SIZE = 100;
    struct PerformanceStats {
        float FPS = 0.0f;
        float FrameTimeMs = 0.0f;
        int DrawCallCount = 0;
        float FrameTimeHistory[FRAME_TIME_HISTORY_SIZE] = {};
        int HistoryIndex = 0;
    } Stats;

    // GBuffer 调试预览资源
    VkSampler DebugSampler = VK_NULL_HANDLE;
    VkDescriptorSetLayout DebugGBufferSetLayout = VK_NULL_HANDLE;
    VkDescriptorSet DebugAlbedoDescSet = VK_NULL_HANDLE;
    VkDescriptorSet DebugNormalDescSet = VK_NULL_HANDLE;

    void InitializeScene();
    void InitializePlanes() const;
    void InitDebugResources();
    void DestroyDebugResources();

    void DrawPerformanceSection();
    void DrawCameraSection();
    void DrawLightingSection();
    void DrawRenderingSection();
    void DrawDebugPanel();
    void DrawInspectorPanel();
    void DrawSceneHierarchyPanel();
    void UpdateDebugGBufferDescriptors();
    bool ValidateSelectedActor();
    [[nodiscard]] int CountPointLights() const;

public:
    AppLogic() = default;
    ~AppLogic();

    void Init(VulkanRenderer* renderer, FAssetManager* assetMgr, InputManager* inputMgr, EditorSessionState* sessionState);
    void Tick(float deltaTime);

    void DrawEditorUI();
    void UpdatePerformanceStats(float frameTime, int drawCallCount);

    [[nodiscard]] FScene* GetScene();
    [[nodiscard]] const FScene* GetScene() const;
    [[nodiscard]] CFirstPersonCamera* GetMainCamera() const { return MainCamera; }
};
