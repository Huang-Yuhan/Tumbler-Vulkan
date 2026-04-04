#include <gtest/gtest.h>

#include "Core/GameSystem/FScene.h"
#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/Components/Component.h"

namespace {

class TickCounterComponent final : public Component {
public:
    void Update(float DeltaTime) override {
        ++UpdateCount;
        LastDelta = DeltaTime;
    }

    int UpdateCount = 0;
    float LastDelta = 0.0f;
};

} // namespace

TEST(FSceneTests, CreateActorAndFindActorByNameWork) {
    FScene scene;

    FActor* actor = scene.CreateActor("Hero");
    ASSERT_NE(actor, nullptr);

    FActor* found = scene.FindActorByName("Hero");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found, actor);
}

TEST(FSceneTests, DestroyActorDefersRemovalUntilTick) {
    FScene scene;
    FActor* actorA = scene.CreateActor("A");
    FActor* actorB = scene.CreateActor("B");
    ASSERT_NE(actorA, nullptr);
    ASSERT_NE(actorB, nullptr);

    scene.DestroyActor(actorA);
    scene.DestroyActor(actorA); // duplicate destroy should be ignored

    EXPECT_EQ(scene.GetAllActors().size(), 2u);
    scene.Tick(0.016f);
    EXPECT_EQ(scene.GetAllActors().size(), 1u);
    EXPECT_EQ(scene.FindActorByName("A"), nullptr);
    EXPECT_NE(scene.FindActorByName("B"), nullptr);
}

TEST(FSceneTests, DestroyActorHandlesNullSafely) {
    FScene scene;
    scene.CreateActor("Solo");

    scene.DestroyActor(nullptr);
    scene.Tick(0.016f);

    EXPECT_EQ(scene.GetAllActors().size(), 1u);
}

TEST(FSceneTests, TickCallsComponentUpdateOnAllActors) {
    FScene scene;

    FActor* a = scene.CreateActor("A");
    FActor* b = scene.CreateActor("B");
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);

    TickCounterComponent* compA = a->AddComponent<TickCounterComponent>();
    TickCounterComponent* compB = b->AddComponent<TickCounterComponent>();
    ASSERT_NE(compA, nullptr);
    ASSERT_NE(compB, nullptr);

    scene.Tick(0.033f);

    EXPECT_EQ(compA->UpdateCount, 1);
    EXPECT_EQ(compB->UpdateCount, 1);
    EXPECT_NEAR(compA->LastDelta, 0.033f, 1e-6f);
    EXPECT_NEAR(compB->LastDelta, 0.033f, 1e-6f);
}

TEST(FSceneTests, DestroyActorRemovesMultipleActorsInSingleTick) {
    FScene scene;
    FActor* actorA = scene.CreateActor("A");
    FActor* actorB = scene.CreateActor("B");
    FActor* actorC = scene.CreateActor("C");
    ASSERT_NE(actorA, nullptr);
    ASSERT_NE(actorB, nullptr);
    ASSERT_NE(actorC, nullptr);

    scene.DestroyActor(actorA);
    scene.DestroyActor(actorC);
    scene.Tick(0.016f);

    EXPECT_EQ(scene.GetAllActors().size(), 1u);
    EXPECT_EQ(scene.FindActorByName("A"), nullptr);
    EXPECT_EQ(scene.FindActorByName("C"), nullptr);
    EXPECT_NE(scene.FindActorByName("B"), nullptr);
}
