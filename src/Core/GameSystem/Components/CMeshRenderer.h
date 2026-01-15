#pragma once
#include "Component.h"
#include <memory>

class FMesh;
class FMaterial;

class CMeshRenderer final:public Component
{
private:
    std::shared_ptr<FMesh> MeshPtr=nullptr;
    std::shared_ptr<FMaterial> MaterialPtr = nullptr;

    bool bIsVisible=true;
public:
    CMeshRenderer();
    ~CMeshRenderer() override;

    [[nodiscard]] std::shared_ptr<FMesh> GetMesh() const { return MeshPtr; }
    void SetMesh(const std::shared_ptr<FMesh>& InMesh) { MeshPtr = InMesh; }
    [[nodiscard]] std::shared_ptr<FMaterial> GetMaterial() const { return MaterialPtr; }
    void SetMaterial(const std::shared_ptr<FMaterial>& InMaterial) { MaterialPtr = InMaterial; }
    [[nodiscard]] bool IsVisible() const { return bIsVisible; }
    void SetVisible(const bool bInIsVisible) { bIsVisible = bInIsVisible; }
};