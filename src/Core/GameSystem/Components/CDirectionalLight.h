#pragma once

#include "Core/GameSystem/Components/Component.h"

namespace Tumbler {

// ============================================================================
// CDirectionalLight — 平行光源组件
// ============================================================================
class CDirectionalLight : public ::Component {
public:
    float Direction[3] = {0.0f, -1.0f, 0.0f};
    float Color[3] = {1.0f, 1.0f, 0.9f};
    float Intensity = 1.0f;
};

} // namespace Tumbler
