#include "CFirstPersonCamera.h"
#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/InputManager.h"
#include <algorithm>

void CFirstPersonCamera::Init(InputManager* input) {
    Input = input;
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
    Owner->Transform.SetRotation(glm::vec3(-Pitch, -Yaw + 90.0f, 0.0f));

    // 2. 直接从 Transform 白嫖算好的方向向量！极其优雅！
    glm::vec3 front = Owner->Transform.GetForwardVector();
    glm::vec3 right = Owner->Transform.GetRightVector();

    // 3. 空间位移
    float forwardInput = Input->GetAxis("MoveForward");
    float rightInput = Input->GetAxis("MoveRight");
    float upInput = Input->GetAxis("MoveUp");
    
    glm::vec3 pos = Owner->Transform.GetPosition();
    pos += front * forwardInput * MoveSpeed * deltaTime;
    pos += right * (-rightInput) * MoveSpeed * deltaTime;
    pos += glm::vec3(0.0f, 1.0f, 0.0f) * upInput * MoveSpeed * deltaTime;
    
    Owner->Transform.SetPosition(pos);
}
