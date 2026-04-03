#include <gtest/gtest.h>

#include "Core/GameSystem/Components/CTransform.h"

namespace {

constexpr float kEpsilon = 1e-4f;

void ExpectVecNear(const glm::vec3& actual, const glm::vec3& expected, float eps = kEpsilon) {
    EXPECT_NEAR(actual.x, expected.x, eps);
    EXPECT_NEAR(actual.y, expected.y, eps);
    EXPECT_NEAR(actual.z, expected.z, eps);
}

} // namespace

TEST(CTransformTests, TransformPointAppliesScaleThenTranslation) {
    CTransform transform;
    transform.SetPosition(glm::vec3(1.0f, 2.0f, 3.0f));
    transform.SetScale(glm::vec3(2.0f, 2.0f, 2.0f));

    const glm::vec3 worldPoint = transform.TransformPoint(glm::vec3(1.0f, 0.0f, 0.0f));
    ExpectVecNear(worldPoint, glm::vec3(3.0f, 2.0f, 3.0f));
}

TEST(CTransformTests, SetParentWithStayWorldKeepsWorldPosition) {
    CTransform parent;
    parent.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));

    CTransform child;
    child.SetPosition(glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::vec3 before = child.TransformPoint(glm::vec3(0.0f));

    child.SetParent(&parent, true);

    const glm::vec3 after = child.TransformPoint(glm::vec3(0.0f));
    ExpectVecNear(after, before);
}

TEST(CTransformTests, SetParentWithoutStayWorldChangesWorldByParentTransform) {
    CTransform parent;
    parent.SetPosition(glm::vec3(10.0f, 0.0f, 0.0f));

    CTransform child;
    child.SetPosition(glm::vec3(1.0f, 0.0f, 0.0f));
    child.SetParent(&parent, false);

    const glm::vec3 worldOrigin = child.TransformPoint(glm::vec3(0.0f));
    ExpectVecNear(worldOrigin, glm::vec3(11.0f, 0.0f, 0.0f));
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
