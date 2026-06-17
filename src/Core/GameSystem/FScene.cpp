#include "FScene.h"
#include "FActor.h"

#include <algorithm>
#include <iostream>

using namespace Tumbler::Math;

FScene::FScene() = default;
FScene::~FScene() = default;
FScene::FScene(FScene&& other) noexcept = default;
FScene& FScene::operator=(FScene&& other) noexcept = default;

void FScene::Tick(float deltaTime) {
    for (const auto& actor : Actors) {
        for (const auto& comp : actor->Components) {
            comp->Update(deltaTime);
        }
    }
    for (FActor* actorToDelete : PendingKillActors) {
        auto it = std::ranges::remove_if(Actors, [actorToDelete](const std::unique_ptr<FActor>& actorPtr) {
                      return actorPtr.get() == actorToDelete;
                  }).begin();
        if (it != Actors.end()) {
            Actors.erase(it, Actors.end());
        }
    }
    PendingKillActors.clear();
}

FActor* FScene::CreateActor(const std::string& name) {
    FActor* NewActor = FActor::CreateActor(name);
    NewActor->Name = name;
    Actors.push_back(std::unique_ptr<FActor>(NewActor));
    return NewActor;
}

void FScene::DestroyActor(FActor* actor) {
    if (actor == nullptr)
        return;

    auto it = std::ranges::find(PendingKillActors, actor);
    if (it != PendingKillActors.end()) {
        return;
    }
    PendingKillActors.push_back(actor);
}

const std::vector<std::unique_ptr<FActor>>& FScene::GetAllActors() const {
    return Actors;
}

FActor* FScene::FindActorByName(const std::string& name) const {
    for (const auto& actor : Actors) {
        if (actor->Name == name) {
            return actor.get();
        }
    }
    return nullptr;
}

bool FScene::ContainsActor(const FActor* actor) const {
    if (actor == nullptr)
        return false;
    return std::ranges::any_of(Actors,
                               [actor](const std::unique_ptr<FActor>& actorPtr) { return actorPtr.get() == actor; });
}
