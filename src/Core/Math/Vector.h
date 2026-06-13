#pragma once

#include "Core/Math/MathFwd.h"
#include "Core/Math/MathUtility.h"

#include <cmath>
#include <type_traits>

namespace Tumbler::Math {

template <typename T>
struct TVector3 {
    static_assert(std::is_floating_point_v<T>, "TVector3 requires a floating point type.");

    T X{};
    T Y{};
    T Z{};

    constexpr TVector3() = default;
    constexpr explicit TVector3(T value) : X(value), Y(value), Z(value) {}
    constexpr TVector3(T x, T y, T z) : X(x), Y(y), Z(z) {}

    [[nodiscard]] static constexpr TVector3 Zero() { return TVector3{T(0), T(0), T(0)}; }
    [[nodiscard]] static constexpr TVector3 One() { return TVector3{T(1), T(1), T(1)}; }
    [[nodiscard]] static constexpr TVector3 UnitX() { return TVector3{T(1), T(0), T(0)}; }
    [[nodiscard]] static constexpr TVector3 UnitY() { return TVector3{T(0), T(1), T(0)}; }
    [[nodiscard]] static constexpr TVector3 UnitZ() { return TVector3{T(0), T(0), T(1)}; }

    [[nodiscard]] constexpr TVector3 operator+() const { return *this; }
    [[nodiscard]] constexpr TVector3 operator-() const { return TVector3{-X, -Y, -Z}; }

    [[nodiscard]] constexpr TVector3 operator+(const TVector3& other) const {
        return TVector3{X + other.X, Y + other.Y, Z + other.Z};
    }

    [[nodiscard]] constexpr TVector3 operator-(const TVector3& other) const {
        return TVector3{X - other.X, Y - other.Y, Z - other.Z};
    }

    [[nodiscard]] constexpr TVector3 operator*(T scalar) const { return TVector3{X * scalar, Y * scalar, Z * scalar}; }
    [[nodiscard]] constexpr TVector3 operator/(T scalar) const { return TVector3{X / scalar, Y / scalar, Z / scalar}; }

    constexpr TVector3& operator+=(const TVector3& other) {
        X += other.X;
        Y += other.Y;
        Z += other.Z;
        return *this;
    }

    constexpr TVector3& operator-=(const TVector3& other) {
        X -= other.X;
        Y -= other.Y;
        Z -= other.Z;
        return *this;
    }

    constexpr TVector3& operator*=(T scalar) {
        X *= scalar;
        Y *= scalar;
        Z *= scalar;
        return *this;
    }

    constexpr TVector3& operator/=(T scalar) {
        X /= scalar;
        Y /= scalar;
        Z /= scalar;
        return *this;
    }

    [[nodiscard]] constexpr T LengthSquared() const { return X * X + Y * Y + Z * Z; }
    [[nodiscard]] T Length() const { return std::sqrt(LengthSquared()); }

    [[nodiscard]] bool IsNearlyZero(T tolerance = static_cast<T>(SmallNumber)) const {
        return Math::IsNearlyZero(X, tolerance) && Math::IsNearlyZero(Y, tolerance) &&
               Math::IsNearlyZero(Z, tolerance);
    }

    bool Normalize(T tolerance = static_cast<T>(SmallNumber)) {
        const T lengthSquared = LengthSquared();
        if (lengthSquared <= tolerance * tolerance) {
            X = T(0);
            Y = T(0);
            Z = T(0);
            return false;
        }

        const T invLength = T(1) / std::sqrt(lengthSquared);
        X *= invLength;
        Y *= invLength;
        Z *= invLength;
        return true;
    }

    [[nodiscard]] TVector3 GetNormalized(T tolerance = static_cast<T>(SmallNumber)) const {
        TVector3 result = *this;
        result.Normalize(tolerance);
        return result;
    }
};

template <typename T>
[[nodiscard]] constexpr TVector3<T> operator*(T scalar, const TVector3<T>& vector) {
    return vector * scalar;
}

template <typename T>
[[nodiscard]] constexpr T Dot(const TVector3<T>& a, const TVector3<T>& b) {
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

template <typename T>
[[nodiscard]] constexpr TVector3<T> Cross(const TVector3<T>& a, const TVector3<T>& b) {
    return TVector3<T>{a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X};
}

} // namespace Tumbler::Math
