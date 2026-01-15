#pragma once
#include "Core/GameSystem/FScene.h"


class AppLogic
{
private:
    std::unique_ptr<FScene> Scene;



    void InitializeScene();
    void InitializePlanes() const;

public:
    AppLogic();
    ~AppLogic();

    [[nodiscard]] const FScene* GetScene() const;
};


