#pragma once

#include "Core/Math/MathFwd.h"
#include "Core/Math/Vector.h"

#include <cmath>
#include <concepts>

namespace Tumbler::Math {

// 齐次 4D 向量 (x, y, z, w) — 用于 RGBA 颜色、同质坐标、平面方程系数等。
template <typename T>
    requires std::floating_point<T>
struct TVector4 {

    T X{};
    T Y{};
    T Z{};
    T W{};

    constexpr TVector4() = default;
    constexpr explicit TVector4(T value) : X(value), Y(value), Z(value), W(value) {}
    constexpr TVector4(T x, T y, T z, T w) : X(x), Y(y), Z(z), W(w) {}
    constexpr TVector4(const TVector3<T>& xyz, T w) : X(xyz.X), Y(xyz.Y), Z(xyz.Z), W(w) {}

    [[nodiscard]] static constexpr TVector4 Zero() { return TVector4{T(0), T(0), T(0), T(0)}; }
    [[nodiscard]] static constexpr TVector4 One() { return TVector4{T(1), T(1), T(1), T(1)}; }
    [[nodiscard]] static constexpr TVector4 UnitX() { return TVector4{T(1), T(0), T(0), T(0)}; }
    [[nodiscard]] static constexpr TVector4 UnitY() { return TVector4{T(0), T(1), T(0), T(0)}; }
    [[nodiscard]] static constexpr TVector4 UnitZ() { return TVector4{T(0), T(0), T(1), T(0)}; }
    [[nodiscard]] static constexpr TVector4 UnitW() { return TVector4{T(0), T(0), T(0), T(1)}; }

    [[nodiscard]] constexpr TVector3<T> XYZ() const { return TVector3<T>{X, Y, Z}; }

    [[nodiscard]] constexpr TVector4 operator+() const { return *this; }
    [[nodiscard]] constexpr TVector4 operator-() const { return TVector4{-X, -Y, -Z, -W}; }

    [[nodiscard]] constexpr TVector4 operator+(const TVector4& other) const {
        return TVector4{X + other.X, Y + other.Y, Z + other.Z, W + other.W};
    }

    [[nodiscard]] constexpr TVector4 operator-(const TVector4& other) const {
        return TVector4{X - other.X, Y - other.Y, Z - other.Z, W - other.W};
    }

    [[nodiscard]] constexpr TVector4 operator*(T scalar) const {
        return TVector4{X * scalar, Y * scalar, Z * scalar, W * scalar};
    }

    [[nodiscard]] constexpr TVector4 operator/(T scalar) const {
        return TVector4{X / scalar, Y / scalar, Z / scalar, W / scalar};
    }

    constexpr TVector4& operator+=(const TVector4& other) {
        X += other.X;
        Y += other.Y;
        Z += other.Z;
        W += other.W;
        return *this;
    }

    constexpr TVector4& operator-=(const TVector4& other) {
        X -= other.X;
        Y -= other.Y;
        Z -= other.Z;
        W -= other.W;
        return *this;
    }

    constexpr TVector4& operator*=(T scalar) {
        X *= scalar;
        Y *= scalar;
        Z *= scalar;
        W *= scalar;
        return *this;
    }

    constexpr TVector4& operator/=(T scalar) {
        X /= scalar;
        Y /= scalar;
        Z /= scalar;
        W /= scalar;
        return *this;
    }

    [[nodiscard]] constexpr T LengthSquared() const { return X * X + Y * Y + Z * Z + W * W; }
    [[nodiscard]] T Length() const { return std::sqrt(LengthSquared()); }

    [[nodiscard]] bool IsNearlyZero(T tolerance = static_cast<T>(SmallNumber)) const {
        return Math::IsNearlyZero(X, tolerance) && Math::IsNearlyZero(Y, tolerance) &&
               Math::IsNearlyZero(Z, tolerance) && Math::IsNearlyZero(W, tolerance);
    }

    bool Normalize(T tolerance = static_cast<T>(SmallNumber)) {
        const T lengthSquared = LengthSquared();
        if (lengthSquared <= tolerance * tolerance) {
            X = T(0);
            Y = T(0);
            Z = T(0);
            W = T(0);
            return false;
        }

        const T invLength = T(1) / std::sqrt(lengthSquared);
        X *= invLength;
        Y *= invLength;
        Z *= invLength;
        W *= invLength;
        return true;
    }

    [[nodiscard]] TVector4 GetNormalized(T tolerance = static_cast<T>(SmallNumber)) const {
        TVector4 result = *this;
        result.Normalize(tolerance);
        return result;
    }
};

template <typename T> [[nodiscard]] constexpr TVector4<T> operator*(T scalar, const TVector4<T>& vector) {
    return vector * scalar;
}

template <typename T> [[nodiscard]] constexpr T Dot(const TVector4<T>& a, const TVector4<T>& b) {
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
}

} // namespace Tumbler::Math
