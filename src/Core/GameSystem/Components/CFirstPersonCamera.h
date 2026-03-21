#pragma once
#include "Core/GameSystem/Components/CCamera.h"

class InputManager;

class CFirstPersonCamera : public CCamera {
public:
    void Init(InputManager* input);
    
    // Component 接口扩展 (目前引擎还未完全实现组件虚函数，我们手动调用)
    void Update(float deltaTime);

    float MoveSpeed = 5.0f;
    float MouseSensitivity = 0.1f;

private:
    InputManager* Input = nullptr;
    float Pitch = 0.0f;
    float Yaw = -90.0f;
};
