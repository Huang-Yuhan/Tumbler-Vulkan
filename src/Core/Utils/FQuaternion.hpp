#pragma once

// FQuaternion is now a thin wrapper around Tumbler::Math::Quaternionf.
// This file exists for backward compatibility; new code should include
// Core/Math/Quaternion.h and use Tumbler::Math::Quaternionf directly.

#include "Core/Math/Quaternion.h"
#include <glm/glm.hpp>

struct FQuaternion
{
    using MathQuat = Tumbler::Math::Quaternionf;
    using MathVec3 = Tumbler::Math::Vector3f;
    using MathMat4 = Tumbler::Math::Matrix4f;

    MathQuat Raw;

    FQuaternion() : Raw(MathQuat::Identity()) {}

    FQuaternion(const MathQuat& q) : Raw(q) {}

    explicit FQuaternion(const MathVec3& eulerDegrees)
        : Raw(MathQuat::FromEulerDegrees(eulerDegrees)) {}

    // glm vec3 constructor for backward compat — converts to MathVec3
    explicit FQuaternion(const glm::vec3& eulerDegrees)
        : Raw(MathQuat::FromEulerDegrees(MathVec3{eulerDegrees.x, eulerDegrees.y, eulerDegrees.z})) {}

    static FQuaternion FromAxisAngle(const MathVec3& axis, float angleDegrees) {
        return FQuaternion(MathQuat::FromAxisAngle(axis, Tumbler::Math::DegreesToRadians(angleDegrees)));
    }

    MathMat4 ToMatrix() const { return Raw.ToMatrix(); }

    // glm compat
    glm::mat4 ToGlmMatrix() const {
        const MathMat4 m = Raw.ToMatrix();
        return glm::mat4(
            m[0][0], m[0][1], m[0][2], m[0][3],
            m[1][0], m[1][1], m[1][2], m[1][3],
            m[2][0], m[2][1], m[2][2], m[2][3],
            m[3][0], m[3][1], m[3][2], m[3][3]
        );
    }

    MathVec3 ToEuler() const { return Raw.ToEulerDegrees(); }

    void Normalize() { Raw.Normalize(); }

    static FQuaternion Slerp(const FQuaternion& a, const FQuaternion& b, float alpha) {
        return FQuaternion(MathQuat::Slerp(a.Raw, b.Raw, alpha));
    }

    MathVec3 GetForwardVector() const { return Raw.GetForwardVector(); }
    MathVec3 GetRightVector() const { return Raw.GetRightVector(); }
    MathVec3 GetUpVector() const { return Raw.GetUpVector(); }

    FQuaternion operator*(const FQuaternion& other) const {
        return FQuaternion(Raw * other.Raw);
    }

    MathVec3 operator*(const MathVec3& v) const { return Raw.RotateVector(v); }
};
