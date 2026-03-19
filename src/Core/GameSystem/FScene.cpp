#include "FActor.h"
#include "FScene.h"

#include <algorithm>
#include <iostream>

#include "Components/CMeshRenderer.h"
#include "Components/CPointLight.h"
#include "Components/CDirectionalLight.h"
#include "Core/Graphics/LightData.h"

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

SceneViewData FScene::GenerateSceneView(const CCamera *camera, const CTransform *cameraTransform, float aspectRatio) const {
    SceneViewData viewData;

    // 1. Camera view
    viewData.ViewMatrix = camera->GetViewMatrix(*cameraTransform);
    viewData.ProjectionMatrix = camera->GetProjectionMatrix(aspectRatio);
    viewData.CameraPosition = cameraTransform->GetPosition();

    // 2. Collect lights from components
    for (const auto& actorPtr : Actors)
    {
        FActor* actor = actorPtr.get();

        if (auto* pl = actor->GetComponent<CPointLight>())
        {
            LightData data;
            data.Type      = ELightType::Point;
            data.Position  = actor->Transform.GetPosition();
            data.Color     = pl->Color;
            data.Intensity = pl->Intensity;
            viewData.Lights.push_back(data);
        }

        if (auto* dl = actor->GetComponent<CDirectionalLight>())
        {
            LightData data;
            data.Type      = ELightType::Directional;
            data.Direction = actor->Transform.GetForwardVector();
            data.Color     = dl->Color;
            data.Intensity = dl->Intensity;
            viewData.Lights.push_back(data);
        }
    }

    return viewData;
}
