#include <gtest/gtest.h>

#include "Core/GameSystem/FScene.h"

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
