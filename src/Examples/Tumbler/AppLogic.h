#pragma once
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/GameSystem/FScene.h"
#include <memory>


class FMesh; // 前置声明

class AppLogic
{
private:
    std::unique_ptr<FScene> Scene;
    std::shared_ptr<FMesh> DefaultPlaneMesh; // 【新增】保存一个共用的平面网格

    void InitializeScene();
    void InitializePlanes() const;

public:
    AppLogic();
    ~AppLogic();
    [[nodiscard]] FScene* GetScene();             // 给需要修改场景的人用 (比如 ImGui)
    [[nodiscard]] const FScene* GetScene() const;
    [[nodiscard]] std::shared_ptr<FMesh> GetDefaultMesh() const { return DefaultPlaneMesh; } // 暴露给外部方便上传
    void InitializeMaterials(VulkanRenderer* renderer);
};