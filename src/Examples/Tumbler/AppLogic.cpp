#include "AppLogic.h"
#include <glm/vec3.hpp>

#include "Core/Assets/FMaterial.h"
#include "Core/Assets/FMaterialInstance.h"
#include "Core/GameSystem/FActor.h"
#include "Core/Geometry/FMesh.h"                           // 【新增】
#include "Core/GameSystem/Components/CMeshRenderer.h"      // 【新增】

void AppLogic::InitializeScene()
{
    Scene = std::make_unique<FScene>();
    
    // 【新增】初始化一个共用的 1x1 平面 Mesh
    DefaultPlaneMesh = std::make_shared<FMesh>(FMesh::CreatePlane(1.0f, 1.0f, 1, 1));
    
    InitializePlanes();
}

void AppLogic::InitializePlanes() const
{
    // 添加5个平面,组成康奈尔盒子
    FActor* Floor = Scene->CreateActor("Floor");
    Floor->Transform.SetPosition(glm::vec3(0.0f, -5.0f, 0.0f));
    Floor->Transform.SetScale(glm::vec3(10.0f, 1.0f, 10.0f));
    Floor->AddComponent<CMeshRenderer>()->SetMesh(DefaultPlaneMesh); // 【新增】挂载渲染组件

    FActor* Ceiling = Scene->CreateActor("Ceiling");
    Ceiling->Transform.SetPosition(glm::vec3(0.0f, 5.0f, 0.0f));
    Ceiling->Transform.SetScale(glm::vec3(10.0f, 1.0f, 10.0f));
    Ceiling->Transform.SetRotation(glm::vec3(0.0f, 0.0f, -180.0f));
    Ceiling->AddComponent<CMeshRenderer>()->SetMesh(DefaultPlaneMesh);

    FActor* BackWall = Scene->CreateActor("BackWall");
    BackWall->Transform.SetPosition(glm::vec3(0.0f, 0.0f, -5.0f));
    BackWall->Transform.SetScale(glm::vec3(10.0f, 1.0f,10.0f));
    BackWall->Transform.SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
    BackWall->AddComponent<CMeshRenderer>()->SetMesh(DefaultPlaneMesh);

    FActor* LeftWall = Scene->CreateActor("LeftWall");
    LeftWall->Transform.SetPosition(glm::vec3(-5.0f, 0.0f, 0.0f));
    LeftWall->Transform.SetScale(glm::vec3(10.0f, 1.0f,10.0f));
    LeftWall->Transform.SetRotation(glm::vec3(0.0f, 0.0f, -90.0f));
    LeftWall->AddComponent<CMeshRenderer>()->SetMesh(DefaultPlaneMesh);

    FActor* RightWall = Scene->CreateActor("RightWall");
    RightWall->Transform.SetPosition(glm::vec3(5.0f, 0.0f, 0.0f));
    RightWall->Transform.SetScale(glm::vec3(10.0f, 1.0f,10.0f));
    RightWall->Transform.SetRotation(glm::vec3(0.0f, 0.0f, 90.0f));
    RightWall->AddComponent<CMeshRenderer>()->SetMesh(DefaultPlaneMesh);
}

AppLogic::AppLogic() { InitializeScene(); }
AppLogic::~AppLogic() = default;

FScene * AppLogic::GetScene() {
    return Scene.get();
}

const FScene* AppLogic::GetScene() const { return Scene.get(); }

