#pragma once

#include "Core/Math/MathConfig.h"
#include "Core/Math/MathFwd.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Vector4.h"

#include <cmath>
#include <concepts>

namespace Tumbler::Math {

template <typename T> requires std::floating_point<T> struct TMatrix4 {

    T M[4][4]{};

    constexpr TMatrix4() = default;

    constexpr TMatrix4(T m00, T m01, T m02, T m03, T m10, T m11, T m12, T m13, T m20, T m21, T m22, T m23, T m30, T m31,
                       T m32, T m33)
        : M{{m00, m01, m02, m03}, {m10, m11, m12, m13}, {m20, m21, m22, m23}, {m30, m31, m32, m33}} {}

    [[nodiscard]] static constexpr TMatrix4 Identity() {
        return TMatrix4{T(1), T(0), T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(0), T(1)};
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

    return Matrix4f{
        right.X,    right.Y,    right.Z,    -Dot(right, eye),  cameraUp.X, cameraUp.Y, cameraUp.Z, -Dot(cameraUp, eye),
        -forward.X, -forward.Y, -forward.Z, Dot(forward, eye), 0.0f,       0.0f,       0.0f,       1.0f};
}

inline Matrix4f MakePerspective(float verticalFovRadians, float aspectRatio, float nearZ, float farZ,
                                DepthConvention convention = kDefaultDepthConvention) {
    const float tanHalfFov = std::tan(verticalFovRadians * 0.5f);
    const float yScale = 1.0f / tanHalfFov;
    const float xScale = yScale / aspectRatio;

    if (convention == DepthConvention::ReverseZZeroToOne) {
        const float zScale = nearZ / (farZ - nearZ);
        const float zOffset = (farZ * nearZ) / (farZ - nearZ);
        return Matrix4f{xScale, 0.0f, 0.0f,   0.0f,    0.0f, yScale, 0.0f,  0.0f,
                        0.0f,   0.0f, zScale, zOffset, 0.0f, 0.0f,   -1.0f, 0.0f};
    }

    const float zScale = farZ / (nearZ - farZ);
    const float zOffset = (farZ * nearZ) / (nearZ - farZ);
    return Matrix4f{xScale, 0.0f, 0.0f,   0.0f,    0.0f, yScale, 0.0f,  0.0f,
                    0.0f,   0.0f, zScale, zOffset, 0.0f, 0.0f,   -1.0f, 0.0f};
}

inline Matrix4f MakeTranslation(const Vector3f& translation) {
    return Matrix4f{1.0f, 0.0f, 0.0f, translation.X, 0.0f, 1.0f, 0.0f, translation.Y,
                    0.0f, 0.0f, 1.0f, translation.Z, 0.0f, 0.0f, 0.0f, 1.0f};
}

inline Matrix4f MakeScale(const Vector3f& scale) {
    return Matrix4f{scale.X, 0.0f, 0.0f,    0.0f, 0.0f, scale.Y, 0.0f, 0.0f,
                    0.0f,    0.0f, scale.Z, 0.0f, 0.0f, 0.0f,    0.0f, 1.0f};
}

inline Matrix4f MakeOrtho(float left, float right, float bottom, float top, float nearZ, float farZ,
                          DepthConvention convention = kDefaultDepthConvention) {
    const float rl = 1.0f / (right - left);
    const float tb = 1.0f / (top - bottom);

    if (convention == DepthConvention::ReverseZZeroToOne) {
        const float fn = 1.0f / (nearZ - farZ);
        return Matrix4f{2.0f * rl, 0.0f, 0.0f, -(right + left) * rl, 0.0f, 2.0f * tb, 0.0f, -(top + bottom) * tb,
                        0.0f,      0.0f, fn,   nearZ * fn,           0.0f, 0.0f,      0.0f, 1.0f};
    }

    const float fn = 1.0f / (farZ - nearZ);
    return Matrix4f{2.0f * rl, 0.0f, 0.0f, -(right + left) * rl, 0.0f, 2.0f * tb, 0.0f, -(top + bottom) * tb,
                    0.0f,      0.0f, -fn,  -nearZ * fn,          0.0f, 0.0f,      0.0f, 1.0f};
}

inline Matrix4f Inverse(const Matrix4f& m) {
    const float m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
    const float m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
    const float m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
    const float m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

    const float s0 = m00 * m11 - m10 * m01;
    const float s1 = m00 * m12 - m10 * m02;
    const float s2 = m00 * m13 - m10 * m03;
    const float s3 = m01 * m12 - m11 * m02;
    const float s4 = m01 * m13 - m11 * m03;
    const float s5 = m02 * m13 - m12 * m03;

    const float c0 = m20 * m31 - m30 * m21;
    const float c1 = m20 * m32 - m30 * m22;
    const float c2 = m20 * m33 - m30 * m23;
    const float c3 = m21 * m32 - m31 * m22;
    const float c4 = m21 * m33 - m31 * m23;
    const float c5 = m22 * m33 - m32 * m23;

    const float det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
    if (IsNearlyZero(det)) {
        return Matrix4f::Identity();
    }

    const float invDet = 1.0f / det;
    return Matrix4f{
        (m11 * c5 - m12 * c4 + m13 * c3) * invDet,  (-m01 * c5 + m02 * c4 - m03 * c3) * invDet,
        (m31 * s5 - m32 * s4 + m33 * s3) * invDet,  (-m21 * s5 + m22 * s4 - m23 * s3) * invDet,

        (-m10 * c5 + m12 * c2 - m13 * c1) * invDet, (m00 * c5 - m02 * c2 + m03 * c1) * invDet,
        (-m30 * s5 + m32 * s2 - m33 * s1) * invDet, (m20 * s5 - m22 * s2 + m23 * s1) * invDet,

        (m10 * c4 - m11 * c2 + m13 * c0) * invDet,  (-m00 * c4 + m01 * c2 - m03 * c0) * invDet,
        (m30 * s4 - m31 * s2 + m33 * s0) * invDet,  (-m20 * s4 + m21 * s2 - m23 * s0) * invDet,

        (-m10 * c3 + m11 * c1 - m12 * c0) * invDet, (m00 * c3 - m01 * c1 + m02 * c0) * invDet,
        (-m30 * s3 + m31 * s1 - m32 * s0) * invDet, (m20 * s3 - m21 * s1 + m22 * s0) * invDet,
    };
}

} // namespace Tumbler::Math
