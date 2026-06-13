#pragma once

#include "Core/Math/MathConfig.h"
#include "Core/Math/MathFwd.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Vector4.h"

#include <cmath>
#include <type_traits>

namespace Tumbler::Math {

template <typename T>
struct TMatrix4 {
    static_assert(std::is_floating_point_v<T>, "TMatrix4 requires a floating point type.");

    T M[4][4]{};

    constexpr TMatrix4() = default;

    constexpr TMatrix4(T m00, T m01, T m02, T m03, T m10, T m11, T m12, T m13, T m20, T m21, T m22, T m23,
                       T m30, T m31, T m32, T m33)
        : M{{m00, m01, m02, m03}, {m10, m11, m12, m13}, {m20, m21, m22, m23}, {m30, m31, m32, m33}} {}

    [[nodiscard]] static constexpr TMatrix4 Identity() {
        return TMatrix4{T(1), T(0), T(0), T(0), T(0), T(1), T(0), T(0),
                        T(0), T(0), T(1), T(0), T(0), T(0), T(0), T(1)};
    }

    [[nodiscard]] constexpr const T* operator[](int row) const { return M[row]; }
    [[nodiscard]] constexpr T* operator[](int row) { return M[row]; }

    [[nodiscard]] constexpr TMatrix4 operator*(const TMatrix4& other) const {
        TMatrix4 result;
        for (int row = 0; row < 4; ++row) {
            for (int column = 0; column < 4; ++column) {
                result.M[row][column] = M[row][0] * other.M[0][column] + M[row][1] * other.M[1][column] +
                                        M[row][2] * other.M[2][column] + M[row][3] * other.M[3][column];
            }
        }
        return result;
    }

    [[nodiscard]] constexpr TVector4<T> TransformVector4(const TVector4<T>& vector) const {
        return TVector4<T>{M[0][0] * vector.X + M[0][1] * vector.Y + M[0][2] * vector.Z + M[0][3] * vector.W,
                           M[1][0] * vector.X + M[1][1] * vector.Y + M[1][2] * vector.Z + M[1][3] * vector.W,
                           M[2][0] * vector.X + M[2][1] * vector.Y + M[2][2] * vector.Z + M[2][3] * vector.W,
                           M[3][0] * vector.X + M[3][1] * vector.Y + M[3][2] * vector.Z + M[3][3] * vector.W};
    }

    [[nodiscard]] constexpr TVector4<T> TransformPosition(const TVector3<T>& position) const {
        return TransformVector4(TVector4<T>{position, T(1)});
    }

    [[nodiscard]] constexpr TVector4<T> TransformVector(const TVector3<T>& vector) const {
        return TransformVector4(TVector4<T>{vector, T(0)});
    }
};

inline Matrix4f MakeLookAt(const Vector3f& eye, const Vector3f& target, const Vector3f& up) {
    const Vector3f forward = (target - eye).GetNormalized();
    const Vector3f right = Cross(forward, up).GetNormalized();
    const Vector3f cameraUp = Cross(right, forward);

    return Matrix4f{right.X,
                    right.Y,
                    right.Z,
                    -Dot(right, eye),
                    cameraUp.X,
                    cameraUp.Y,
                    cameraUp.Z,
                    -Dot(cameraUp, eye),
                    -forward.X,
                    -forward.Y,
                    -forward.Z,
                    Dot(forward, eye),
                    0.0f,
                    0.0f,
                    0.0f,
                    1.0f};
}

inline Matrix4f MakePerspective(float verticalFovRadians, float aspectRatio, float nearZ, float farZ,
                                DepthConvention convention = kDefaultDepthConvention) {
    const float tanHalfFov = std::tan(verticalFovRadians * 0.5f);
    const float yScale = 1.0f / tanHalfFov;
    const float xScale = yScale / aspectRatio;

    if (convention == DepthConvention::ReverseZZeroToOne) {
        const float zScale = nearZ / (farZ - nearZ);
        const float zOffset = (farZ * nearZ) / (farZ - nearZ);
        return Matrix4f{xScale, 0.0f,   0.0f,   0.0f,
                        0.0f,   yScale, 0.0f,   0.0f,
                        0.0f,   0.0f,   zScale,  zOffset,
                        0.0f,   0.0f,   -1.0f,  0.0f};
    }

    const float zScale = farZ / (nearZ - farZ);
    const float zOffset = (farZ * nearZ) / (nearZ - farZ);
    return Matrix4f{xScale, 0.0f,   0.0f,  0.0f,
                    0.0f,   yScale, 0.0f,  0.0f,
                    0.0f,   0.0f,   zScale, zOffset,
                    0.0f,   0.0f,   -1.0f, 0.0f};
}

inline Matrix4f MakeTranslation(const Vector3f& translation) {
    return Matrix4f{1.0f, 0.0f, 0.0f, translation.X,
                    0.0f, 1.0f, 0.0f, translation.Y,
                    0.0f, 0.0f, 1.0f, translation.Z,
                    0.0f, 0.0f, 0.0f, 1.0f};
}

inline Matrix4f MakeScale(const Vector3f& scale) {
    return Matrix4f{scale.X, 0.0f,    0.0f,    0.0f,
                    0.0f,    scale.Y, 0.0f,    0.0f,
                    0.0f,    0.0f,    scale.Z, 0.0f,
                    0.0f,    0.0f,    0.0f,    1.0f};
}

} // namespace Tumbler::Math
