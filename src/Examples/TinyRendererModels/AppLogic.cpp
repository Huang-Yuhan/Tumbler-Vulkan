#include "AppLogic.h"
#include <glm/vec3.hpp>

#include "Core/GameSystem/Components/CFirstPersonCamera.h"
#include "Core/GameSystem/InputManager.h"
#include "Core/Assets/FMaterial.h"
#include "Core/Assets/FMaterialInstance.h"
#include "Core/Assets/FAssetManager.h"
#include "Core/GameSystem/FActor.h"
#include "Core/Geometry/FMesh.h"
#include "Core/GameSystem/Components/CMeshRenderer.h"
#include "Core/GameSystem/Components/CPointLight.h"

void AppLogic::InitializeScene()
{
    Scene = std::make_unique<FScene>();
    LoadTinyRendererModels();
}

void AppLogic::LoadTinyRendererModels() const
{
    auto pbrMaterial = AssetMgr->GetOrLoadMaterial("PBR_Base", "assets/shaders/engine/pbr.vert.spv", "assets/shaders/engine/pbr.frag.spv");

    auto defaultMat = pbrMaterial->CreateInstance();
    defaultMat->SetVector("BaseColorTint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    defaultMat->SetFloat("Roughness", 0.5f);
    defaultMat->SetFloat("Metallic", 0.0f);
    defaultMat->ApplyChanges();

    auto floorMesh = AssetMgr->GetOrLoadMesh("Floor", "assets/models/tiny-renderer-obj/floor.obj");
    FActor* floor = Scene->CreateActor("Floor");
    floor->Transform.SetPosition(glm::vec3(0.0f, -1.0f, 0.0f));
    floor->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    floor->AddComponent<CMeshRenderer>()->SetMesh(floorMesh);
    floor->GetComponent<CMeshRenderer>()->SetMaterial(defaultMat);

    auto diabloMesh = AssetMgr->GetOrLoadMesh("Diablo", "assets/models/tiny-renderer-obj/diablo3_pose/diablo3_pose.obj");
    FActor* diablo = Scene->CreateActor("Diablo");
    diablo->Transform.SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    diablo->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    diablo->AddComponent<CMeshRenderer>()->SetMesh(diabloMesh);
    diablo->GetComponent<CMeshRenderer>()->SetMaterial(defaultMat);

    auto africanHeadMesh = AssetMgr->GetOrLoadMesh("AfricanHead", "assets/models/tiny-renderer-obj/african_head/african_head.obj");
    FActor* africanHead = Scene->CreateActor("AfricanHead");
    africanHead->Transform.SetPosition(glm::vec3(-3.0f, 0.0f, 0.0f));
    africanHead->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    africanHead->AddComponent<CMeshRenderer>()->SetMesh(africanHeadMesh);
    africanHead->GetComponent<CMeshRenderer>()->SetMaterial(defaultMat);

    auto boggieHeadMesh = AssetMgr->GetOrLoadMesh("BoggieHead", "assets/models/tiny-renderer-obj/boggie/head.obj");
    FActor* boggieHead = Scene->CreateActor("BoggieHead");
    boggieHead->Transform.SetPosition(glm::vec3(3.0f, 0.0f, 0.0f));
    boggieHead->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    boggieHead->AddComponent<CMeshRenderer>()->SetMesh(boggieHeadMesh);
    boggieHead->GetComponent<CMeshRenderer>()->SetMaterial(defaultMat);

    auto boggieBodyMesh = AssetMgr->GetOrLoadMesh("BoggieBody", "assets/models/tiny-renderer-obj/boggie/body.obj");
    FActor* boggieBody = Scene->CreateActor("BoggieBody");
    boggieBody->Transform.SetPosition(glm::vec3(3.0f, 0.0f, 0.0f));
    boggieBody->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    boggieBody->AddComponent<CMeshRenderer>()->SetMesh(boggieBodyMesh);
    boggieBody->GetComponent<CMeshRenderer>()->SetMaterial(defaultMat);

    auto boggieEyesMesh = AssetMgr->GetOrLoadMesh("BoggieEyes", "assets/models/tiny-renderer-obj/boggie/eyes.obj");
    FActor* boggieEyes = Scene->CreateActor("BoggieEyes");
    boggieEyes->Transform.SetPosition(glm::vec3(3.0f, 0.0f, 0.0f));
    boggieEyes->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    boggieEyes->AddComponent<CMeshRenderer>()->SetMesh(boggieEyesMesh);
    boggieEyes->GetComponent<CMeshRenderer>()->SetMaterial(defaultMat);

    FActor* lightActor = Scene->CreateActor("MainLight");
    lightActor->Transform.SetPosition(glm::vec3(0.0f, 5.0f, 5.0f));
    auto* pl = lightActor->AddComponent<CPointLight>();
    pl->Color = glm::vec3(1.0f, 1.0f, 1.0f);
    pl->Intensity = 100.0f;
}

AppLogic::~AppLogic() = default;

FScene* AppLogic::GetScene() { return Scene.get(); }
const FScene* AppLogic::GetScene() const { return Scene.get(); }

void AppLogic::Init(VulkanRenderer* renderer, FAssetManager* assetMgr, InputManager* inputMgr) {
    AssetMgr = assetMgr;
    InputMgr = inputMgr;
    InitializeScene();

    FActor* cameraActor = Scene->CreateActor("MainCamera");
    cameraActor->Transform.SetPosition(glm::vec3(0.0f, 2.0f, 8.0f));
    cameraActor->Transform.SetRotation(glm::vec3(0.0f, 180.0f, 0.0f));
    MainCamera = cameraActor->AddComponent<CFirstPersonCamera>();
    MainCamera->Fov = 60.0f;
    MainCamera->Init(InputMgr);
}

void AppLogic::Tick(float deltaTime) {
    if (Scene) {
        Scene->Tick(deltaTime);
    }
}
