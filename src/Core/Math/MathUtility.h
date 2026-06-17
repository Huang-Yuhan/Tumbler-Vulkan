#pragma once

#include "Core/Math/MathConfig.h"

#include <algorithm>
#include <cmath>

namespace Tumbler::Math {

template <typename T> constexpr T Clamp(T value, T minValue, T maxValue) {
    return value < minValue ? minValue : (value > maxValue ? maxValue : value);
}

constexpr float DegreesToRadians(float degrees) {
    return degrees * (Pi / 180.0f);
}

constexpr float RadiansToDegrees(float radians) {
    return radians * (180.0f / Pi);
}

inline bool IsFinite(float value) {
    return std::isfinite(value);
}

inline bool IsNearlyZero(float value, float tolerance = SmallNumber) {
    return std::fabs(value) <= tolerance;
}

inline bool IsNearlyEqual(float a, float b, float tolerance = SmallNumber) {
    return std::fabs(a - b) <= tolerance;
}

inline float InvSqrt(float value) {
    return 1.0f / std::sqrt(value);
}

template <typename T> constexpr T Lerp(const T& a, const T& b, float t) {
    return a + (b - a) * t;
}

} // namespace Tumbler::Math
