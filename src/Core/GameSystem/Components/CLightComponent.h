#pragma once
#include "Component.h"
#include <Core/Math/Math.h>

class CLightComponent : public Component
{
public:
    Tumbler::Math::Vector3f Color = Tumbler::Math::Vector3f{1.0f, 1.0f, 1.0f};
    float Intensity = 5.0f;

    void OnDrawUI() override;
};
