#pragma once
#include "Core/Graphics/VulkanRenderer.h"
#include "Core/GameSystem/FScene.h"
#include <memory>
#include <string>
#include <vector>

class FMesh; // 前置声明

class AppLogic
{
private:
    std::unique_ptr<FScene> Scene;
    std::shared_ptr<FMesh> DefaultPlaneMesh; // 共用的平面网格(Cornell Box)
    std::vector<std::shared_ptr<FMesh>> ObjMeshes; // 【新增】存放所有外部加载的 OBJ 模型

    void InitializeScene();
    void InitializePlanes() const;

public:
    AppLogic();
    ~AppLogic();
    [[nodiscard]] FScene* GetScene();
    [[nodiscard]] const FScene* GetScene() const;
    [[nodiscard]] std::shared_ptr<FMesh> GetDefaultMesh() const { return DefaultPlaneMesh; }
    void InitializeMaterials(VulkanRenderer* renderer);
    
    // 【新增】加载一个 OBJ，创建 Actor 挂到场景里，返回 FMesh 的 shared_ptr 供上传
    std::shared_ptr<FMesh> LoadOBJMesh(const std::string& filePath, const std::string& actorName);
};