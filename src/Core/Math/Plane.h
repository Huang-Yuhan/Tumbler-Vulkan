#pragma once

#include "Core/Math/MathFwd.h"
#include "Core/Math/Vector.h"

#include <cmath>
#include <concepts>

namespace Tumbler::Math {

template <typename T> requires std::floating_point<T> struct TPlane {

    T X{};
    T Y{};
    T Z{};
    T D{};

    constexpr TPlane() = default;
    constexpr TPlane(T x, T y, T z, T d) : X(x), Y(y), Z(z), D(d) {}
    constexpr TPlane(const TVector3<T>& normal, T d) : X(normal.X), Y(normal.Y), Z(normal.Z), D(d) {}

    [[nodiscard]] constexpr TVector3<T> Normal() const { return TVector3<T>{X, Y, Z}; }

    [[nodiscard]] constexpr T SignedDistance(const TVector3<T>& point) const {
        return X * point.X + Y * point.Y + Z * point.Z + D;
    }

    bool Normalize(T tolerance = static_cast<T>(SmallNumber)) {
        const T lengthSquared = X * X + Y * Y + Z * Z;
        if (lengthSquared <= tolerance * tolerance) {
            return false;
        }

        const T invLength = T(1) / std::sqrt(lengthSquared);
        X *= invLength;
        Y *= invLength;
        Z *= invLength;
        D *= invLength;
        return true;
    }
};

} // namespace Tumbler::Math
