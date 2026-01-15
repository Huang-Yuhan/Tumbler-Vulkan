//
// Created by Icecream_Sarkaz on 2026/1/10.
//

#include "AppLogic.h"

#include <glm/vec3.hpp>

#include "GameSystem/FActor.h"

void AppLogic::InitializeScene()
{
    Scene = std::make_unique<FScene>();
    //添加5个平面
    for (int i = 0; i < 5; ++i)
    {
        FActor* PlaneActor = Scene->CreateActor("Plane_" + std::to_string(i));
        PlaneActor->Transform.SetPosition(glm::vec3(0.0f, 0.0f, i * 2.0f));
        PlaneActor->Transform.SetScale(glm::vec3(5.0f, 1.0f, 5.0f));
    }
}

AppLogic::AppLogic()
{
    InitializeScene();
}

AppLogic::~AppLogic()= default;

