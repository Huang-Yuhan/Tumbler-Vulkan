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

    auto floorMat = pbrMaterial->CreateInstance();
    floorMat->SetVector("BaseColorTint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    floorMat->SetFloat("Roughness", 0.8f);
    floorMat->SetFloat("Metallic", 0.0f);
    auto floorDiffuse = AssetMgr->GetOrLoadTexture("Floor_Diffuse", "assets/models/tiny-renderer-obj/floor_diffuse.tga");
    floorMat->SetTexture("BaseColorMap", floorDiffuse);
    auto floorNormal = AssetMgr->GetOrLoadTexture("Floor_Normal", "assets/models/tiny-renderer-obj/floor_nm_tangent.tga");
    floorMat->SetTexture("NormalMap", floorNormal);
    floorMat->ApplyChanges();

    auto floorMesh = AssetMgr->GetOrLoadMesh("Floor", "assets/models/tiny-renderer-obj/floor.obj");
    FActor* floor = Scene->CreateActor("Floor");
    floor->Transform.SetPosition(glm::vec3(0.0f, -1.0f, 0.0f));
    floor->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    floor->AddComponent<CMeshRenderer>()->SetMesh(floorMesh);
    floor->GetComponent<CMeshRenderer>()->SetMaterial(floorMat);

    auto diabloMat = pbrMaterial->CreateInstance();
    diabloMat->SetVector("BaseColorTint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    diabloMat->SetFloat("Roughness", 0.4f);
    diabloMat->SetFloat("Metallic", 0.3f);
    auto diabloDiffuse = AssetMgr->GetOrLoadTexture("Diablo_Diffuse", "assets/models/tiny-renderer-obj/diablo3_pose/diablo3_pose_diffuse.tga");
    diabloMat->SetTexture("BaseColorMap", diabloDiffuse);
    auto diabloNormal = AssetMgr->GetOrLoadTexture("Diablo_Normal", "assets/models/tiny-renderer-obj/diablo3_pose/diablo3_pose_nm_tangent.tga");
    diabloMat->SetTexture("NormalMap", diabloNormal);
    diabloMat->ApplyChanges();

    auto diabloMesh = AssetMgr->GetOrLoadMesh("Diablo", "assets/models/tiny-renderer-obj/diablo3_pose/diablo3_pose.obj");
    FActor* diablo = Scene->CreateActor("Diablo");
    diablo->Transform.SetPosition(glm::vec3(0.0f, 0.0f, 0.0f));
    diablo->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    diablo->AddComponent<CMeshRenderer>()->SetMesh(diabloMesh);
    diablo->GetComponent<CMeshRenderer>()->SetMaterial(diabloMat);

    auto africanHeadMat = pbrMaterial->CreateInstance();
    africanHeadMat->SetVector("BaseColorTint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    africanHeadMat->SetFloat("Roughness", 0.5f);
    africanHeadMat->SetFloat("Metallic", 0.0f);
    auto africanDiffuse = AssetMgr->GetOrLoadTexture("African_Diffuse", "assets/models/tiny-renderer-obj/african_head/african_head_diffuse.tga");
    africanHeadMat->SetTexture("BaseColorMap", africanDiffuse);
    auto africanNormal = AssetMgr->GetOrLoadTexture("African_Normal", "assets/models/tiny-renderer-obj/african_head/african_head_nm_tangent.tga");
    africanHeadMat->SetTexture("NormalMap", africanNormal);
    africanHeadMat->ApplyChanges();

    auto africanHeadMesh = AssetMgr->GetOrLoadMesh("AfricanHead", "assets/models/tiny-renderer-obj/african_head/african_head.obj");
    FActor* africanHead = Scene->CreateActor("AfricanHead");
    africanHead->Transform.SetPosition(glm::vec3(-3.0f, 0.0f, 0.0f));
    africanHead->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    africanHead->AddComponent<CMeshRenderer>()->SetMesh(africanHeadMesh);
    africanHead->GetComponent<CMeshRenderer>()->SetMaterial(africanHeadMat);

    auto africanEyeInnerMat = pbrMaterial->CreateInstance();
    africanEyeInnerMat->SetVector("BaseColorTint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    africanEyeInnerMat->SetFloat("Roughness", 0.2f);
    africanEyeInnerMat->SetFloat("Metallic", 0.0f);
    auto africanEyeInnerDiffuse = AssetMgr->GetOrLoadTexture("AfricanEyeInner_Diffuse", "assets/models/tiny-renderer-obj/african_head/african_head_eye_inner_diffuse.tga");
    africanEyeInnerMat->SetTexture("BaseColorMap", africanEyeInnerDiffuse);
    auto africanEyeInnerNormal = AssetMgr->GetOrLoadTexture("AfricanEyeInner_Normal", "assets/models/tiny-renderer-obj/african_head/african_head_eye_inner_nm_tangent.tga");
    africanEyeInnerMat->SetTexture("NormalMap", africanEyeInnerNormal);
    africanEyeInnerMat->ApplyChanges();

    auto africanEyeInnerMesh = AssetMgr->GetOrLoadMesh("AfricanEyeInner", "assets/models/tiny-renderer-obj/african_head/african_head_eye_inner.obj");
    FActor* africanEyeInner = Scene->CreateActor("AfricanEyeInner");
    africanEyeInner->Transform.SetPosition(glm::vec3(-3.0f, 0.0f, 0.0f));
    africanEyeInner->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    africanEyeInner->AddComponent<CMeshRenderer>()->SetMesh(africanEyeInnerMesh);
    africanEyeInner->GetComponent<CMeshRenderer>()->SetMaterial(africanEyeInnerMat);

    auto africanEyeOuterMat = pbrMaterial->CreateInstance();
    africanEyeOuterMat->SetVector("BaseColorTint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    africanEyeOuterMat->SetFloat("Roughness", 0.1f);
    africanEyeOuterMat->SetFloat("Metallic", 0.0f);
    auto africanEyeOuterDiffuse = AssetMgr->GetOrLoadTexture("AfricanEyeOuter_Diffuse", "assets/models/tiny-renderer-obj/african_head/african_head_eye_outer_diffuse.tga");
    africanEyeOuterMat->SetTexture("BaseColorMap", africanEyeOuterDiffuse);
    auto africanEyeOuterNormal = AssetMgr->GetOrLoadTexture("AfricanEyeOuter_Normal", "assets/models/tiny-renderer-obj/african_head/african_head_eye_outer_nm_tangent.tga");
    africanEyeOuterMat->SetTexture("NormalMap", africanEyeOuterNormal);
    africanEyeOuterMat->ApplyChanges();

    auto africanEyeOuterMesh = AssetMgr->GetOrLoadMesh("AfricanEyeOuter", "assets/models/tiny-renderer-obj/african_head/african_head_eye_outer.obj");
    FActor* africanEyeOuter = Scene->CreateActor("AfricanEyeOuter");
    africanEyeOuter->Transform.SetPosition(glm::vec3(-3.0f, 0.0f, 0.0f));
    africanEyeOuter->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    africanEyeOuter->AddComponent<CMeshRenderer>()->SetMesh(africanEyeOuterMesh);
    africanEyeOuter->GetComponent<CMeshRenderer>()->SetMaterial(africanEyeOuterMat);

    auto boggieHeadMat = pbrMaterial->CreateInstance();
    boggieHeadMat->SetVector("BaseColorTint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    boggieHeadMat->SetFloat("Roughness", 0.6f);
    boggieHeadMat->SetFloat("Metallic", 0.0f);
    auto boggieHeadDiffuse = AssetMgr->GetOrLoadTexture("BoggieHead_Diffuse", "assets/models/tiny-renderer-obj/boggie/head_diffuse.tga");
    boggieHeadMat->SetTexture("BaseColorMap", boggieHeadDiffuse);
    auto boggieHeadNormal = AssetMgr->GetOrLoadTexture("BoggieHead_Normal", "assets/models/tiny-renderer-obj/boggie/head_nm_tangent.tga");
    boggieHeadMat->SetTexture("NormalMap", boggieHeadNormal);
    boggieHeadMat->ApplyChanges();

    auto boggieHeadMesh = AssetMgr->GetOrLoadMesh("BoggieHead", "assets/models/tiny-renderer-obj/boggie/head.obj");
    FActor* boggieHead = Scene->CreateActor("BoggieHead");
    boggieHead->Transform.SetPosition(glm::vec3(3.0f, 0.0f, 0.0f));
    boggieHead->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    boggieHead->AddComponent<CMeshRenderer>()->SetMesh(boggieHeadMesh);
    boggieHead->GetComponent<CMeshRenderer>()->SetMaterial(boggieHeadMat);

    auto boggieBodyMat = pbrMaterial->CreateInstance();
    boggieBodyMat->SetVector("BaseColorTint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    boggieBodyMat->SetFloat("Roughness", 0.7f);
    boggieBodyMat->SetFloat("Metallic", 0.0f);
    auto boggieBodyDiffuse = AssetMgr->GetOrLoadTexture("BoggieBody_Diffuse", "assets/models/tiny-renderer-obj/boggie/body_diffuse.tga");
    boggieBodyMat->SetTexture("BaseColorMap", boggieBodyDiffuse);
    auto boggieBodyNormal = AssetMgr->GetOrLoadTexture("BoggieBody_Normal", "assets/models/tiny-renderer-obj/boggie/body_nm_tangent.tga");
    boggieBodyMat->SetTexture("NormalMap", boggieBodyNormal);
    boggieBodyMat->ApplyChanges();

    auto boggieBodyMesh = AssetMgr->GetOrLoadMesh("BoggieBody", "assets/models/tiny-renderer-obj/boggie/body.obj");
    FActor* boggieBody = Scene->CreateActor("BoggieBody");
    boggieBody->Transform.SetPosition(glm::vec3(3.0f, 0.0f, 0.0f));
    boggieBody->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    boggieBody->AddComponent<CMeshRenderer>()->SetMesh(boggieBodyMesh);
    boggieBody->GetComponent<CMeshRenderer>()->SetMaterial(boggieBodyMat);

    auto boggieEyesMat = pbrMaterial->CreateInstance();
    boggieEyesMat->SetVector("BaseColorTint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
    boggieEyesMat->SetFloat("Roughness", 0.3f);
    boggieEyesMat->SetFloat("Metallic", 0.0f);
    auto boggieEyesDiffuse = AssetMgr->GetOrLoadTexture("BoggieEyes_Diffuse", "assets/models/tiny-renderer-obj/boggie/eyes_diffuse.tga");
    boggieEyesMat->SetTexture("BaseColorMap", boggieEyesDiffuse);
    auto boggieEyesNormal = AssetMgr->GetOrLoadTexture("BoggieEyes_Normal", "assets/models/tiny-renderer-obj/boggie/eyes_nm_tangent.tga");
    boggieEyesMat->SetTexture("NormalMap", boggieEyesNormal);
    boggieEyesMat->ApplyChanges();

    auto boggieEyesMesh = AssetMgr->GetOrLoadMesh("BoggieEyes", "assets/models/tiny-renderer-obj/boggie/eyes.obj");
    FActor* boggieEyes = Scene->CreateActor("BoggieEyes");
    boggieEyes->Transform.SetPosition(glm::vec3(3.0f, 0.0f, 0.0f));
    boggieEyes->Transform.SetScale(glm::vec3(1.0f, 1.0f, 1.0f));
    boggieEyes->AddComponent<CMeshRenderer>()->SetMesh(boggieEyesMesh);
    boggieEyes->GetComponent<CMeshRenderer>()->SetMaterial(boggieEyesMat);

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