void AppLogic::InitializeMaterials(VulkanRenderer* renderer) {
    // 1. 创建 PBR 母体材质
    auto pbrMaterial = std::make_shared<FMaterial>(renderer);
    pbrMaterial->BuildPipeline("assets/shaders/engine/pbr.vert.spv", "assets/shaders/engine/pbr.frag.spv");

    // 2. 康奈尔经典红 (左墙) - 偏暗的砖红色
    auto matRed = pbrMaterial->CreateInstance();
    matRed->SetVector("BaseColorTint", glm::vec4(0.63f, 0.06f, 0.05f, 1.0f));
    matRed->ApplyChanges(); // 提交到 GPU

    // 3. 康奈尔经典绿 (右墙) - 偏暗的森林绿
    auto matGreen = pbrMaterial->CreateInstance();
    matGreen->SetVector("BaseColorTint", glm::vec4(0.15f, 0.48f, 0.09f, 1.0f));
    matGreen->ApplyChanges();

    // 4. 康奈尔经典白 (天花板、地板、后墙) - 物理真实世界中的白墙大约只有 73% 的反射率
    auto matWhite = pbrMaterial->CreateInstance();
    matWhite->SetVector("BaseColorTint", glm::vec4(0.73f, 0.73f, 0.73f, 1.0f));
    matWhite->ApplyChanges();

    // 测试 1：绝缘体（非常粗糙的塑料）- 给左边的红墙
    matRed->SetFloat("Roughness", 0.9f);
    matRed->SetFloat("Metallic", 0.0f);

    // 测试 2：纯金属（极度光滑的金/铜镜面）- 给右边的绿墙
    matGreen->SetFloat("Roughness", 0.1f); // 留一点点粗糙度，能看到光斑
    matGreen->SetFloat("Metallic", 1.0f);

    // 5. 挂载到我们之前创建好的墙壁上
    Scene->FindActorByName("LeftWall")->GetComponent<CMeshRenderer>()->SetMaterial(matRed);
    Scene->FindActorByName("RightWall")->GetComponent<CMeshRenderer>()->SetMaterial(matGreen);
    Scene->FindActorByName("Floor")->GetComponent<CMeshRenderer>()->SetMaterial(matWhite);
    Scene->FindActorByName("Ceiling")->GetComponent<CMeshRenderer>()->SetMaterial(matWhite);
    Scene->FindActorByName("BackWall")->GetComponent<CMeshRenderer>()->SetMaterial(matWhite);

    // ==========================================
    // 6. 加载 Sting 剑模型
    // ==========================================
    auto swordMesh = LoadOBJMesh("assets/models/Sting-Sword-lowpoly.obj", "StingSword");

    // 摆放在场景中央，竖直摆放，缩放到合适大小
    FActor* sword = Scene->FindActorByName("StingSword");
    sword->Transform.SetPosition(glm::vec3(0.0f, -3.0f, 0.0f));
    sword->Transform.SetScale(glm::vec3(0.5f, 0.5f, 0.5f));
    sword->Transform.SetRotation(glm::vec3(0.0f, 90.0f, 0.0f));  // 后续可调整朝向

    // 钢铁金属质感：高金属度、中等粗糙度(有些磨损感)
    auto matSword = pbrMaterial->CreateInstance();
    matSword->SetVector("BaseColorTint", glm::vec4(0.80f, 0.77f, 0.70f, 1.0f)); // 淡银灰色
    matSword->SetFloat("Roughness", 0.3f);  // 略有划痕，不是完美镜面
    matSword->SetFloat("Metallic", 1.0f);   // 完全金属
    matSword->ApplyChanges();

    sword->GetComponent<CMeshRenderer>()->SetMaterial(matSword);

    // 提前上传到 GPU（避免首帧卡顿）
    renderer->UploadMesh(swordMesh.get());
}

std::shared_ptr<FMesh> AppLogic::LoadOBJMesh(const std::string& filePath, const std::string& actorName)
{
    // 1. 加载 OBJ，解析顶点数据
    auto mesh = std::make_shared<FMesh>(FMesh::LoadFromOBJ(filePath));

    // 2. 创建 Actor，挂载 CMeshRenderer（材质由调用方另行设置）
    FActor* actor = Scene->CreateActor(actorName);
    actor->AddComponent<CMeshRenderer>()->SetMesh(mesh);

    // 3. 保存所有权，防止 mesh 在函数外被提前析构
    ObjMeshes.push_back(mesh);

    return mesh;
}