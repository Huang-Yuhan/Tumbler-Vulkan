#pragma once

#include "Core/Math/MathConfig.h"

#include <algorithm>
#include <cmath>

namespace Tumbler::Math {

// 数值工具函数
// Clamp: 将 value 钳制在 [minValue, maxValue] 范围内
// Lerp: 线性插值 a + (b-a)*t, t∈[0,1] 时在 ab 之间, 超出范围时为外推
// IsNearlyZero/IsNearlyEqual: 浮点容差比较, 默认容差 SmallNumber=1e-8

template <typename T> constexpr T Clamp(T value, T minValue, T maxValue) noexcept {
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

constexpr float DegreesToRadians(float degrees) noexcept {
    return degrees * (Pi / 180.0f);
}

constexpr float RadiansToDegrees(float radians) noexcept {
    return radians * (180.0f / Pi);
}

inline bool IsFinite(float value) noexcept {
    return std::isfinite(value);
}

inline bool IsNearlyZero(float value, float tolerance = SmallNumber) noexcept {
    return std::fabs(value) <= tolerance;
}

inline bool IsNearlyEqual(float a, float b, float tolerance = SmallNumber) noexcept {
    return std::fabs(a - b) <= tolerance;
}

inline float InvSqrt(float value) noexcept {
    return 1.0f / std::sqrt(value);
}

template <typename T> constexpr T Lerp(const T& a, const T& b, float t) noexcept {
    return a + (b - a) * t;
}

} // namespace Tumbler::Math
