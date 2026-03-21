#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

#include "KeyCodes.h"

// 保持底层 API 的纯净，只前置声明
struct GLFWwindow;

class InputManager {
public:
    void Init(GLFWwindow* window);

    // 每一帧调用，更新按键状态和鼠标位移
    void Tick();

    // ==========================================
    // 1. 映射绑定接口
    // ==========================================
    // 绑定连续轴 (例如：MoveForward, 正向是W, 负向是S)
    void BindAxis(const std::string& name, EKeyCode positiveKey, EKeyCode negativeKey);
    // 绑定瞬时动作 (例如：Jump, 按键是 Space)
    void BindAction(const std::string& name, EKeyCode key);

    // ==========================================
    // 2. 状态查询接口
    // ==========================================
    // 获取轴的值 (返回 1.0, -1.0 或 0.0)
    float GetAxis(const std::string& name) const;

    // 动作是否一直被按住 (适合扫射机枪)
    bool IsActionPressed(const std::string& name) const;

    bool GetKey(EKeyCode key);

    // 动作是否在当前帧【刚刚】被按下 (适合跳跃、打开菜单)
    bool IsActionJustPressed(const std::string& name) const;

    // 获取鼠标相对上一帧的位移 (用于转动视角)
    glm::vec2 GetMouseDelta() const;

    // ==========================================
    // 3. UI 穿透拦截
    // ==========================================
    bool IsUIFocused() const;

private:
    GLFWwindow* WindowHandle = nullptr;

    struct AxisBinding { EKeyCode PosKey; EKeyCode NegKey; };
    std::unordered_map<std::string, AxisBinding> AxisBindings;
    std::unordered_map<std::string, EKeyCode> ActionBindings;

    // 为了实现 IsActionJustPressed，我们需要极度轻量级的状态缓存 (只有几百个字节)
    bool CurrentKeys[350] = {false};  // GLFW_KEY_LAST 大约是 348
    bool PreviousKeys[350] = {false};

    glm::vec2 LastMousePos{0.0f};
    glm::vec2 MouseDelta{0.0f};
    bool bFirstMouse = true;
};