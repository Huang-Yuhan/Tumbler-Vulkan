#include "Core/Math/Math.h"

#include <cmath>
#include <gtest/gtest.h>

using namespace Tumbler::Math;

namespace {

constexpr float kTolerance = 1.0e-5f;

void ExpectVectorNear(const Vector3f& actual, const Vector3f& expected, float tolerance = kTolerance) {
    EXPECT_NEAR(actual.X, expected.X, tolerance);
    EXPECT_NEAR(actual.Y, expected.Y, tolerance);
    EXPECT_NEAR(actual.Z, expected.Z, tolerance);
}

void ExpectVectorNear(const Vector4f& actual, const Vector4f& expected, float tolerance = kTolerance) {
    EXPECT_NEAR(actual.X, expected.X, tolerance);
    EXPECT_NEAR(actual.Y, expected.Y, tolerance);
    EXPECT_NEAR(actual.Z, expected.Z, tolerance);
    EXPECT_NEAR(actual.W, expected.W, tolerance);
}

void ExpectVectorNear(const Vector2f& actual, const Vector2f& expected, float tolerance = kTolerance) {
    EXPECT_NEAR(actual.X, expected.X, tolerance);
    EXPECT_NEAR(actual.Y, expected.Y, tolerance);
}

void ExpectMatrixNear(const Matrix4f& a, const Matrix4f& b, float tolerance = kTolerance) {
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            EXPECT_NEAR(a[r][c], b[r][c], tolerance) << "M[" << r << "][" << c << "]";
        }
    }
}

} // namespace

// ==========================================
// Vector2
// ==========================================

TEST(MathVector2, DefaultConstructsToZero) {
    const Vector2f v;
    EXPECT_FLOAT_EQ(v.X, 0.0f);
    EXPECT_FLOAT_EQ(v.Y, 0.0f);
}

TEST(MathVector2, SupportsBasicArithmetic) {
    const Vector2f a{1.0f, 2.0f};
    const Vector2f b{3.0f, 4.0f};

    ExpectVectorNear(a + b, Vector2f{4.0f, 6.0f});
    ExpectVectorNear(a - b, Vector2f{-2.0f, -2.0f});
    ExpectVectorNear(a * 2.0f, Vector2f{2.0f, 4.0f});
    ExpectVectorNear(2.0f * a, Vector2f{2.0f, 4.0f});
    ExpectVectorNear(a / 2.0f, Vector2f{0.5f, 1.0f});
    EXPECT_NEAR(Dot(a, b), 11.0f, kTolerance);
}

TEST(MathVector2, ComputesLengthAndNormalizes) {
    const Vector2f v{3.0f, 4.0f};
    EXPECT_NEAR(v.Length(), 5.0f, kTolerance);

    Vector2f n = v.GetNormalized();
    EXPECT_NEAR(n.Length(), 1.0f, kTolerance);

    Vector2f zero;
    EXPECT_FALSE(zero.Normalize());
    EXPECT_TRUE(zero.IsNearlyZero());
}

TEST(MathVector2, StaticFactoriesReturnCorrectValues) {
    EXPECT_TRUE(Vector2f::Zero().IsNearlyZero());
    ExpectVectorNear(Vector2f::One(), Vector2f{1.0f, 1.0f});
    ExpectVectorNear(Vector2f::UnitX(), Vector2f{1.0f, 0.0f});
    ExpectVectorNear(Vector2f::UnitY(), Vector2f{0.0f, 1.0f});
}

// ==========================================
// Quaternion
// ==========================================

TEST(MathQuaternion, DefaultConstructsToIdentity) {
    const Quaternionf q;
    EXPECT_FLOAT_EQ(q.X, 0.0f);
    EXPECT_FLOAT_EQ(q.Y, 0.0f);
    EXPECT_FLOAT_EQ(q.Z, 0.0f);
    EXPECT_FLOAT_EQ(q.W, 1.0f);
}

TEST(MathQuaternion, IdentityDoesNotRotateVector) {
    const Quaternionf q = Quaternionf::Identity();
    const Vector3f v{1.0f, 2.0f, 3.0f};
    ExpectVectorNear(q.RotateVector(v), v);
}

