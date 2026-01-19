//
// Created by Icecream_Sarkaz on 2026/1/10.
//

#include "AppLogic.h"

#include <glm/vec3.hpp>

#include "Core/GameSystem/FActor.h"

void AppLogic::InitializeScene()
{
    Scene = std::make_unique<FScene>();
    InitializePlanes();

}

void AppLogic::InitializePlanes() const
{
    //添加5个平面,组成康奈尔盒子
    FActor* Floor = Scene->CreateActor("Floor");
    Floor->Transform.SetPosition(glm::vec3(0.0f, -5.0f, 0.0f));
    Floor->Transform.SetScale(glm::vec3(10.0f, 1.0f, 10.0f));
    FActor* Ceiling = Scene->CreateActor("Ceiling");
    Ceiling->Transform.SetPosition(glm::vec3(0.0f, 5.0f, 0.0f));
    Ceiling->Transform.SetScale(glm::vec3(10.0f, 1.0f, 10.0f));
    Ceiling->Transform.SetRotation(glm::vec3(0.0f, 0.0f, -180.0f));
    FActor* BackWall = Scene->CreateActor("BackWall");
    BackWall->Transform.SetPosition(glm::vec3(0.0f, 0.0f, -5.0f));
    BackWall->Transform.SetScale(glm::vec3(10.0f, 10.0f, 1.0f));
    BackWall->Transform.SetRotation(glm::vec3(90.0f, 0.0f, 0.0f));
    FActor* LeftWall = Scene->CreateActor("LeftWall");
    LeftWall->Transform.SetPosition(glm::vec3(-5.0f, 0.0f, 0.0f));
    LeftWall->Transform.SetScale(glm::vec3(10.0f, 10.0f, 1.0f));
    LeftWall->Transform.SetRotation(glm::vec3(0.0f, 0.0f, 90.0f));
    FActor* RightWall = Scene->CreateActor("RightWall");
    RightWall->Transform.SetPosition(glm::vec3(5.0f, 0.0f, 0.0f));
    RightWall->Transform.SetScale(glm::vec3(10.0f, 10.0f, 1.0f));
    RightWall->Transform.SetRotation(glm::vec3(0.0f, 0.0f, -90.0f));
}

AppLogic::AppLogic()
{
    InitializeScene();
}

AppLogic::~AppLogic()= default;

const FScene * AppLogic::GetScene() const
{
    return Scene.get();
}

