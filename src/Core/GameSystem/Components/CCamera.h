#pragma once

#include "Core/GameSystem/Components/Component.h"

namespace Tumbler {

// ============================================================================
// CCamera — 相机组件
// ============================================================================
class CCamera : public Component {
public:
    float FOV = 60.0f;
    float NearPlane = 0.1f;
    float FarPlane = 1000.0f;
    float LookAt[3] = {0, 0, 0};
};

} // namespace Tumbler
