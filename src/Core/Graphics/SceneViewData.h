#pragma once
#include <Core/Math/Math.h>
#include <vector>
#include "LightData.h"

enum class ERenderPath {
    Forward,
    Deferred,
    GPUDriven
};

struct SceneViewData {
    Tumbler::Math::Matrix4f ViewMatrix{Tumbler::Math::Matrix4f::Identity()};
    Tumbler::Math::Matrix4f ProjectionMatrix{Tumbler::Math::Matrix4f::Identity()};
    Tumbler::Math::Vector3f CameraPosition{};

    std::vector<LightData> Lights;

    Tumbler::Math::Matrix4f LightViewProj{Tumbler::Math::Matrix4f::Identity()};

    ERenderPath RenderPath = ERenderPath::Forward;
};
