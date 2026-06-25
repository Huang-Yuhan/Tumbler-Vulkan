#pragma once

#include "Core/Math/MathConfig.h"
#include "Core/Math/MathFwd.h"
#include "Core/Math/Vector.h"
#include "Core/Math/Vector4.h"

#include <cmath>
#include <concepts>

namespace Tumbler::Math {

// 4x4 矩阵 — 以行主序存储 (M[row][col]), 但变换向量时按列向量惯例 (M * v)
// 矩阵元素按 16 参构造函数顺序: M00..M03 (row0), M10..M13 (row1), M20..M23 (row2), M30..M33 (row3)
// TransformPosition: v 视为 (x,y,z,1) 应用完整仿射变换 (含平移)
// TransformVector:   v 视为 (x,y,z,0) 仅应用旋转/缩放 (忽略平移分量)
template <typename T>
    requires std::floating_point<T>
struct TMatrix4 {

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

template <typename T> TMatrix4<T> MakeLookAt(const TVector3<T>& eye, const TVector3<T>& target, const TVector3<T>& up) {
    const TVector3<T> forward = (target - eye).GetNormalized();
    const TVector3<T> right = Cross(forward, up).GetNormalized();
    const TVector3<T> cameraUp = Cross(right, forward);

    return TMatrix4<T>{
        right.X,    right.Y,    right.Z,    -Dot(right, eye),  cameraUp.X, cameraUp.Y, cameraUp.Z, -Dot(cameraUp, eye),
        -forward.X, -forward.Y, -forward.Z, Dot(forward, eye), T(0),       T(0),       T(0),       T(1)};
}

template <typename T>
TMatrix4<T> MakePerspective(T verticalFovRadians, T aspectRatio, T nearZ, T farZ,
                            DepthConvention convention = kDefaultDepthConvention) {
    const T tanHalfFov = std::tan(verticalFovRadians * T(0.5));
    const T yScale = T(1) / tanHalfFov;
    const T xScale = yScale / aspectRatio;

    if (convention == DepthConvention::ReverseZZeroToOne) {
        const T zScale = nearZ / (farZ - nearZ);
        const T zOffset = (farZ * nearZ) / (farZ - nearZ);
        return TMatrix4<T>{xScale, T(0), T(0),   T(0),    T(0), yScale, T(0),  T(0),
                           T(0),   T(0), zScale, zOffset, T(0), T(0),   -T(1), T(0)};
    }

    const T zScale = farZ / (nearZ - farZ);
    const T zOffset = (farZ * nearZ) / (nearZ - farZ);
    return TMatrix4<T>{xScale, T(0), T(0),   T(0),    T(0), yScale, T(0),  T(0),
                       T(0),   T(0), zScale, zOffset, T(0), T(0),   -T(1), T(0)};
}

template <typename T> TMatrix4<T> MakeTranslation(const TVector3<T>& translation) {
    return TMatrix4<T>{T(1), T(0), T(0), translation.X, T(0), T(1), T(0), translation.Y,
                       T(0), T(0), T(1), translation.Z, T(0), T(0), T(0), T(1)};
}

template <typename T> TMatrix4<T> MakeScale(const TVector3<T>& scale) {
    return TMatrix4<T>{scale.X, T(0), T(0),    T(0), T(0), scale.Y, T(0), T(0),
                       T(0),    T(0), scale.Z, T(0), T(0), T(0),    T(0), T(1)};
}

template <typename T>
TMatrix4<T> MakeOrtho(T left, T right, T bottom, T top, T nearZ, T farZ,
                      DepthConvention convention = kDefaultDepthConvention) {
    const T rl = T(1) / (right - left);
    const T tb = T(1) / (top - bottom);

    if (convention == DepthConvention::ReverseZZeroToOne) {
        const T fn = T(1) / (nearZ - farZ);
        return TMatrix4<T>{T(2) * rl, T(0), T(0), -(right + left) * rl, T(0), T(2) * tb, T(0), -(top + bottom) * tb,
                           T(0),      T(0), fn,   nearZ * fn,           T(0), T(0),      T(0), T(1)};
    }

    const T fn = T(1) / (farZ - nearZ);
    return TMatrix4<T>{T(2) * rl, T(0), T(0), -(right + left) * rl, T(0), T(2) * tb, T(0), -(top + bottom) * tb,
                       T(0),      T(0), -fn,  -nearZ * fn,          T(0), T(0),      T(0), T(1)};
}

template <typename T> TMatrix4<T> Inverse(const TMatrix4<T>& m) {
    const T m00 = m[0][0], m01 = m[0][1], m02 = m[0][2], m03 = m[0][3];
    const T m10 = m[1][0], m11 = m[1][1], m12 = m[1][2], m13 = m[1][3];
    const T m20 = m[2][0], m21 = m[2][1], m22 = m[2][2], m23 = m[2][3];
    const T m30 = m[3][0], m31 = m[3][1], m32 = m[3][2], m33 = m[3][3];

    const T s0 = m00 * m11 - m10 * m01;
    const T s1 = m00 * m12 - m10 * m02;
    const T s2 = m00 * m13 - m10 * m03;
    const T s3 = m01 * m12 - m11 * m02;
    const T s4 = m01 * m13 - m11 * m03;
    const T s5 = m02 * m13 - m12 * m03;

    const T c0 = m20 * m31 - m30 * m21;
    const T c1 = m20 * m32 - m30 * m22;
    const T c2 = m20 * m33 - m30 * m23;
    const T c3 = m21 * m32 - m31 * m22;
    const T c4 = m21 * m33 - m31 * m23;
    const T c5 = m22 * m33 - m32 * m23;

    const T det = s0 * c5 - s1 * c4 + s2 * c3 + s3 * c2 - s4 * c1 + s5 * c0;
    if (IsNearlyZero(det)) {
        return TMatrix4<T>::Identity();
    }

    const T invDet = T(1) / det;
    return TMatrix4<T>{
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
