#include <gtest/gtest.h>

#include "Core/GameSystem/FActor.h"
#include "Core/GameSystem/Components/Component.h"

namespace {

class DummyComponent final : public Component {
public:
    explicit DummyComponent(int value)
        : Value(value) {}

    int Value = 0;
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
