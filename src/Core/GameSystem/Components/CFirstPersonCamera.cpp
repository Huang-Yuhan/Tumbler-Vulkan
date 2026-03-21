#include "CFirstPersonCamera.h"
#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/InputManager.h"
#include <algorithm>

void CFirstPersonCamera::Init(InputManager* input) {
    Input = input;
}

void CFirstPersonCamera::Update(float deltaTime) {
    if (!Input || !Owner) return;

    // 1. Rotation (Mouse) - Right Click 激活视角转动
    glm::vec2 mouseDelta = Input->GetMouseDelta();
    if (Input->GetKey(EKeyCode::MouseRight)) { 
        Yaw += mouseDelta.x * MouseSensitivity;
        Pitch -= mouseDelta.y * MouseSensitivity; // 鼠标上滑，y减小，Pitch 增加，抬头
        Pitch = std::clamp(Pitch, -89.0f, 89.0f); // 防万向节死锁
    }
    
    // 将欧拉角应用到 Transform 上
    // 引擎 Transform 的默认前向是 +Z，而我们用公式算出的 forward 中，Yaw=-90 对应 -Z。
    // 推导可得正确的映射应当是：X轴 = -Pitch, Y轴 = -Yaw + 90.0f
    Owner->Transform.SetRotation(glm::vec3(-Pitch, -Yaw + 90.0f, 0.0f));

    glm::vec3 front;
    front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front.y = sin(glm::radians(Pitch));
    front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
    front = glm::normalize(front);

    glm::vec3 right = glm::normalize(glm::cross(front, glm::vec3(0.0f, 1.0f, 0.0f)));

    // 2. Translation (Keyboard)
    float forwardInput = Input->GetAxis("MoveForward");
    float rightInput = Input->GetAxis("MoveRight");
    float upInput = Input->GetAxis("MoveUp");
    
    glm::vec3 pos = Owner->Transform.GetPosition();
    pos += front * forwardInput * MoveSpeed * deltaTime;
    pos += right * rightInput * MoveSpeed * deltaTime;
    pos += glm::vec3(0.0f, 1.0f, 0.0f) * upInput * MoveSpeed * deltaTime;
    
    Owner->Transform.SetPosition(pos);
}
