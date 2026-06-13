#pragma once
#include <Core/Math/Math.h>
#include <memory>

class FMesh;
class FMaterialInstance;

// 这就是我们的简易版 Render Proxy
struct RenderPacket {
    std::shared_ptr<FMesh> Mesh;
    std::shared_ptr<FMaterialInstance> Material;
    Tumbler::Math::Matrix4f TransformMatrix{Tumbler::Math::Matrix4f::Identity()};
};
