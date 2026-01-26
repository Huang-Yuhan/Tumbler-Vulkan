//
// Created by Icecream_Sarkaz on 2026/1/20.
//

#include "CCamera.h"
#include "CTransform.h"

glm::mat4 CCamera::GetViewMatrix(const CTransform& transform)
{
    // 计算相机的前向量
    const auto position = transform.GetPosition();
    const auto forward = transform.GetForwardVector();
    const auto up = transform.GetUpVector();
    return glm::lookAt(position, position + forward, up);
}

glm::mat4 CCamera::GetProjectionMatrix(float aspectRatio) const {
    glm::mat4 projection = glm::perspective(glm::radians(Fov), aspectRatio, NearPlane, FarPlane);
    // Vulkan 的 Y 轴坐标系和 OpenGL 是反的，必须修补一下
    projection[1][1] *= -1;
    return projection;
}
