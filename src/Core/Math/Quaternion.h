#pragma once

#include "Core/Math/MathFwd.h"
#include "Core/Math/MathUtility.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Vector.h"

#include <cmath>
#include <concepts>

namespace Tumbler::Math {

// 单位四元数表示旋转, 存储顺序 [X, Y, Z, W] (Unity/UE 风格, 非 GLM 的 [W,X,Y,Z])
// 旋转公式: v' = q * v * q⁻¹, 用 RotateVector() 一步完成 (等价于 q * v)
// 乘法 (operator*): 四元数合成, q1 * q2 表示先应用 q2 再 q1
// Slerp: 球面线性插值, 沿 4D 单位球大圆弧匀速过渡
template <typename T>
    requires std::floating_point<T>
struct TQuaternion {

    T X{};
    T Y{};
    T Z{};
    T W{T(1)};

    constexpr TQuaternion() = default;
    constexpr TQuaternion(T x, T y, T z, T w) : X(x), Y(y), Z(z), W(w) {}

    [[nodiscard]] static constexpr TQuaternion Identity() { return TQuaternion{T(0), T(0), T(0), T(1)}; }

    static TQuaternion FromAxisAngle(const TVector3<T>& axis, T angleRadians) {
        const T halfAngle = angleRadians * T(0.5);
        const T sinHalf = std::sin(halfAngle);
        const TVector3<T> normAxis = axis.GetNormalized();
        return TQuaternion{normAxis.X * sinHalf, normAxis.Y * sinHalf, normAxis.Z * sinHalf, std::cos(halfAngle)};
    }

    static TQuaternion FromEulerRadians(const TVector3<T>& eulerRadians) {
        const T halfX = eulerRadians.X * T(0.5);
        const T halfY = eulerRadians.Y * T(0.5);
        const T halfZ = eulerRadians.Z * T(0.5);

        const T cx = std::cos(halfX);
        const T sx = std::sin(halfX);
        const T cy = std::cos(halfY);
        const T sy = std::sin(halfY);
        const T cz = std::cos(halfZ);
        const T sz = std::sin(halfZ);

        return TQuaternion{
            sx * cy * cz - cx * sy * sz,
            cx * sy * cz + sx * cy * sz,
            cx * cy * sz - sx * sy * cz,
            cx * cy * cz + sx * sy * sz,
        };
    }

    static TQuaternion FromEulerDegrees(const TVector3<T>& eulerDegrees) {
        return FromEulerRadians(TVector3<T>{DegreesToRadians(eulerDegrees.X), DegreesToRadians(eulerDegrees.Y),
                                            DegreesToRadians(eulerDegrees.Z)});
    }

    [[nodiscard]] T LengthSquared() const { return X * X + Y * Y + Z * Z + W * W; }
    [[nodiscard]] T Length() const { return std::sqrt(LengthSquared()); }

    bool Normalize(T tolerance = static_cast<T>(SmallNumber)) {
        const T lenSq = LengthSquared();
        if (lenSq <= tolerance * tolerance) {
            X = T(0);
            Y = T(0);
            Z = T(0);
            W = T(1);
            return false;
        }
        const T invLength = T(1) / std::sqrt(lenSq);
        X *= invLength;
        Y *= invLength;
        Z *= invLength;
        W *= invLength;
        return true;
    }

    [[nodiscard]] TQuaternion GetNormalized(T tolerance = static_cast<T>(SmallNumber)) const {
        TQuaternion result = *this;
        result.Normalize(tolerance);
        return result;
    }

    [[nodiscard]] TQuaternion Conjugate() const { return TQuaternion{-X, -Y, -Z, W}; }

    [[nodiscard]] TQuaternion Inverse() const {
        const T lenSq = LengthSquared();
        if (lenSq <= static_cast<T>(SmallNumber)) {
            return TQuaternion::Identity();
        }
        const TQuaternion conj = Conjugate();
        const T invLenSq = T(1) / lenSq;
        return TQuaternion{conj.X * invLenSq, conj.Y * invLenSq, conj.Z * invLenSq, conj.W * invLenSq};
    }

    [[nodiscard]] TVector3<T> RotateVector(const TVector3<T>& v) const {
        const TVector3<T> qv{X, Y, Z};
        const TVector3<T> t = T(2) * Cross(qv, v);
        return v + W * t + Cross(qv, t);
    }

    [[nodiscard]] TMatrix4<T> ToMatrix() const {
        const T xx = X * X;
        const T yy = Y * Y;
        const T zz = Z * Z;
        const T xy = X * Y;
        const T xz = X * Z;
        const T yz = Y * Z;
        const T wx = W * X;
        const T wy = W * Y;
        const T wz = W * Z;

        return TMatrix4<T>{
            T(1) - T(2) * (yy + zz),
            T(2) * (xy - wz),
            T(2) * (xz + wy),
            T(0),
            T(2) * (xy + wz),
            T(1) - T(2) * (xx + zz),
            T(2) * (yz - wx),
            T(0),
            T(2) * (xz - wy),
            T(2) * (yz + wx),
            T(1) - T(2) * (xx + yy),
            T(0),
            T(0),
            T(0),
            T(0),
            T(1),
        };
    }

