#pragma once

#include "Core/Math/MathFwd.h"
#include "Core/Math/Vector.h"

#include <type_traits>

namespace Tumbler::Math {

template <typename T>
struct TVector4 {
    static_assert(std::is_floating_point_v<T>, "TVector4 requires a floating point type.");

    T X{};
    T Y{};
    T Z{};
    T W{};

    constexpr TVector4() = default;
    constexpr explicit TVector4(T value) : X(value), Y(value), Z(value), W(value) {}
    constexpr TVector4(T x, T y, T z, T w) : X(x), Y(y), Z(z), W(w) {}
    constexpr TVector4(const TVector3<T>& xyz, T w) : X(xyz.X), Y(xyz.Y), Z(xyz.Z), W(w) {}

    [[nodiscard]] static constexpr TVector4 Zero() { return TVector4{T(0), T(0), T(0), T(0)}; }

    [[nodiscard]] constexpr TVector3<T> XYZ() const { return TVector3<T>{X, Y, Z}; }

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
};

template <typename T>
[[nodiscard]] constexpr TVector4<T> operator*(T scalar, const TVector4<T>& vector) {
    return vector * scalar;
}

template <typename T>
[[nodiscard]] constexpr T Dot(const TVector4<T>& a, const TVector4<T>& b) {
    return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
}

} // namespace Tumbler::Math
