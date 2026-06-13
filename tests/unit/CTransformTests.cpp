#include <gtest/gtest.h>

#include "Core/GameSystem/Components/CTransform.h"

using namespace Tumbler::Math;

namespace {

constexpr float kEpsilon = 1e-4f;

void ExpectVecNear(const Vector3f& actual, const Vector3f& expected, float eps = kEpsilon) {
    EXPECT_NEAR(actual.X, expected.X, eps);
    EXPECT_NEAR(actual.Y, expected.Y, eps);
    EXPECT_NEAR(actual.Z, expected.Z, eps);
}

} // namespace

TEST(CTransformTests, TransformPointAppliesScaleThenTranslation) {
    CTransform transform;
    transform.SetPosition(Vector3f{1.0f, 2.0f, 3.0f});
    transform.SetScale(Vector3f{2.0f, 2.0f, 2.0f});

    const Vector3f worldPoint = transform.TransformPoint(Vector3f{1.0f, 0.0f, 0.0f});
    ExpectVecNear(worldPoint, Vector3f{3.0f, 2.0f, 3.0f});
}

TEST(CTransformTests, SetParentWithStayWorldKeepsWorldPosition) {
    CTransform parent;
    parent.SetPosition(Vector3f{10.0f, 0.0f, 0.0f});

    CTransform child;
    child.SetPosition(Vector3f{1.0f, 0.0f, 0.0f});
    const Vector3f before = child.TransformPoint(Vector3f{0.0f});

    child.SetParent(&parent, true);

    const Vector3f after = child.TransformPoint(Vector3f{0.0f});
    ExpectVecNear(after, before);
}

TEST(CTransformTests, SetParentWithoutStayWorldChangesWorldByParentTransform) {
    CTransform parent;
    parent.SetPosition(Vector3f{10.0f, 0.0f, 0.0f});

    CTransform child;
    child.SetPosition(Vector3f{1.0f, 0.0f, 0.0f});
    child.SetParent(&parent, false);

    const Vector3f worldOrigin = child.TransformPoint(Vector3f{0.0f});
    ExpectVecNear(worldOrigin, Vector3f{11.0f, 0.0f, 0.0f});
}

TEST(CTransformTests, ReparentUpdatesChildrenCollections) {
    CTransform parentA;
    CTransform parentB;
    CTransform child;

    child.SetParent(&parentA, false);
    ASSERT_EQ(child.GetParent(), &parentA);
    ASSERT_EQ(parentA.GetChildren().size(), 1u);
    ASSERT_EQ(parentA.GetChildren()[0], &child);

    child.SetParent(&parentB, false);
    ASSERT_EQ(child.GetParent(), &parentB);
    EXPECT_TRUE(parentA.GetChildren().empty());
    ASSERT_EQ(parentB.GetChildren().size(), 1u);
    EXPECT_EQ(parentB.GetChildren()[0], &child);
}

TEST(CTransformTests, TransformDirectionIgnoresTranslation) {
    CTransform transform;
    transform.SetPosition(Vector3f{100.0f, -50.0f, 25.0f});
    transform.SetRotation(Vector3f{0.0f, 90.0f, 0.0f});

    const Vector3f dir = transform.TransformDirection(Vector3f{0.0f, 0.0f, 1.0f}).GetNormalized();
    EXPECT_NEAR(std::abs(dir.X), 1.0f, 1e-3f);
    EXPECT_NEAR(dir.Y, 0.0f, 1e-3f);
    EXPECT_NEAR(std::abs(dir.Z), 0.0f, 1e-3f);
}

TEST(CTransformTests, ChildWorldPositionUpdatesWhenParentMoves) {
    CTransform parent;
    CTransform child;
    child.SetPosition(Vector3f{1.0f, 0.0f, 0.0f});
    child.SetParent(&parent, false);

    parent.SetPosition(Vector3f{5.0f, 0.0f, 0.0f});
    const Vector3f world = child.TransformPoint(Vector3f{0.0f});
    ExpectVecNear(world, Vector3f{6.0f, 0.0f, 0.0f});
}
