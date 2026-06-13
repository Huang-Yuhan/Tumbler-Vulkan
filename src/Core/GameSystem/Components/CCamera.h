#pragma once
#include <type_traits>
#include <Core/Math/Math.h>
#include "Component.h"

class CTransform;

class CCamera : public Component
{
public:
    float Fov = 45.0f;
    float NearPlane = 0.1f;
    float FarPlane = 100.0f;

    static Tumbler::Math::Matrix4f GetViewMatrix(const CTransform& transform);
    Tumbler::Math::Matrix4f GetProjectionMatrix(float aspectRatio) const;
};

static_assert(std::is_base_of_v<Component, CCamera>, "CCamera must be a subclass of Component");
