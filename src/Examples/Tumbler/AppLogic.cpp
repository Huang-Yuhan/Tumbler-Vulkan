#include "AppLogic.h"
#include <glm/vec3.hpp>
#include <imgui.h>

#include "Core/GameSystem/Components/CFirstPersonCamera.h"
#include "Core/GameSystem/InputManager.h"
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
    
    // 初始化一个共用的 1x1 平面 Mesh，借由 FAssetManager 管理
    auto planeMesh = std::make_shared<FMesh>(FMesh::CreatePlane(1.0f, 1.0f, 1, 1));
    AssetMgr->RegisterMesh("DefaultPlane", planeMesh);
    
    InitializePlanes();
}

void AppLogic::InitializePlanes() const
{
    auto planeMesh = AssetMgr->GetOrLoadMesh("DefaultPlane");

    // 添加5个平面,组成康奈尔盒子
    FActor* Floor = Scene->CreateActor("Floor");
    Floor->Transform.SetPosition(glm::vec3(0.0f, -5.0f, 0.0f));
    Floor->Transform.SetScale(glm::vec3(10.0f, 1.0f, 10.0f));
    Floor->AddComponent<CMeshRenderer>()->SetMesh(planeMesh); 

    FActor* Ceiling = Scene->CreateActor("Ceiling");
    Ceiling->Transform.SetPosition(glm::vec3(0.0f, 5.0f, 0.0f));
    Ceiling->Transform.SetScale(glm::vec3(10.0f, 1.0f, 10.0f));
    Ceiling->Transform.SetRotation(glm::vec3(0.0f, 0.0f, -180.0f));
    Ceiling->AddComponent<CMeshRenderer>()->SetMesh(planeMesh);

    FActor* BackWall = Scene->CreateActor("BackWall");
    BackWall->Transform.SetPosition(glm::vec3(0.0f, 0.0f, -5.0f));
    BackWall->Transform.SetScale(glm::vec3(10.0f, 1.0f,10.0f));
    BackWall->Transform.SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
    BackWall->AddComponent<CMeshRenderer>()->SetMesh(planeMesh);

    FActor* LeftWall = Scene->CreateActor("LeftWall");
    LeftWall->Transform.SetPosition(glm::vec3(-5.0f, 0.0f, 0.0f));
    LeftWall->Transform.SetScale(glm::vec3(10.0f, 1.0f,10.0f));
    LeftWall->Transform.SetRotation(glm::vec3(0.0f, 0.0f, -90.0f));
    LeftWall->AddComponent<CMeshRenderer>()->SetMesh(planeMesh);

    FActor* RightWall = Scene->CreateActor("RightWall");
    RightWall->Transform.SetPosition(glm::vec3(5.0f, 0.0f, 0.0f));
    RightWall->Transform.SetScale(glm::vec3(10.0f, 1.0f,10.0f));
    RightWall->Transform.SetRotation(glm::vec3(0.0f, 0.0f, 90.0f));
    RightWall->AddComponent<CMeshRenderer>()->SetMesh(planeMesh);
}

AppLogic::~AppLogic() = default;

FScene* AppLogic::GetScene() { return Scene.get(); }
const FScene* AppLogic::GetScene() const { return Scene.get(); }

