#pragma once

#include "Core/GameSystem/Components/Component.h"

namespace Tumbler {

// ============================================================================
// CPointLight — 点光源组件
// ============================================================================
class CPointLight : public ::Component {
public:
    float Color[3] = {1.0f, 1.0f, 1.0f};
    float Intensity = 50.0f;
    float Range = 20.0f;
};

} // namespace Tumbler
