#include <gtest/gtest.h>

#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/Components/Component.h"

using namespace Tumbler;

namespace {

class DummyComponent final : public Component {
public:
    explicit DummyComponent(int value)
        : Value(value) {}

    int Value = 0;
};

class AnotherDummyComponent final : public Component {
public:
    explicit AnotherDummyComponent(float value)
        : Value(value) {}

    float Value = 0.0f;
};

} // namespace

TEST(FActorTests, CreateActorInitializesNameAndTransformOwner) {
    FActor* actor = FActor::CreateActor("UnitActor");
    ASSERT_NE(actor, nullptr);

    EXPECT_EQ(actor->Name, "UnitActor");
    EXPECT_EQ(actor->Transform.GetOwner(), actor);

    delete actor;
}

TEST(FActorTests, AddComponentSetsOwnerAndSupportsGetters) {
    FActor* actor = FActor::CreateActor("ComponentHost");
    ASSERT_NE(actor, nullptr);

    DummyComponent* component = actor->AddComponent<DummyComponent>(7);
    ASSERT_NE(component, nullptr);
    EXPECT_EQ(component->GetOwner(), actor);
    EXPECT_EQ(component->Value, 7);

    DummyComponent* resolvedSingle = actor->GetComponent<DummyComponent>();
    ASSERT_NE(resolvedSingle, nullptr);
    EXPECT_EQ(resolvedSingle, component);

    const std::vector<DummyComponent*> resolvedMany = actor->GetComponents<DummyComponent>();
    ASSERT_EQ(resolvedMany.size(), 1u);
    EXPECT_EQ(resolvedMany[0], component);

    delete actor;
}

TEST(FActorTests, GetComponentTransformSpecializationReturnsEmbeddedTransform) {
    FActor* actor = FActor::CreateActor("TransformOwner");
    ASSERT_NE(actor, nullptr);

    CTransform* transform = actor->GetComponent<CTransform>();
    ASSERT_NE(transform, nullptr);
    EXPECT_EQ(transform, &actor->Transform);

    delete actor;
}

TEST(FActorTests, GetComponentReturnsNullWhenComponentDoesNotExist) {
    FActor* actor = FActor::CreateActor("MissingCompHost");
    ASSERT_NE(actor, nullptr);

    EXPECT_EQ(actor->GetComponent<DummyComponent>(), nullptr);
    EXPECT_EQ(actor->GetComponent<AnotherDummyComponent>(), nullptr);

    delete actor;
}

TEST(FActorTests, GetComponentsReturnsAllMatchingInstances) {
    FActor* actor = FActor::CreateActor("MultiComponentHost");
    ASSERT_NE(actor, nullptr);

    DummyComponent* first = actor->AddComponent<DummyComponent>(1);
    DummyComponent* second = actor->AddComponent<DummyComponent>(2);
    AnotherDummyComponent* other = actor->AddComponent<AnotherDummyComponent>(3.0f);
    ASSERT_NE(first, nullptr);
    ASSERT_NE(second, nullptr);
    ASSERT_NE(other, nullptr);

    const std::vector<DummyComponent*> allDummy = actor->GetComponents<DummyComponent>();
    ASSERT_EQ(allDummy.size(), 2u);
    EXPECT_EQ(allDummy[0], first);
    EXPECT_EQ(allDummy[1], second);
    EXPECT_EQ(actor->GetComponent<AnotherDummyComponent>(), other);

    delete actor;
}
