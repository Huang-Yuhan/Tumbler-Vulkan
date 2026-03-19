#include "FActor.h"
#include "FScene.h"

#include <algorithm>
#include <iostream>

#include "Components/CMeshRenderer.h"


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
    PendingKillActors.clear();
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

FActor * FScene::FindActorByName(const std::string &name) const
{
    for (const auto& actor : Actors) {
        if (actor->Name == name) {
            return actor.get();
        }
    }
    return nullptr;
}

void FScene::ExtractRenderPackets(std::vector<RenderPacket>& outPackets) const {
    outPackets.clear(); // 清空上一帧的旧包裹

    for (const auto& actorPtr : Actors) {
        FActor* actor = actorPtr.get();
        auto* meshRenderer = actor->GetComponent<CMeshRenderer>();

        if (meshRenderer && meshRenderer->IsVisible() && meshRenderer->GetMesh() && meshRenderer->GetMaterial()) {

            RenderPacket packet;
            packet.Mesh = meshRenderer->GetMesh().get();
            packet.Material = meshRenderer->GetMaterial().get();
            packet.TransformMatrix = actor->Transform.GetLocalToWorldMatrix();

            outPackets.push_back(packet);
        }
    }
}

SceneViewData FScene::GenerateSceneView(const CCamera *camera, const CTransform *cameraTransform) const {
    SceneViewData viewData;

    // 1. 从 Camera 获取“怎么看” (视角)
    viewData.ViewMatrix = camera->GetViewMatrix(*cameraTransform);
    // 这里 aspect 的获取如果不想依赖 renderer，可以传进来，或者写死为 1280/720 临时用
    viewData.ProjectionMatrix = camera->GetProjectionMatrix(1280.0f / 720.0f);
    viewData.CameraPosition = cameraTransform->GetPosition();

    // 2. 从 Scene 获取“环境光照” (环境)
    viewData.LightPosition = GlobalLightPos;
    viewData.LightColor = GlobalLightColor;
    viewData.LightIntensity = GlobalLightIntensity;

    return viewData;
}
