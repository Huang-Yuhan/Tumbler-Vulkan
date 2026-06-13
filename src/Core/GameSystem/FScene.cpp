#include "FActor.h"
#include "FScene.h"

#include <algorithm>
#include <iostream>

#include "Components/CMeshRenderer.h"
#include "Components/CLightComponent.h"
#include "Components/CPointLight.h"
#include "Components/CDirectionalLight.h"
#include "Core/Graphics/LightData.h"

using namespace Tumbler::Math;

FScene::FScene() = default;
FScene::~FScene() = default;
FScene::FScene(FScene&& other) noexcept = default;
FScene& FScene::operator=(FScene&& other) noexcept = default;

void FScene::Tick(float deltaTime)
{
    for (const auto& actor : Actors) {
        for (const auto& comp : actor->Components) {
            comp->Update(deltaTime);
        }
    }
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

    auto it = std::ranges::find(PendingKillActors, actor);
    if (it != PendingKillActors.end()) {
        return;
    }
    PendingKillActors.push_back(actor);
}

const std::vector<std::unique_ptr<FActor>>& FScene::GetAllActors() const
{
    return Actors;
}

FActor* FScene::FindActorByName(const std::string& name) const
{
    for (const auto& actor : Actors) {
        if (actor->Name == name) {
            return actor.get();
        }
    }
    return nullptr;
}

bool FScene::ContainsActor(const FActor* actor) const
{
    if (actor == nullptr) return false;
    return std::ranges::any_of(Actors, [actor](const std::unique_ptr<FActor>& actorPtr) {
        return actorPtr.get() == actor;
    });
}

void FScene::ExtractRenderPackets(std::vector<RenderPacket>& outPackets) const {
    outPackets.clear();

    for (const auto& actorPtr : Actors) {
        FActor* actor = actorPtr.get();
        auto* meshRenderer = actor->GetComponent<CMeshRenderer>();

        if (meshRenderer && meshRenderer->IsVisible() && meshRenderer->GetMesh() && meshRenderer->GetMaterial()) {
            RenderPacket packet;
            packet.Mesh = meshRenderer->GetMesh();
            packet.Material = meshRenderer->GetMaterial();
            packet.TransformMatrix = actor->Transform.GetLocalToWorldMatrix();
            outPackets.push_back(packet);
        }
    }
}

SceneViewData FScene::GenerateSceneView(const CCamera* camera, const CTransform* cameraTransform,
                                         float aspectRatio) const {
    SceneViewData viewData;

    // 1. Camera view
    viewData.ViewMatrix = camera->GetViewMatrix(*cameraTransform);
    viewData.ProjectionMatrix = camera->GetProjectionMatrix(aspectRatio);
    viewData.CameraPosition = cameraTransform->GetPosition();

    // 2. Collect lights from components
    for (const auto& actorPtr : Actors) {
        FActor* actor = actorPtr.get();
        auto* light = actor->GetComponent<CLightComponent>();
        if (!light) continue;

        LightData data;
        data.Color = light->Color;
        data.Intensity = light->Intensity;

        if (auto* pl = dynamic_cast<CPointLight*>(light)) {
            data.Type = ELightType::Point;
            data.Position = actor->Transform.GetPosition();
            data.Range = pl->Range;
        } else if (dynamic_cast<CDirectionalLight*>(light)) {
            data.Type = ELightType::Directional;
            data.Direction = actor->Transform.GetForwardVector();
        }
        viewData.Lights.push_back(data);
    }

    // 3. Compute shadow LightViewProj from first directional light
    viewData.LightViewProj = Matrix4f::Identity();
    for (const auto& actorPtr : Actors) {
        FActor* actor = actorPtr.get();
        if (auto* dl = actor->GetComponent<CDirectionalLight>()) {
            Vector3f lightDir = actor->Transform.GetForwardVector().GetNormalized();
            Vector3f lightPos = viewData.CameraPosition - lightDir * 15.0f;
            Vector3f up{0.0f, 1.0f, 0.0f};
            if (std::abs(Dot(lightDir, up)) > 0.99f)
                up = Vector3f{1.0f, 0.0f, 0.0f};

            Matrix4f lightView = MakeLookAt(lightPos, lightPos + lightDir, up);
            float half = 20.0f;
            Matrix4f lightProj = MakeOrtho(-half, half, -half, half, 0.1f, 50.0f);
            viewData.LightViewProj = lightProj * lightView;
            break;
        }
    }

    return viewData;
}
