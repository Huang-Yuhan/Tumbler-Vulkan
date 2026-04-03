#include <gtest/gtest.h>

#include <glm/geometric.hpp>

#include "Core/Utils/FQuaternion.hpp"

namespace {

constexpr float kEpsilon = 1e-4f;

void ExpectVecNear(const glm::vec3& actual, const glm::vec3& expected, float eps = kEpsilon) {
    EXPECT_NEAR(actual.x, expected.x, eps);
    EXPECT_NEAR(actual.y, expected.y, eps);
    EXPECT_NEAR(actual.z, expected.z, eps);
}

} // namespace

TEST(FQuaternionTests, DefaultQuaternionHasCanonicalBasisVectors) {
    FQuaternion q;

    ExpectVecNear(q.GetForwardVector(), glm::vec3(0.0f, 0.0f, 1.0f));
    ExpectVecNear(q.GetRightVector(), glm::vec3(1.0f, 0.0f, 0.0f));
    ExpectVecNear(q.GetUpVector(), glm::vec3(0.0f, 1.0f, 0.0f));
}

TEST(FQuaternionTests, FromAxisAngleProducesNormalizedQuaternion) {
    const FQuaternion q = FQuaternion::FromAxisAngle(glm::vec3(0.0f, 3.0f, 0.0f), 45.0f);

    EXPECT_NEAR(glm::length(q.Raw), 1.0f, kEpsilon);
}

TEST(FQuaternionTests, SlerpReturnsMidRotationForHalfAlpha) {
    const FQuaternion start(glm::vec3(0.0f, 0.0f, 0.0f));
    const FQuaternion end(glm::vec3(0.0f, 180.0f, 0.0f));
    const FQuaternion mid = FQuaternion::Slerp(start, end, 0.5f);

    const glm::vec3 forward = glm::normalize(mid.GetForwardVector());
    EXPECT_NEAR(glm::abs(forward.x), 1.0f, 1e-3f);
    EXPECT_NEAR(forward.y, 0.0f, 1e-3f);
    EXPECT_NEAR(glm::abs(forward.z), 0.0f, 1e-3f);
}