    [[nodiscard]] TVector3<T> ToEulerRadians() const {
        const T sinRCosP = T(2) * (W * X + Y * Z);
        const T cosRCosP = T(1) - T(2) * (X * X + Y * Y);
        const T roll = std::atan2(sinRCosP, cosRCosP);

        const T sinP = T(2) * (W * Y - Z * X);
        T pitch;
        if (std::abs(sinP) >= T(1)) {
            pitch = std::copysign(Pi / T(2), sinP);
        } else {
            pitch = std::asin(sinP);
        }

        const T sinYCosP = T(2) * (W * Z + X * Y);
        const T cosYCosP = T(1) - T(2) * (Y * Y + Z * Z);
        const T yaw = std::atan2(sinYCosP, cosYCosP);

        return TVector3<T>{roll, pitch, yaw};
    }

    [[nodiscard]] TVector3<T> ToEulerDegrees() const {
        const TVector3<T> radians = ToEulerRadians();
        return TVector3<T>{RadiansToDegrees(radians.X), RadiansToDegrees(radians.Y), RadiansToDegrees(radians.Z)};
    }

    [[nodiscard]] TVector3<T> GetForwardVector() const { return RotateVector(TVector3<T>::UnitZ()); }
    [[nodiscard]] TVector3<T> GetRightVector() const { return RotateVector(TVector3<T>::UnitX()); }
    [[nodiscard]] TVector3<T> GetUpVector() const { return RotateVector(TVector3<T>::UnitY()); }

    static TQuaternion Slerp(const TQuaternion& a, const TQuaternion& b, T alpha) {
        T cosOmega = a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;

        TQuaternion bAdj = b;
        if (cosOmega < T(0)) {
            cosOmega = -cosOmega;
            bAdj = TQuaternion{-b.X, -b.Y, -b.Z, -b.W};
        }

        T scale0, scale1;
        if (T(1) - cosOmega > static_cast<T>(SmallNumber)) {
            const T omega = std::acos(cosOmega);
            const T sinOmega = std::sin(omega);
            scale0 = std::sin((T(1) - alpha) * omega) / sinOmega;
            scale1 = std::sin(alpha * omega) / sinOmega;
        } else {
            scale0 = T(1) - alpha;
            scale1 = alpha;
        }

        return TQuaternion{
            scale0 * a.X + scale1 * bAdj.X,
            scale0 * a.Y + scale1 * bAdj.Y,
            scale0 * a.Z + scale1 * bAdj.Z,
            scale0 * a.W + scale1 * bAdj.W,
        };
    }

    // ==========================================
    // 运算符
    // ==========================================

    [[nodiscard]] TQuaternion operator*(const TQuaternion& other) const {
        return TQuaternion{
            W * other.X + X * other.W + Y * other.Z - Z * other.Y,
            W * other.Y - X * other.Z + Y * other.W + Z * other.X,
            W * other.Z + X * other.Y - Y * other.X + Z * other.W,
            W * other.W - X * other.X - Y * other.Y - Z * other.Z,
        };
    }

    [[nodiscard]] TVector3<T> operator*(const TVector3<T>& v) const { return RotateVector(v); }
};

// ==========================================
// 矩阵 — 四元数互操作
// ==========================================

template <typename T> TMatrix4<T> MakeRotation(const TQuaternion<T>& q) {
    return q.ToMatrix();
}

template <typename T>
bool Decompose(const TMatrix4<T>& matrix, TVector3<T>& outTranslation, TQuaternion<T>& outRotation,
               TVector3<T>& outScale) {
    outTranslation = TVector3<T>{matrix[0][3], matrix[1][3], matrix[2][3]};

    TVector3<T> col0{matrix[0][0], matrix[1][0], matrix[2][0]};
    TVector3<T> col1{matrix[0][1], matrix[1][1], matrix[2][1]};
    TVector3<T> col2{matrix[0][2], matrix[1][2], matrix[2][2]};

    outScale = TVector3<T>{col0.Length(), col1.Length(), col2.Length()};

    if (IsNearlyZero(outScale.X) || IsNearlyZero(outScale.Y) || IsNearlyZero(outScale.Z)) {
        outRotation = TQuaternion<T>::Identity();
        return false;
    }

    col0 /= outScale.X;
    col1 /= outScale.Y;
    col2 /= outScale.Z;

    const T trace = col0.X + col1.Y + col2.Z;

    if (trace > T(0)) {
        T s = std::sqrt(trace + T(1)) * T(2);
        outRotation = TQuaternion<T>{
            (col1.Z - col2.Y) / s,
            (col2.X - col0.Z) / s,
            (col0.Y - col1.X) / s,
            s / T(4),
        };
    } else if (col0.X > col1.Y && col0.X > col2.Z) {
        T s = std::sqrt(T(1) + col0.X - col1.Y - col2.Z) * T(2);
        outRotation = TQuaternion<T>{
            s / T(4),
            (col0.Y + col1.X) / s,
            (col2.X + col0.Z) / s,
            (col1.Z - col2.Y) / s,
        };
    } else if (col1.Y > col2.Z) {
        T s = std::sqrt(T(1) + col1.Y - col0.X - col2.Z) * T(2);
        outRotation = TQuaternion<T>{
            (col0.Y + col1.X) / s,
            s / T(4),
            (col1.Z + col2.Y) / s,
            (col2.X - col0.Z) / s,
        };
    } else {
        T s = std::sqrt(T(1) + col2.Z - col0.X - col1.Y) * T(2);
        outRotation = TQuaternion<T>{
            (col2.X + col0.Z) / s,
            (col1.Z + col2.Y) / s,
            s / T(4),
            (col0.Y - col1.X) / s,
        };
    }

    outRotation.Normalize();
    return true;
}

} // namespace Tumbler::Math
