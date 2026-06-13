#pragma once

#include "Core/Math/MathFwd.h"
#include "Core/Math/MathUtility.h"

#include <cmath>
#include <type_traits>

namespace Tumbler::Math {

template <typename T>
struct TVector2 {
    static_assert(std::is_floating_point_v<T>, "TVector2 requires a floating point type.");

    T X{};
    T Y{};

    constexpr TVector2() = default;
    constexpr explicit TVector2(T value) : X(value), Y(value) {}
    constexpr TVector2(T x, T y) : X(x), Y(y) {}

    [[nodiscard]] static constexpr TVector2 Zero() { return TVector2{T(0), T(0)}; }
    [[nodiscard]] static constexpr TVector2 One() { return TVector2{T(1), T(1)}; }
    [[nodiscard]] static constexpr TVector2 UnitX() { return TVector2{T(1), T(0)}; }
    [[nodiscard]] static constexpr TVector2 UnitY() { return TVector2{T(0), T(1)}; }

    [[nodiscard]] constexpr TVector2 operator+() const { return *this; }
    [[nodiscard]] constexpr TVector2 operator-() const { return TVector2{-X, -Y}; }

    [[nodiscard]] constexpr TVector2 operator+(const TVector2& other) const {
        return TVector2{X + other.X, Y + other.Y};
    }

    [[nodiscard]] constexpr TVector2 operator-(const TVector2& other) const {
        return TVector2{X - other.X, Y - other.Y};
    }

    [[nodiscard]] constexpr TVector2 operator*(T scalar) const { return TVector2{X * scalar, Y * scalar}; }
    [[nodiscard]] constexpr TVector2 operator/(T scalar) const { return TVector2{X / scalar, Y / scalar}; }

    constexpr TVector2& operator+=(const TVector2& other) {
        X += other.X;
        Y += other.Y;
        return *this;
    }

    constexpr TVector2& operator-=(const TVector2& other) {
        X -= other.X;
        Y -= other.Y;
        return *this;
    }

    constexpr TVector2& operator*=(T scalar) {
        X *= scalar;
        Y *= scalar;
        return *this;
    }

    constexpr TVector2& operator/=(T scalar) {
        X /= scalar;
        Y /= scalar;
        return *this;
    }

    [[nodiscard]] constexpr T LengthSquared() const { return X * X + Y * Y; }
    [[nodiscard]] T Length() const { return std::sqrt(LengthSquared()); }

    [[nodiscard]] bool IsNearlyZero(T tolerance = static_cast<T>(SmallNumber)) const {
        return Math::IsNearlyZero(X, tolerance) && Math::IsNearlyZero(Y, tolerance);
    }

    bool Normalize(T tolerance = static_cast<T>(SmallNumber)) {
        const T lengthSquared = LengthSquared();
        if (lengthSquared <= tolerance * tolerance) {
            X = T(0);
            Y = T(0);
            return false;
        }

        const T invLength = T(1) / std::sqrt(lengthSquared);
        X *= invLength;
        Y *= invLength;
        return true;
    }

    [[nodiscard]] TVector2 GetNormalized(T tolerance = static_cast<T>(SmallNumber)) const {
        TVector2 result = *this;
        result.Normalize(tolerance);
        return result;
    }
};

template <typename T>
[[nodiscard]] constexpr TVector2<T> operator*(T scalar, const TVector2<T>& vector) {
    return vector * scalar;
}

template <typename T>
[[nodiscard]] constexpr T Dot(const TVector2<T>& a, const TVector2<T>& b) {
    return a.X * b.X + a.Y * b.Y;
}

} // namespace Tumbler::Math
