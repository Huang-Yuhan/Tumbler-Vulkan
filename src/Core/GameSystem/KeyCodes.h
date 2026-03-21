#pragma once
#include <cstdint>

// Tumbler Engine 专属按键定义
enum class EKeyCode : uint16_t {
    Unknown = 0,

    // 字母键
    A, B, C, D, E, F, G, H, I, J, K, L, M,
    N, O, P, Q, R, S, T, U, V, W, X, Y, Z,

    // 控制键
    Escape,
    Space,
    Enter,
    LeftShift,
    LeftCtrl,

    // 方向键
    Up, Down, Left, Right,

    // 鼠标按键
    MouseLeft,
    MouseRight,
    MouseMiddle,

    // 用于标记枚举最大值，方便数组分配
    MaxKeys
};