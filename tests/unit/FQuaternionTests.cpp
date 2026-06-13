#include <gtest/gtest.h>

#include <cmath>

#include "Core/Math/Math.h"
#include "Core/Utils/FQuaternion.hpp"

using namespace Tumbler::Math;

namespace {

constexpr float kEpsilon = 1e-4f;

void ExpectVecNear(const Vector3f& actual, const Vector3f& expected, float eps = kEpsilon) {
    EXPECT_NEAR(actual.X, expected.X, eps);
    EXPECT_NEAR(actual.Y, expected.Y, eps);
    EXPECT_NEAR(actual.Z, expected.Z, eps);
}

} // namespace

TEST(FQuaternionTests, DefaultQuaternionHasCanonicalBasisVectors) {
    FQuaternion q;

    ExpectVecNear(q.GetForwardVector(), Vector3f{0.0f, 0.0f, 1.0f});
    ExpectVecNear(q.GetRightVector(), Vector3f{1.0f, 0.0f, 0.0f});
    ExpectVecNear(q.GetUpVector(), Vector3f{0.0f, 1.0f, 0.0f});
}

TEST(FQuaternionTests, FromAxisAngleProducesNormalizedQuaternion) {
    const FQuaternion q = FQuaternion::FromAxisAngle(Vector3f{0.0f, 3.0f, 0.0f}, 45.0f);

    EXPECT_NEAR(q.Raw.Length(), 1.0f, kEpsilon);
}

TEST(FQuaternionTests, SlerpReturnsMidRotationForHalfAlpha) {
    const FQuaternion start(Quaternionf::Identity());
    const FQuaternion end(Quaternionf::FromEulerDegrees(Vector3f{0.0f, 180.0f, 0.0f}));
    const FQuaternion mid = FQuaternion::Slerp(start, end, 0.5f);

    const Vector3f forward = mid.GetForwardVector();
    EXPECT_NEAR(std::abs(forward.X), 1.0f, 1e-3f);
    EXPECT_NEAR(forward.Y, 0.0f, 1e-3f);
    EXPECT_NEAR(std::abs(forward.Z), 0.0f, 1e-3f);
}

TEST(FQuaternionTests, NormalizeReNormalizesScaledQuaternion) {
    FQuaternion q;
    q.Raw = Quaternionf{q.Raw.X * 3.0f, q.Raw.Y * 3.0f, q.Raw.Z * 3.0f, q.Raw.W * 3.0f};
    ASSERT_GT(q.Raw.Length(), 1.0f);

    q.Normalize();

    EXPECT_NEAR(q.Raw.Length(), 1.0f, kEpsilon);
}

TEST(FQuaternionTests, VectorMultiplicationRotatesDirection) {
    const FQuaternion q(Quaternionf::FromEulerDegrees(Vector3f{0.0f, 90.0f, 0.0f}));
    const Vector3f rotated = (q * Vector3f{0.0f, 0.0f, 1.0f}).GetNormalized();

    EXPECT_NEAR(std::abs(rotated.X), 1.0f, 1e-3f);
    EXPECT_NEAR(rotated.Y, 0.0f, 1e-3f);
    EXPECT_NEAR(std::abs(rotated.Z), 0.0f, 1e-3f);
}