TEST(MathQuaternion, AxisAngleRotationIsCorrect) {
    // 90-degree rotation around Y axis
    const Quaternionf q = Quaternionf::FromAxisAngle(Vector3f::UnitY(), DegreesToRadians(90.0f));

    // Forward (Z+) should become Right (X+)
    ExpectVectorNear(q.GetForwardVector(), Vector3f::UnitX(), 1e-4f);
    // Up (Y+) should stay Up (Y+)
    ExpectVectorNear(q.GetUpVector(), Vector3f::UnitY(), 1e-4f);
    // Right (X+) should become Backward (Z-)
    ExpectVectorNear(q.GetRightVector(), -Vector3f::UnitZ(), 1e-4f);
}

TEST(MathQuaternion, AxisAngleZeroProducesIdentity) {
    const Quaternionf q = Quaternionf::FromAxisAngle(Vector3f::UnitX(), 0.0f);
    ExpectVectorNear(q.RotateVector(Vector3f::UnitZ()), Vector3f::UnitZ());
}

TEST(MathQuaternion, EulerAnglesRoundTrip) {
    const Vector3f euler{DegreesToRadians(30.0f), DegreesToRadians(45.0f), DegreesToRadians(60.0f)};
    const Quaternionf q = Quaternionf::FromEulerRadians(euler);
    const Vector3f result = q.ToEulerRadians();
    // Euler angles are not perfectly round-trip due to gimbal lock regions
    // but for reasonable angles they should be close
    EXPECT_NEAR(result.X, euler.X, 0.01f);
    EXPECT_NEAR(result.Y, euler.Y, 0.01f);
    EXPECT_NEAR(result.Z, euler.Z, 0.01f);
}

TEST(MathQuaternion, DegreeEulerMatchesRadianEuler) {
    const Quaternionf fromDeg = Quaternionf::FromEulerDegrees(Vector3f{30.0f, 45.0f, 60.0f});
    const Quaternionf fromRad = Quaternionf::FromEulerRadians(
        Vector3f{DegreesToRadians(30.0f), DegreesToRadians(45.0f), DegreesToRadians(60.0f)});
    EXPECT_NEAR(fromDeg.X, fromRad.X, 1e-5f);
    EXPECT_NEAR(fromDeg.Y, fromRad.Y, 1e-5f);
    EXPECT_NEAR(fromDeg.Z, fromRad.Z, 1e-5f);
    EXPECT_NEAR(fromDeg.W, fromRad.W, 1e-5f);
}

TEST(MathQuaternion, ToMatrixIsOrthonormal) {
    const Quaternionf q = Quaternionf::FromEulerDegrees(Vector3f{10.0f, 20.0f, 30.0f});
    const Matrix4f m = q.ToMatrix();

    // Columns should be orthonormal (rotation matrix property)
    const Vector3f col0{m[0][0], m[1][0], m[2][0]};
    const Vector3f col1{m[0][1], m[1][1], m[2][1]};
    const Vector3f col2{m[0][2], m[1][2], m[2][2]};

    EXPECT_NEAR(col0.Length(), 1.0f, 1e-4f);
    EXPECT_NEAR(col1.Length(), 1.0f, 1e-4f);
    EXPECT_NEAR(col2.Length(), 1.0f, 1e-4f);
    EXPECT_NEAR(Dot(col0, col1), 0.0f, 1e-4f);
    EXPECT_NEAR(Dot(col1, col2), 0.0f, 1e-4f);
    EXPECT_NEAR(Dot(col0, col2), 0.0f, 1e-4f);

    // Bottom row should be (0, 0, 0, 1)
    EXPECT_NEAR(m[3][0], 0.0f, 1e-6f);
    EXPECT_NEAR(m[3][1], 0.0f, 1e-6f);
    EXPECT_NEAR(m[3][2], 0.0f, 1e-6f);
    EXPECT_NEAR(m[3][3], 1.0f, 1e-6f);
}

