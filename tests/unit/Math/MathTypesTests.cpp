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

} // namespace

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
