#pragma once
#include <type_traits>
#include <glm/glm.hpp>
#include "Component.h"


class CTransform;

class CCamera : public Component
{
public:
    float Fov=45.0f;
    float NearPlane=0.1f;
    float FarPlane=100.0f;

    static glm::mat4 GetViewMatrix(const CTransform& transform);
    glm::mat4 GetProjectionMatrix(float aspectRatio) const;
};


static_assert(std::is_base_of_v<Component, CCamera>, "CCamera must be a subclass of Component");