TEST(MathQuaternion, SlerpAtZeroReturnsA) {
    const Quaternionf a = Quaternionf::FromEulerDegrees(Vector3f{0.0f, 90.0f, 0.0f});
    const Quaternionf b = Quaternionf::Identity();
    const Quaternionf result = Quaternionf::Slerp(a, b, 0.0f);
    EXPECT_NEAR(result.X, a.X, 1e-4f);
    EXPECT_NEAR(result.Y, a.Y, 1e-4f);
    EXPECT_NEAR(result.Z, a.Z, 1e-4f);
    EXPECT_NEAR(result.W, a.W, 1e-4f);
}

TEST(MathQuaternion, SlerpAtOneReturnsB) {
    const Quaternionf a = Quaternionf::Identity();
    const Quaternionf b = Quaternionf::FromEulerDegrees(Vector3f{0.0f, 90.0f, 0.0f});
    const Quaternionf result = Quaternionf::Slerp(a, b, 1.0f);
    EXPECT_NEAR(result.X, b.X, 1e-4f);
    EXPECT_NEAR(result.Y, b.Y, 1e-4f);
    EXPECT_NEAR(result.Z, b.Z, 1e-4f);
    EXPECT_NEAR(result.W, b.W, 1e-4f);
}

TEST(MathQuaternion, SlerpHalfwayIsHalfAngle) {
    const Quaternionf a = Quaternionf::Identity();
    const Quaternionf b = Quaternionf::FromAxisAngle(Vector3f::UnitY(), DegreesToRadians(90.0f));
    const Quaternionf mid = Quaternionf::Slerp(a, b, 0.5f);

    // Half of 90 degrees around Y should give ~45 degrees around Y
    const Vector3f euler = mid.ToEulerDegrees();
    EXPECT_NEAR(euler.Y, 45.0f, 0.1f);
}

TEST(MathQuaternion, NormalizePreservesRotation) {
    Quaternionf q{1.0f, 2.0f, 3.0f, 4.0f}; // non-unit quaternion
    q.Normalize();
    EXPECT_NEAR(q.Length(), 1.0f, 1e-5f);
}

TEST(MathQuaternion, MultiplyCombinesRotations) {
    // Two 90-degree Y rotations = 180-degree Y rotation
    const Quaternionf rot90Y = Quaternionf::FromAxisAngle(Vector3f::UnitY(), DegreesToRadians(90.0f));
    const Quaternionf rot180Y = rot90Y * rot90Y;

    // Forward should become backward
    ExpectVectorNear(rot180Y.GetForwardVector(), -Vector3f::UnitZ(), 1e-4f);
}

TEST(MathQuaternion, ConjugateAndInverse) {
    const Quaternionf q = Quaternionf::FromEulerDegrees(Vector3f{30.0f, 45.0f, 60.0f});
    const Quaternionf conj = q.Conjugate();
    const Quaternionf inv = q.Inverse();

    // q * q^-1 = identity
    const Quaternionf result = q * inv;
    EXPECT_NEAR(result.X, 0.0f, 1e-4f);
    EXPECT_NEAR(result.Y, 0.0f, 1e-4f);
    EXPECT_NEAR(result.Z, 0.0f, 1e-4f);
    EXPECT_NEAR(result.W, 1.0f, 1e-4f);
}

// ==========================================
// Matrix extensions
// ==========================================

TEST(MathMatrix, MakeTranslationIsCorrect) {
    const Vector3f pos{1.0f, 2.0f, 3.0f};
    const Matrix4f t = MakeTranslation(pos);

    ExpectVectorNear(t.TransformPosition(Vector3f::Zero()), Vector4f{1.0f, 2.0f, 3.0f, 1.0f});
    ExpectVectorNear(t.TransformVector(Vector3f::UnitX()), Vector4f{1.0f, 0.0f, 0.0f, 0.0f});
}

TEST(MathMatrix, MakeScaleIsCorrect) {
    const Vector3f scale{2.0f, 3.0f, 4.0f};
    const Matrix4f s = MakeScale(scale);

    ExpectVectorNear(s.TransformPosition(Vector3f{1.0f, 1.0f, 1.0f}), Vector4f{2.0f, 3.0f, 4.0f, 1.0f});
    ExpectVectorNear(s.TransformVector(Vector3f{1.0f, 1.0f, 1.0f}), Vector4f{2.0f, 3.0f, 4.0f, 0.0f});
}

