#pragma once
#include "Component.h"
#include <glm/glm.hpp>

class CLightComponent : public Component
{
public:
    glm::vec3 Color     = glm::vec3(1.0f, 1.0f, 1.0f);
    float     Intensity = 5.0f;

    void OnDrawUI() override;
};