void AppLogic::Init(VulkanRenderer* renderer, FAssetManager* assetMgr, InputManager* inputMgr) {
    AssetMgr = assetMgr;
    InputMgr = inputMgr;
    InitializeScene();

    // 0. 创建相机实体与漫游组件
    FActor* cameraActor = Scene->CreateActor("MainCamera");
    cameraActor->Transform.SetPosition(glm::vec3(0.0f, -1.0f, 16.0f));
    cameraActor->Transform.SetRotation(glm::vec3(0.0f, 180.0f, 0.0f));
    MainCamera = cameraActor->AddComponent<CFirstPersonCamera>();
    MainCamera->Fov = 60.0f;
    MainCamera->Init(InputMgr);

    // 1. 创建 PBR 母体材质 (由 AssetManager 管理)
    auto pbrMaterial = AssetMgr->GetOrLoadMaterial("PBR_Base", "assets/shaders/engine/pbr.vert.spv", "assets/shaders/engine/pbr.frag.spv");

    // 2. 康奈尔经典红 (左墙)
    auto matRed = pbrMaterial->CreateInstance();
    matRed->SetVector("BaseColorTint", glm::vec4(0.63f, 0.06f, 0.05f, 1.0f));
    matRed->SetTwoSided(true);

    // 3. 康奈尔经典绿 (右墙)
    auto matGreen = pbrMaterial->CreateInstance();
    matGreen->SetVector("BaseColorTint", glm::vec4(0.15f, 0.48f, 0.09f, 1.0f));
    matGreen->SetTwoSided(true);

    // 4. 康奈尔经典白 (天花板、地板、后墙)
    auto matWhite = pbrMaterial->CreateInstance();
    matWhite->SetVector("BaseColorTint", glm::vec4(0.73f, 0.73f, 0.73f, 1.0f));
    matWhite->SetTwoSided(true);
    matWhite->ApplyChanges();

    // 测试 1：绝缘体（非常粗糙的塑料）- 给左边的红墙
    matRed->SetFloat("Roughness", 0.9f);
    matRed->SetFloat("Metallic", 0.0f);
    matRed->ApplyChanges();

    // 测试 2：纯金属（极度光滑的金/铜镜面）- 给右边的绿墙
    matGreen->SetFloat("Roughness", 0.1f);
    matGreen->SetFloat("Metallic", 1.0f);
    matGreen->ApplyChanges();

    // 5. 挂载到我们之前创建好的墙壁上
    Scene->FindActorByName("LeftWall")->GetComponent<CMeshRenderer>()->SetMaterial(matRed);
    Scene->FindActorByName("RightWall")->GetComponent<CMeshRenderer>()->SetMaterial(matGreen);
    Scene->FindActorByName("Floor")->GetComponent<CMeshRenderer>()->SetMaterial(matWhite);
    Scene->FindActorByName("Ceiling")->GetComponent<CMeshRenderer>()->SetMaterial(matWhite);
    Scene->FindActorByName("BackWall")->GetComponent<CMeshRenderer>()->SetMaterial(matWhite);

    // ==========================================
    // 6. 加载 Sting 剑模型
    // ==========================================
    auto swordMesh = AssetMgr->GetOrLoadMesh("StingSwordMesh", "assets/models/Sting-Sword-lowpoly.obj");

    FActor* sword = Scene->CreateActor("StingSword");
    sword->AddComponent<CMeshRenderer>()->SetMesh(swordMesh);

    // 摆放在场景中央，竖直摆放，缩放到合适大小
    sword->Transform.SetPosition(glm::vec3(0.0f, -3.0f, 0.0f));
    sword->Transform.SetScale(glm::vec3(0.5f, 0.5f, 0.5f));
    sword->Transform.SetRotation(glm::vec3(0.0f, 90.0f, 0.0f));

    // 钢铁金属质感
    auto matSword = pbrMaterial->CreateInstance();
    matSword->SetVector("BaseColorTint", glm::vec4(0.80f, 0.77f, 0.70f, 1.0f)); 
    matSword->SetFloat("Roughness", 0.3f);  
    matSword->SetFloat("Metallic", 1.0f);   
    matSword->ApplyChanges();

    sword->GetComponent<CMeshRenderer>()->SetMaterial(matSword);

    // ==========================================
    // 7. 创建主光源 (使用新组件系统)
    // ==========================================
    FActor* lightActor = Scene->CreateActor("MainLight");
    lightActor->Transform.SetPosition(glm::vec3(0.0f, 4.0f, 0.0f));
    auto* pl = lightActor->AddComponent<CPointLight>();
    pl->Color = glm::vec3(1.0f, 1.0f, 1.0f);
    pl->Intensity = 50.0f;
}

void AppLogic::Tick(float deltaTime) {
    if (Scene) {
        Scene->Tick(deltaTime);
    }
}

void AppLogic::UpdatePerformanceStats(float frameTime, int drawCallCount) {
    Stats.FrameTimeMs = frameTime * 1000.0f;
    Stats.FPS = 1.0f / frameTime;
    Stats.DrawCallCount = drawCallCount;
    
    Stats.FrameTimeHistory[Stats.HistoryIndex] = Stats.FrameTimeMs;
    Stats.HistoryIndex = (Stats.HistoryIndex + 1) % FRAME_TIME_HISTORY_SIZE;
}

void AppLogic::DrawPerformancePanel() {
    ImGui::Begin("Performance");
    
    ImGui::Text("FPS: %.1f", Stats.FPS);
    ImGui::Text("Frame Time: %.2f ms", Stats.FrameTimeMs);
    ImGui::Text("Draw Calls: %d", Stats.DrawCallCount);
    
    ImGui::Separator();
    
    ImGui::Text("Frame Time Graph:");
    ImGui::PlotLines("##FrameTime", Stats.FrameTimeHistory, FRAME_TIME_HISTORY_SIZE, Stats.HistoryIndex, nullptr, 0.0f, 33.3f, ImVec2(0, 80));
    
    ImGui::End();
}

