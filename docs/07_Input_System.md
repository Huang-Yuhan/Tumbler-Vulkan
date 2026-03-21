# 输入系统 (Input System)

Tumbler 引擎的输入系统提供了灵活且易于使用的输入抽象层，封装了 GLFW 的底层输入 API，支持轴映射、动作绑定和鼠标输入。

## 1. 系统概述

输入系统的核心类是 `InputManager`，它负责：
- 封装 GLFW 窗口输入
- 管理按键状态缓存
- 提供轴（Axis）和动作（Action）的抽象绑定
- 处理鼠标位移

## 2. 初始化

```cpp
// 创建输入管理器
InputManager inputManager;

// 初始化，传入 GLFW 窗口句柄
inputManager.Init(window.GetNativeWindow());
```

## 3. 每帧更新

在游戏主循环的每一帧开始时，必须调用 `Tick()` 来更新输入状态：

```cpp
while (!window.ShouldClose()) {
    window.PollEvents();
    
    // 更新输入状态
    inputManager.Tick();
    
    // ... 游戏逻辑 ...
}
```

## 4. 轴输入 (Axis Input)

轴输入用于连续的范围值，适合移动、视角旋转等。

### 4.1 绑定轴

```cpp
// 绑定移动轴
inputManager.BindAxis("MoveForward", EKeyCode::W, EKeyCode::S);
inputManager.BindAxis("MoveRight",   EKeyCode::D, EKeyCode::A);
inputManager.BindAxis("MoveUp",      EKeyCode::E, EKeyCode::Q);
```

### 4.2 获取轴值

```cpp
// 获取轴值（返回 -1.0 到 1.0 之间的值）
float forward = inputManager.GetAxis("MoveForward");
float right   = inputManager.GetAxis("MoveRight");

// 使用轴值移动相机
glm::vec3 movement = (forward * forwardVector + right * rightVector) * speed * deltaTime;
cameraTransform.Translate(movement);
```

## 5. 动作输入 (Action Input)

动作输入用于瞬时事件，适合跳跃、开火、打开菜单等。

### 5.1 绑定动作

```cpp
// 绑定跳跃动作
inputManager.BindAction("Jump", EKeyCode::Space);

// 绑定开火动作
inputManager.BindAction("Fire", EKeyCode::MouseLeft);
```

### 5.2 检查动作状态

```cpp
// 检查动作是否被按住（适合持续开火）
if (inputManager.IsActionPressed("Fire")) {
    ShootBullet();
}

// 检查动作是否在当前帧刚刚按下（适合跳跃、菜单开关）
if (inputManager.IsActionJustPressed("Jump")) {
    player->Jump();
}

// 检查动作是否在当前帧刚刚释放
if (inputManager.IsActionJustReleased("Jump")) {
    player->StopJumping();
}
```

## 6. 直接按键查询

除了轴和动作绑定外，也可以直接查询单个按键的状态：

```cpp
// 检查某个键是否被按住
if (inputManager.GetKey(EKeyCode::LeftShift)) {
    speed = sprintSpeed;
}
```

## 7. 鼠标输入

### 7.1 获取鼠标位移

```cpp
// 获取鼠标相对上一帧的位移（适合转动视角）
glm::vec2 mouseDelta = inputManager.GetMouseDelta();

// 使用鼠标位移旋转相机
float yaw   += mouseDelta.x * mouseSensitivity;
float pitch += mouseDelta.y * mouseSensitivity;
```

### 7.2 第一人称相机示例

```cpp
class CFirstPersonCamera : public Component {
public:
    void Update(float deltaTime) override {
        InputManager* input = ...;
        
        // 鼠标转动视角
        glm::vec2 mouseDelta = input->GetMouseDelta();
        Yaw   += mouseDelta.x * Sensitivity;
        Pitch -= mouseDelta.y * Sensitivity;
        
        // 限制俯仰角
        Pitch = glm::clamp(Pitch, -89.0f, 89.0f);
        
        // 应用旋转
        Owner->Transform.SetRotation(
            glm::quat(glm::vec3(glm::radians(Pitch), glm::radians(Yaw), 0.0f))
        );
        
        // WASD 移动
        float forward = input->GetAxis("MoveForward");
        float right   = input->GetAxis("MoveRight");
        float up      = input->GetAxis("MoveUp");
        
        glm::vec3 moveDir = forward * GetForwardVector() 
                          + right * GetRightVector() 
                          + up * glm::vec3(0, 1, 0);
        
        Owner->Transform.Translate(moveDir * MoveSpeed * deltaTime);
    }
    
private:
    float Yaw = 0.0f;
    float Pitch = 0.0f;
    float MoveSpeed = 5.0f;
    float Sensitivity = 0.1f;
};
```

## 8. UI 穿透检测

当 ImGui 等 UI 获得焦点时，输入系统可以检测到这一点，避免游戏逻辑在 UI 交互时误触发：

```cpp
if (!inputManager.IsUIFocused()) {
    // 只有当 UI 没有焦点时才处理游戏输入
    ProcessGameInput();
}
```

## 9. KeyCodes (按键码)

所有支持的按键定义在 `KeyCodes.h` 中，包括：

### 9.1 键盘按键

```cpp
EKeyCode::Space
EKeyCode::A, EKeyCode::B, ..., EKeyCode::Z
EKeyCode::0, EKeyCode::1, ..., EKeyCode::9
EKeyCode::LeftShift, EKeyCode::RightShift
EKeyCode::LeftControl, EKeyCode::RightControl
EKeyCode::LeftAlt, EKeyCode::RightAlt
EKeyCode::Escape, EKeyCode::Enter, EKeyCode::Tab
// ... 更多按键
```

### 9.2 鼠标按键

```cpp
EKeyCode::MouseLeft
EKeyCode::MouseRight
EKeyCode::MouseMiddle
```

## 10. 完整示例

```cpp
// 初始化
InputManager inputManager;
inputManager.Init(window.GetNativeWindow());

// 绑定输入
inputManager.BindAxis("MoveForward", EKeyCode::W, EKeyCode::S);
inputManager.BindAxis("MoveRight",   EKeyCode::D, EKeyCode::A);
inputManager.BindAxis("MoveUp",      EKeyCode::E, EKeyCode::Q);
inputManager.BindAction("Jump",      EKeyCode::Space);

// 主循环
while (!window.ShouldClose()) {
    window.PollEvents();
    inputManager.Tick();
    
    // 只有当 UI 没焦点时才处理游戏输入
    if (!inputManager.IsUIFocused()) {
        // 移动
        float forward = inputManager.GetAxis("MoveForward");
        float right = inputManager.GetAxis("MoveRight");
        float up = inputManager.GetAxis("MoveUp");
        
        // 跳跃（只在按下瞬间触发）
        if (inputManager.IsActionJustPressed("Jump")) {
            player->Jump();
        }
        
        // 鼠标转动
        glm::vec2 mouseDelta = inputManager.GetMouseDelta();
        camera->Rotate(mouseDelta);
    }
    
    // ... 渲染 ...
}
```

## 11. 设计特点

| 特点 | 说明 |
|------|------|
| **抽象层** | 隐藏 GLFW 底层细节 |
| **状态缓存** | 使用双缓冲记录当前/上一帧状态 |
| **灵活绑定** | 支持运行时重新绑定输入 |
| **UI 感知** | 支持检测 UI 焦点避免误触发 |
| **鼠标位移** | 自动计算相对位移 |
