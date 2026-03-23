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

    void InitializeScene();
    void LoadTinyRendererModels() const;

public:
    AppLogic() = default;
    ~AppLogic();

    void Init(VulkanRenderer* renderer, FAssetManager* assetMgr, InputManager* inputMgr);
    void Tick(float deltaTime);

    [[nodiscard]] FScene* GetScene();
    [[nodiscard]] const FScene* GetScene() const;
    [[nodiscard]] CFirstPersonCamera* GetMainCamera() const { return MainCamera; }
};