TEST(MathMatrix, MakeRotationFromQuaternion) {
    const Quaternionf q = Quaternionf::FromAxisAngle(Vector3f::UnitZ(), DegreesToRadians(90.0f));
    const Matrix4f r = MakeRotation(q);

    // Should match quaternion.ToMatrix()
    ExpectMatrixNear(r, q.ToMatrix());
}

TEST(MathMatrix, DecomposeRoundTrip_TRS) {
    // Build a TRS matrix
    const Vector3f translation{10.0f, 20.0f, 30.0f};
    const Quaternionf rotation = Quaternionf::FromEulerDegrees(Vector3f{15.0f, 30.0f, 45.0f});
    const Vector3f scale{1.0f, 2.0f, 3.0f};

    const Matrix4f t = MakeTranslation(translation);
    const Matrix4f r = MakeRotation(rotation);
    const Matrix4f s = MakeScale(scale);

    const Matrix4f trs = t * r * s;

    // Decompose
    Vector3f outTrans, outScale;
    Quaternionf outRot;
    EXPECT_TRUE(Decompose(trs, outTrans, outRot, outScale));

    ExpectVectorNear(outTrans, translation);
    ExpectVectorNear(outScale, scale);
    // Quaternion comparison: check they rotate the same way
    ExpectVectorNear(outRot.RotateVector(Vector3f::UnitX()), rotation.RotateVector(Vector3f::UnitX()), 1e-4f);
    ExpectVectorNear(outRot.RotateVector(Vector3f::UnitY()), rotation.RotateVector(Vector3f::UnitY()), 1e-4f);
    ExpectVectorNear(outRot.RotateVector(Vector3f::UnitZ()), rotation.RotateVector(Vector3f::UnitZ()), 1e-4f);
}

TEST(MathMatrix, DecomposeIdentityMatrix) {
    Vector3f trans, scale;
    Quaternionf rot;
    EXPECT_TRUE(Decompose(Matrix4f::Identity(), trans, rot, scale));

    ExpectVectorNear(trans, Vector3f::Zero());
    ExpectVectorNear(scale, Vector3f::One());
    EXPECT_NEAR(rot.X, 0.0f, 1e-5f);
    EXPECT_NEAR(rot.Y, 0.0f, 1e-5f);
    EXPECT_NEAR(rot.Z, 0.0f, 1e-5f);
    EXPECT_NEAR(rot.W, 1.0f, 1e-5f);
}

TEST(MathMatrix, DecomposeTranslationOnly) {
    const Vector3f trans{5.0f, -3.0f, 7.0f};
    const Matrix4f m = MakeTranslation(trans);

    Vector3f outTrans, outScale;
    Quaternionf outRot;
    EXPECT_TRUE(Decompose(m, outTrans, outRot, outScale));

    ExpectVectorNear(outTrans, trans);
    ExpectVectorNear(outScale, Vector3f::One());
}

TEST(MathMatrix, DecomposeScaleOnly) {
    const Vector3f s{2.0f, 0.5f, 4.0f};
    const Matrix4f m = MakeScale(s);

    Vector3f outTrans, outScale;
    Quaternionf outRot;
    EXPECT_TRUE(Decompose(m, outTrans, outRot, outScale));

    ExpectVectorNear(outTrans, Vector3f::Zero());
    ExpectVectorNear(outScale, s);
}

// ==========================================
// MathUtility additions
// ==========================================

TEST(MathUtility, LerpInterpolatesCorrectly) {
    EXPECT_FLOAT_EQ(Lerp(0.0f, 10.0f, 0.0f), 0.0f);
    EXPECT_FLOAT_EQ(Lerp(0.0f, 10.0f, 1.0f), 10.0f);
    EXPECT_FLOAT_EQ(Lerp(0.0f, 10.0f, 0.5f), 5.0f);
    EXPECT_FLOAT_EQ(Lerp(-5.0f, 5.0f, 0.5f), 0.0f);
}