void AppLogic::DrawLightPanel() {
    ImGui::Begin("Light Settings");
    
    if (FActor* mainLight = Scene->FindActorByName("MainLight")) {
        if (auto* pl = mainLight->GetComponent<CPointLight>()) {
            ImGui::Text("Main Light");
            ImGui::Separator();
            
            glm::vec3 pos = mainLight->Transform.GetPosition();
            if (ImGui::DragFloat3("Position", &pos.x, 0.1f, -20.0f, 20.0f)) {
                mainLight->Transform.SetPosition(pos);
            }
            
            ImGui::ColorEdit3("Color", &pl->Color.x);
            ImGui::SliderFloat("Intensity", &pl->Intensity, 0.0f, 200.0f);
        }
    } else {
        ImGui::Text("MainLight not found");
    }
    
    ImGui::End();
}

void AppLogic::DrawCameraPanel() {
    ImGui::Begin("Camera");
    
    if (MainCamera) {
        FActor* cameraActor = MainCamera->GetOwner();
        
        ImGui::Text("Camera Settings");
        ImGui::Separator();
        
        glm::vec3 pos = cameraActor->Transform.GetPosition();
        if (ImGui::DragFloat3("Position", &pos.x, 0.1f, -50.0f, 50.0f)) {
            cameraActor->Transform.SetPosition(pos);
        }
        
        glm::vec3 rot = cameraActor->Transform.GetEulerAngles();
        if (ImGui::DragFloat3("Rotation", &rot.x, 1.0f, -180.0f, 180.0f)) {
            cameraActor->Transform.SetRotation(rot);
        }
        
        ImGui::Separator();
        
        ImGui::SliderFloat("FOV", &MainCamera->Fov, 30.0f, 120.0f);
        ImGui::SliderFloat("Move Speed", &MainCamera->MoveSpeed, 1.0f, 50.0f);
        ImGui::SliderFloat("Mouse Sensitivity", &MainCamera->MouseSensitivity, 0.1f, 5.0f);
    }
    
    ImGui::End();
}

void AppLogic::DrawSceneHierarchyPanel() {
    ImGui::Begin("Scene Hierarchy");
    
    if (Scene) {
        const auto& actors = Scene->GetAllActors();
        
        for (const auto& actor : actors) {
            bool isSelected = (SelectedActor == actor.get());
            
            if (ImGui::Selectable(actor->Name.c_str(), isSelected)) {
                SelectedActor = actor.get();
            }
            
            if (isSelected) {
                ImGui::SetItemDefaultFocus();
            }
        }
    }
    
    ImGui::End();
}

void AppLogic::DrawMaterialPanel() {
    ImGui::Begin("Material Editor");
    
    if (!SelectedActor) {
        ImGui::Text("Select an object in the Scene Hierarchy");
        ImGui::End();
        return;
    }
    
    CMeshRenderer* meshRenderer = SelectedActor->GetComponent<CMeshRenderer>();
    if (!meshRenderer) {
        ImGui::Text("Selected object has no MeshRenderer component");
        ImGui::End();
        return;
    }
    
    auto material = meshRenderer->GetMaterial();
    if (!material) {
        ImGui::Text("Selected object has no material");
        ImGui::End();
        return;
    }
    
    ImGui::Text("Actor: %s", SelectedActor->Name.c_str());
    ImGui::Separator();
    
    bool materialChanged = false;
    
    glm::vec4 baseColor = material->GetBaseColorTint();
    float roughness = material->GetRoughness();
    float metallic = material->GetMetallic();
    float normalStrength = material->GetNormalMapStrength();
    bool twoSided = material->IsTwoSided();
    
    ImGui::Text("Material Parameters");
    ImGui::Separator();
    
    if (ImGui::ColorEdit4("Base Color", &baseColor.x)) {
        material->SetVector("BaseColorTint", baseColor);
        materialChanged = true;
    }
    
    if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
        material->SetFloat("Roughness", roughness);
        materialChanged = true;
    }
    
    if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
        material->SetFloat("Metallic", metallic);
        materialChanged = true;
    }
    
    if (ImGui::SliderFloat("Normal Strength", &normalStrength, 0.0f, 2.0f)) {
        material->SetFloat("NormalMapStrength", normalStrength);
        materialChanged = true;
    }
    
    if (ImGui::Checkbox("Two Sided", &twoSided)) {
        material->SetTwoSided(twoSided);
        materialChanged = true;
    }
    
    ImGui::Separator();
    
    if (materialChanged) {
        material->UpdateUBO();
    }
    
    ImGui::End();
}

void AppLogic::DrawEditorUI() {
    DrawPerformancePanel();
    DrawLightPanel();
    DrawCameraPanel();
    DrawSceneHierarchyPanel();
    DrawMaterialPanel();
}
