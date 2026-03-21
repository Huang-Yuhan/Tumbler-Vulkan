#include "InputManager.h"
#include <GLFW/glfw3.h>
#include <imgui.h>

#include "KeyCodes.h"

static int TranslateToGLFWKey(EKeyCode code)
{
    switch (code)
    {
        case EKeyCode::Escape: return GLFW_KEY_ESCAPE;
            case EKeyCode::Space: return GLFW_KEY_SPACE;
            case EKeyCode::Enter: return GLFW_KEY_ENTER;
            case EKeyCode::A: return GLFW_KEY_A;
            case EKeyCode::B: return GLFW_KEY_B;
            case EKeyCode::C: return GLFW_KEY_C;
            case EKeyCode::D: return GLFW_KEY_D;
            case EKeyCode::E: return GLFW_KEY_E;
            case EKeyCode::F: return GLFW_KEY_F;
            case EKeyCode::G: return GLFW_KEY_G;
            case EKeyCode::H: return GLFW_KEY_H;
            case EKeyCode::I: return GLFW_KEY_I;
            case EKeyCode::J: return GLFW_KEY_J;
            case EKeyCode::K: return GLFW_KEY_K;
            case EKeyCode::L: return GLFW_KEY_L;
            case EKeyCode::M: return GLFW_KEY_M;
            case EKeyCode::N: return GLFW_KEY_N;
            case EKeyCode::O: return GLFW_KEY_O;
            case EKeyCode::P: return GLFW_KEY_P;
            case EKeyCode::Q: return GLFW_KEY_Q;
            case EKeyCode::R: return GLFW_KEY_R;
            case EKeyCode::S: return GLFW_KEY_S;
            case EKeyCode::T: return GLFW_KEY_T;
            case EKeyCode::U: return GLFW_KEY_U;
            case EKeyCode::V: return GLFW_KEY_V;
            case EKeyCode::W: return GLFW_KEY_W;
            case EKeyCode::X: return GLFW_KEY_X;
            case EKeyCode::Y: return GLFW_KEY_Y;
            case EKeyCode::Z: return GLFW_KEY_Z;
            case EKeyCode::Left: return GLFW_KEY_LEFT;
            case EKeyCode::Right: return GLFW_KEY_RIGHT;
            case EKeyCode::Up: return GLFW_KEY_UP;
            case EKeyCode::Down: return GLFW_KEY_DOWN;
            case EKeyCode::LeftShift: return GLFW_KEY_LEFT_SHIFT;
        default: return GLFW_KEY_UNKNOWN;
    }
}

void InputManager::Init(GLFWwindow* window) {
    WindowHandle = window;
}

void InputManager::Tick()
{
    // 1. 缓存上一帧的键盘状态，并读取当前帧状态
    // for (int i = 32; i < 350; ++i) { // GLFW 的按键宏从 32 开始
    //     PreviousKeys[i] = CurrentKeys[i];
    //     CurrentKeys[i] = glfwGetKey(WindowHandle, i) == GLFW_PRESS;
    // }

    for (uint16_t i=static_cast<uint16_t>(EKeyCode::Unknown)+1; i<static_cast<uint16_t>(EKeyCode::MaxKeys); i++)
    {
        auto glfwKey = TranslateToGLFWKey(static_cast<EKeyCode>(i));
// Handle mouse
        if (glfwKey == GLFW_KEY_UNKNOWN) continue;
        PreviousKeys[i]=CurrentKeys[i];
        CurrentKeys[i]=glfwGetKey(WindowHandle, glfwKey) == GLFW_PRESS;
    }

    // 2. 处理鼠标和 UI 穿透
    if (IsUIFocused()) {
        MouseDelta = glm::vec2(0.0f);
        bFirstMouse = true; // 防止鼠标离开 UI 界面时视角发生剧烈跳跃
        return;
    }

    double xpos, ypos;
    glfwGetCursorPos(WindowHandle, &xpos, &ypos);

    if (bFirstMouse) {
        LastMousePos = glm::vec2(xpos, ypos);
        bFirstMouse = false;
    }

    MouseDelta.x = static_cast<float>(xpos - LastMousePos.x);
    MouseDelta.y = static_cast<float>(ypos - LastMousePos.y);
    LastMousePos = glm::vec2(xpos, ypos);
}

void InputManager::BindAxis(const std::string& name, EKeyCode positiveKey, EKeyCode negativeKey) {
    AxisBindings[name] = {positiveKey, negativeKey};
}

void InputManager::BindAction(const std::string& name, EKeyCode key) {
    ActionBindings[name] = key;
}

float InputManager::GetAxis(const std::string& name) const {
    if (IsUIFocused()) return 0.0f; // 如果在输入 UI，禁止移动角色

    auto it = AxisBindings.find(name);
    if (it != AxisBindings.end()) {
        float val = 0.0f;
        if (CurrentKeys[static_cast<uint16_t>(it->second.PosKey)]) val += 1.0f;
        if (CurrentKeys[static_cast<uint16_t>(it->second.NegKey)]) val -= 1.0f;
        return val;
    }
    return 0.0f;
}

bool InputManager::IsActionPressed(const std::string& name) const {
    if (IsUIFocused()) return false;

    auto it = ActionBindings.find(name);
    if (it != ActionBindings.end()) {
        return CurrentKeys[static_cast<uint16_t>(it->second)];
    }
    return false;
}

bool InputManager::GetKey(EKeyCode key)
{
    int glfwKey=TranslateToGLFWKey(key);
    return CurrentKeys[static_cast<uint16_t>(key)];
}

bool InputManager::IsActionJustPressed(const std::string& name) const {
    if (IsUIFocused()) return false;

    auto it = ActionBindings.find(name);
    if (it != ActionBindings.end()) {
        EKeyCode key = it->second;
        return CurrentKeys[static_cast<uint16_t>(key)] && !PreviousKeys[static_cast<uint16_t>(key)]; // 只有当前按下且上一帧没按，才是“刚刚按下”
    }
    return false;
}

glm::vec2 InputManager::GetMouseDelta() const {
    return MouseDelta;
}

bool InputManager::IsUIFocused() const {
    // 只要 ImGui 想要接管鼠标或键盘，我们就认为 UI 激活了
    ImGuiIO& io = ImGui::GetIO();
    return io.WantCaptureMouse || io.WantCaptureKeyboard;
}