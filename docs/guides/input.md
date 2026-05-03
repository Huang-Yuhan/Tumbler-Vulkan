# 输入系统 (Input System)

Tumbler 引擎的输入系统提供了灵活且易于使用的输入抽象层，封装了 GLFW 的底层输入 API，支持轴映射、动作绑定、原始按键查询以及运行时 UI 输入阻断。

## 1. 系统概述

输入系统的核心类是 `InputManager`，它负责：
- 封装 GLFW 窗口输入
- 管理按键状态缓存
- 提供轴（Axis）和动作（Action）的抽象绑定
- 处理鼠标位移
- 提供 `WasKeyJustPressed()` 这类不受 Gameplay 阻断影响的原始按键查询
- 通过 `SetGameplayInputBlocked()` 在运行时控制台等 UI 打开时阻断游戏输入

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
    uiManager.TickInput();
    
    // ... 游戏逻辑 ...
}
```

推荐帧顺序：

1. `window.PollEvents()`
2. `inputManager.Tick()`
3. `uiManager.TickInput()`
4. `logic.Tick(frameTime)`
5. `uiManager.BeginFrame()`
6. `logic.DrawEditorUI()`
7. `uiManager.EndFrame()`

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

```

## 6. 直接按键查询

除了轴和动作绑定外，也可以直接查询单个按键的状态：

```cpp
// 检查某个键是否被按住
if (inputManager.GetKey(EKeyCode::LeftShift)) {
    speed = sprintSpeed;
}

// 检查某个键是否在本帧刚刚按下
if (inputManager.WasKeyJustPressed(EKeyCode::GraveAccent)) {
    // 例如：切换运行时控制台
}
```

## 7. 运行时 UI 输入阻断

`InputManager` 区分“Gameplay 输入”和“原始按键输入”：

- `GetAxis()`、`IsActionPressed()`、`IsActionJustPressed()`、`GetMouseDelta()` 会受到 UI 捕获和 `GameplayInputBlocked` 共同影响
- `WasKeyJustPressed()` 不受 Gameplay 阻断影响，适合做控制台、调试菜单这类全局切换键

示例：

```cpp
// 运行时控制台打开时，立即阻断相机移动与鼠标看向
inputManager.SetGameplayInputBlocked(true);

if (inputManager.IsGameplayInputBlocked()) {
    // 这里的 WASD、鼠标位移都会被归零
}
```

当前 Tumbler 示例中：

- `~` 用于打开/关闭运行时控制台
- 控制台打开后，同一帧开始阻断移动与鼠标视角输入
- 鼠标右键锁定会自动解除

### 7.1 实现细节

`InputManager` 内部维护两份按键缓存：

- `CurrentKeys[]`
- `PreviousKeys[]`

每帧 `Tick()` 的顺序大致是：

1. 复制上一帧状态到 `PreviousKeys`
2. 从 GLFW 读取当前键盘/鼠标状态写入 `CurrentKeys`
3. 先处理鼠标右键锁定/解锁
4. 再根据 `IsInputBlocked()` 决定是否清零 `MouseDelta`

这种顺序保证了：
- `WasKeyJustPressed()` 可以稳定通过 `Current && !Previous` 计算
- 控制台在打开的同一帧就能把鼠标位移归零
- 被阻断时如果此前处于鼠标锁定状态，会主动恢复系统光标

## 8. 鼠标输入

### 8.1 获取鼠标位移

```cpp
// 获取鼠标相对上一帧的位移（适合转动视角）
glm::vec2 mouseDelta = inputManager.GetMouseDelta();

// 使用鼠标位移旋转相机
float yaw   += mouseDelta.x * mouseSensitivity;
float pitch += mouseDelta.y * mouseSensitivity;
```

### 8.2 第一人称相机示例

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

## 9. UI 穿透检测

当 ImGui 等 UI 获得焦点时，输入系统可以检测到这一点，避免游戏逻辑在 UI 交互时误触发：

```cpp
if (!inputManager.IsInputBlocked()) {
    // 只有当 UI 没有焦点且 Gameplay 输入未被阻断时才处理游戏输入
    ProcessGameInput();
}
```

## 10. KeyCodes (按键码)

所有当前支持的按键定义在 `KeyCodes.h` 中，包括：

### 10.1 键盘按键

```cpp
EKeyCode::Space
EKeyCode::A, EKeyCode::B, ..., EKeyCode::Z
EKeyCode::Escape
EKeyCode::Enter
EKeyCode::GraveAccent
EKeyCode::LeftShift
EKeyCode::LeftCtrl
EKeyCode::Up, EKeyCode::Down, EKeyCode::Left, EKeyCode::Right
```

### 10.2 鼠标按键

```cpp
EKeyCode::MouseLeft
EKeyCode::MouseRight
EKeyCode::MouseMiddle
```

## 11. 完整示例

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
    uiManager.TickInput();
    
    // 只有当 UI 没焦点时才处理游戏输入
    if (!inputManager.IsInputBlocked()) {
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

## 12. 鼠标锁定功能 (Editor Camera 体验)

输入系统内置了鼠标锁定功能，用于实现类似 Unreal/Unity 编辑器的无尽拖拽体验：

### 11.1 功能说明

- 当按住鼠标右键（`EKeyCode::MouseRight`）时，自动锁定鼠标并隐藏光标
- 释放鼠标右键时，恢复鼠标正常模式
- 在 UI 获得焦点，或 Gameplay 输入被阻断时，自动解除鼠标锁定

### 11.2 实现细节

```cpp
// 在 InputManager::Tick() 中自动处理
// 1. 检测右键按下 -> 锁定鼠标
if (bMouseRightJustPressed && !IsUIFocused()) {
    glfwSetInputMode(WindowHandle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    bCursorLocked = true;
}

// 2. 检测右键释放 -> 解锁鼠标
else if (bMouseRightJustReleased) {
    glfwSetInputMode(WindowHandle, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    bCursorLocked = false;
}
```

## 12. 设计特点

| 特点 | 说明 |
|------|------|
| **抽象层** | 隐藏 GLFW 底层细节 |
| **状态缓存** | 使用双缓冲记录当前/上一帧状态 |
| **灵活绑定** | 支持运行时重新绑定输入 |
| **UI 感知** | 支持检测 UI 焦点避免误触发 |
| **鼠标位移** | 自动计算相对位移 |
| **鼠标锁定** | Editor Camera 无尽拖拽体验 |
| **类型安全** | 使用 `EKeyCode::MaxKeys` 自动管理数组大小 |
