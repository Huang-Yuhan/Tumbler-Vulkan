#pragma once
#include "CLightComponent.h"

class CDirectionalLight final : public CLightComponent
{
public:
    CDirectionalLight() { Intensity = 5.0f; }

    void OnDrawUI() override;
};
