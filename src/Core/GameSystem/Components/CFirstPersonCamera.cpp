#include "CFirstPersonCamera.h"
#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/InputManager.h"
#include <algorithm>

using namespace Tumbler::Math;

void CFirstPersonCamera::Init(InputManager* input) {
    Input = input;
}

void CFirstPersonCamera::SetLookEuler(const Vector3f& eulerDegrees)
{
    Pitch = -eulerDegrees.X;
    Yaw = -eulerDegrees.Y + 90.0f;

    if (Owner) {
        Owner->Transform.SetRotation(eulerDegrees);
    }
}

void CFirstPersonCamera::Update(float deltaTime) {
    if (!Input || !Owner) return;

    // 1. 视角旋转
    glm::vec2 mouseDelta = Input->GetMouseDelta();
    if (Input->GetKey(EKeyCode::MouseRight)) {
        Yaw += mouseDelta.x * MouseSensitivity;
        Pitch -= mouseDelta.y * MouseSensitivity;
        Pitch = std::clamp(Pitch, -89.0f, 89.0f);
    }
    Owner->Transform.SetRotation(Vector3f{-Pitch, -Yaw + 90.0f, 0.0f});

    // 2. 直接从 Transform 获取算好的方向向量
    Vector3f front = Owner->Transform.GetForwardVector();
    Vector3f right = Owner->Transform.GetRightVector();

    // 3. 空间位移
    float forwardInput = Input->GetAxis("MoveForward");
    float rightInput = Input->GetAxis("MoveRight");
    float upInput = Input->GetAxis("MoveUp");

    Vector3f pos = Owner->Transform.GetPosition();
    pos += front * forwardInput * MoveSpeed * deltaTime;
    pos += right * (-rightInput) * MoveSpeed * deltaTime;
    pos += Vector3f{0.0f, 1.0f, 0.0f} * upInput * MoveSpeed * deltaTime;

    Owner->Transform.SetPosition(pos);
}