TEST(MathUtility, ClampConstrainsValue) {
    EXPECT_FLOAT_EQ(Clamp(5.0f, 0.0f, 10.0f), 5.0f);
    EXPECT_FLOAT_EQ(Clamp(-1.0f, 0.0f, 10.0f), 0.0f);
    EXPECT_FLOAT_EQ(Clamp(15.0f, 0.0f, 10.0f), 10.0f);
}

TEST(MathVector, SupportsBasicVectorOperations) {
    const Vector3f a{1.0f, 2.0f, 3.0f};
    const Vector3f b{-4.0f, 5.0f, -6.0f};

    ExpectVectorNear(a + b, Vector3f{-3.0f, 7.0f, -3.0f});
    ExpectVectorNear(a - b, Vector3f{5.0f, -3.0f, 9.0f});
    ExpectVectorNear(a * 2.0f, Vector3f{2.0f, 4.0f, 6.0f});
    ExpectVectorNear(2.0f * a, Vector3f{2.0f, 4.0f, 6.0f});
    ExpectVectorNear(a / 2.0f, Vector3f{0.5f, 1.0f, 1.5f});

    EXPECT_NEAR(Dot(a, b), -12.0f, kTolerance);
    ExpectVectorNear(Cross(Vector3f::UnitX(), Vector3f::UnitY()), Vector3f::UnitZ());
    const float length = Vector3f{3.0f, 4.0f, 0.0f}.Length();
    EXPECT_NEAR(length, 5.0f, kTolerance);
    EXPECT_TRUE(Vector3f::Zero().IsNearlyZero());
}

TEST(MathVector, SafeNormalizeHandlesZeroLengthVectors) {
    Vector3f normal{0.0f, 3.0f, 4.0f};
    EXPECT_TRUE(normal.Normalize());
    ExpectVectorNear(normal, Vector3f{0.0f, 0.6f, 0.8f});

    Vector3f zero = Vector3f::Zero();
    EXPECT_FALSE(zero.Normalize());
    ExpectVectorNear(zero, Vector3f::Zero());
}

TEST(MathMatrix, BuildsLookAtAndPerspectiveMatrices) {
    const Matrix4f view =
        MakeLookAt(Vector3f{0.0f, 0.0f, 5.0f}, Vector3f::Zero(), Vector3f::UnitY());
    const Matrix4f projection =
        MakePerspective(DegreesToRadians(90.0f), 1.0f, 0.1f, 100.0f, DepthConvention::VulkanZeroToOne);
    const Matrix4f viewProj = projection * view;

    const Vector4f clipOrigin = viewProj.TransformPosition(Vector3f::Zero());
    EXPECT_GT(clipOrigin.W, 0.0f);

    const Matrix4f identity = Matrix4f::Identity();
    ExpectVectorNear(identity.TransformPosition(Vector3f{1.0f, 2.0f, 3.0f}),
                     Vector4f{1.0f, 2.0f, 3.0f, 1.0f});
    ExpectVectorNear(identity.TransformVector(Vector3f{1.0f, 2.0f, 3.0f}),
                     Vector4f{1.0f, 2.0f, 3.0f, 0.0f});
}

TEST(MathPlane, ComputesSignedDistanceAndNormalizesSafely) {
    Planef plane{0.0f, 0.0f, 2.0f, -10.0f};
    EXPECT_TRUE(plane.Normalize());

    EXPECT_NEAR(plane.Normal().Length(), 1.0f, kTolerance);
    EXPECT_NEAR(plane.SignedDistance(Vector3f{0.0f, 0.0f, 5.0f}), 0.0f, kTolerance);
    EXPECT_NEAR(plane.SignedDistance(Vector3f{0.0f, 0.0f, 6.0f}), 1.0f, kTolerance);

    Planef invalid{0.0f, 0.0f, 0.0f, 1.0f};
    EXPECT_FALSE(invalid.Normalize());
}

TEST(MathUtility, SharedDepthConventionDefaultsToVulkanZeroToOne) {
    EXPECT_EQ(kDefaultDepthConvention, DepthConvention::VulkanZeroToOne);
    EXPECT_EQ(TUMBLER_MATH_DEFAULT_DEPTH_CONVENTION, TUMBLER_DEPTH_CONVENTION_VULKAN_ZERO_TO_ONE);
}
