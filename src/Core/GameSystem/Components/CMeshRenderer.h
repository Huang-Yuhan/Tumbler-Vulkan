#pragma once
#include "Component.h"
#include <memory>

class FMesh;
class FMaterialInstance; // 【修改】前置声明改为实例

class CMeshRenderer final : public Component
{
private:
    std::shared_ptr<FMesh> MeshPtr = nullptr;
    std::shared_ptr<FMaterialInstance> MaterialPtr = nullptr; // 【修改】持有实例

    bool bIsVisible = true;
public:
    CMeshRenderer();
    ~CMeshRenderer() override;

    [[nodiscard]] std::shared_ptr<FMesh> GetMesh() const { return MeshPtr; }
    void SetMesh(const std::shared_ptr<FMesh>& InMesh) { MeshPtr = InMesh; }

    // 【修改】Get/Set 改为 FMaterialInstance
    [[nodiscard]] std::shared_ptr<FMaterialInstance> GetMaterial() const { return MaterialPtr; }
    void SetMaterial(const std::shared_ptr<FMaterialInstance>& InMaterial) { MaterialPtr = InMaterial; }

    [[nodiscard]] bool IsVisible() const { return bIsVisible; }
    void SetVisible(const bool bInIsVisible) { bIsVisible = bInIsVisible; }

    void OnDrawUI() override;
};