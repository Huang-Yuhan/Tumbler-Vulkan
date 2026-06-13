//
// Created by Icecream_Sarkaz on 2026/1/20.
//

#include "CCamera.h"
#include "CTransform.h"

using namespace Tumbler::Math;

Matrix4f CCamera::GetViewMatrix(const CTransform& transform)
{
    const Vector3f position = transform.GetPosition();
    const Vector3f forward = transform.GetForwardVector();
    const Vector3f up = transform.GetUpVector();
    return MakeLookAt(position, position + forward, up);
}

Matrix4f CCamera::GetProjectionMatrix(float aspectRatio) const
{
    Matrix4f proj = MakePerspective(DegreesToRadians(Fov), aspectRatio, NearPlane, FarPlane);
    // Vulkan Y-axis flip
    proj[1][1] *= -1;
    return proj;
}
