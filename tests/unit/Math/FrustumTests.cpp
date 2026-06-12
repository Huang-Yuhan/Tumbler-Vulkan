#include "Core/Math/Math.h"

#include <gtest/gtest.h>

using namespace Tumbler::Math;

namespace {

constexpr float kTolerance = 1.0e-5f;

Matrix4f MakeTestViewProj(DepthConvention convention) {
    const Matrix4f view =
        MakeLookAt(Vector3f{0.0f, 0.0f, 5.0f}, Vector3f::Zero(), Vector3f::UnitY());
    const Matrix4f projection = MakePerspective(DegreesToRadians(90.0f), 1.0f, 0.1f, 100.0f, convention);
    return projection * view;
}

void ExpectAllPlanesNormalized(const Frustum& frustum) {
    for (const Planef& plane : frustum.Planes) {
        EXPECT_NEAR(plane.Normal().Length(), 1.0f, kTolerance);
    }
}

} // namespace

TEST(MathFrustum, ExtractsVulkanZeroToOneFrustumPlanesInStableOrder) {
    Frustum frustum;
    ASSERT_TRUE(ExtractFrustumPlanes(MakeTestViewProj(DepthConvention::VulkanZeroToOne), frustum,
                                     DepthConvention::VulkanZeroToOne));

    ExpectAllPlanesNormalized(frustum);
    EXPECT_GE(frustum[FrustumPlane::Left].SignedDistance(Vector3f::Zero()), 0.0f);
    EXPECT_GE(frustum[FrustumPlane::Right].SignedDistance(Vector3f::Zero()), 0.0f);
    EXPECT_GE(frustum[FrustumPlane::Bottom].SignedDistance(Vector3f::Zero()), 0.0f);
    EXPECT_GE(frustum[FrustumPlane::Top].SignedDistance(Vector3f::Zero()), 0.0f);
    EXPECT_GE(frustum[FrustumPlane::Near].SignedDistance(Vector3f::Zero()), 0.0f);
    EXPECT_GE(frustum[FrustumPlane::Far].SignedDistance(Vector3f::Zero()), 0.0f);

    EXPECT_LT(frustum[FrustumPlane::Right].SignedDistance(Vector3f{10.0f, 0.0f, 0.0f}), 0.0f);
    EXPECT_LT(frustum[FrustumPlane::Top].SignedDistance(Vector3f{0.0f, 10.0f, 0.0f}), 0.0f);
    EXPECT_LT(frustum[FrustumPlane::Near].SignedDistance(Vector3f{0.0f, 0.0f, 10.0f}), 0.0f);
    EXPECT_LT(frustum[FrustumPlane::Far].SignedDistance(Vector3f{0.0f, 0.0f, -200.0f}), 0.0f);
}

TEST(MathFrustum, ExtractsReverseZZeroToOneFrustumPlanesInStableOrder) {
    Frustum frustum;
    ASSERT_TRUE(ExtractFrustumPlanes(MakeTestViewProj(DepthConvention::ReverseZZeroToOne), frustum,
                                     DepthConvention::ReverseZZeroToOne));

    ExpectAllPlanesNormalized(frustum);
    EXPECT_GE(frustum[FrustumPlane::Near].SignedDistance(Vector3f::Zero()), 0.0f);
    EXPECT_GE(frustum[FrustumPlane::Far].SignedDistance(Vector3f::Zero()), 0.0f);
    EXPECT_LT(frustum[FrustumPlane::Near].SignedDistance(Vector3f{0.0f, 0.0f, 10.0f}), 0.0f);
    EXPECT_LT(frustum[FrustumPlane::Far].SignedDistance(Vector3f{0.0f, 0.0f, -200.0f}), 0.0f);
}

TEST(MathFrustum, TestsSpheresAgainstPlanes) {
    Frustum frustum;
    ASSERT_TRUE(ExtractFrustumPlanes(MakeTestViewProj(DepthConvention::VulkanZeroToOne), frustum));

    EXPECT_EQ(frustum.TestSphere(Vector3f{0.0f, 0.0f, 0.0f}, 1.0f), FrustumIntersection::Inside);
    EXPECT_EQ(frustum.TestSphere(Vector3f{10.0f, 0.0f, 0.0f}, 0.5f), FrustumIntersection::Outside);
    EXPECT_EQ(frustum.TestSphere(Vector3f{5.0f, 0.0f, 0.0f}, 1.0f), FrustumIntersection::Intersects);
}

TEST(MathFrustum, RejectsDegenerateViewProjectionMatrices) {
    Frustum frustum;
    EXPECT_FALSE(ExtractFrustumPlanes(Matrix4f{}, frustum));
}

