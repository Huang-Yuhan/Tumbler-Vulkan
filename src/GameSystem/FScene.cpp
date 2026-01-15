#include "FActor.h"
#include "FScene.h"

#include <algorithm>
#include <iostream>


FScene::FScene()=default;
FScene::~FScene()=default;
// 实现移动构造
FScene::FScene(FScene&& other) noexcept = default;

// 实现移动赋值
FScene& FScene::operator=(FScene&& other) noexcept = default;

void FScene::Tick(float deltaTime)
{
    for (const auto& actor : Actors) {
        // 可以考虑添加一个标志位，跳过待删除的 Actor
        for (const auto& comp : actor->Components) {
            comp->Update(deltaTime);
        }
    }
    // 处理待删除的 Actor
    for (FActor* actorToDelete : PendingKillActors)
    {
        auto it = std::ranges::remove_if(Actors,
                                         [actorToDelete](const std::unique_ptr<FActor>& actorPtr) {
                                             return actorPtr.get() == actorToDelete;
                                         }).begin();
        if (it != Actors.end()) {
            Actors.erase(it, Actors.end());
        }
    }
}

FActor* FScene::CreateActor(const std::string& name) {
    FActor* NewActor = FActor::CreateActor(name);
    NewActor->Name = name;
    Actors.push_back(std::unique_ptr<FActor>(NewActor));
    return NewActor;
}

void FScene::DestroyActor(FActor *actor)
{
    if (actor == nullptr) return;

    // 使用 std::ranges::find 直接查找指针
    auto it = std::ranges::find(PendingKillActors, actor);

    // 如果迭代器不指向末尾，说明找到了
    if (it != PendingKillActors.end()) {
        return; // 已存在，跳过
    }

    // 否则添加
    PendingKillActors.push_back(actor);
}

const std::vector<std::unique_ptr<FActor>> & FScene::GetAllActors() const
{
    return Actors;
}
