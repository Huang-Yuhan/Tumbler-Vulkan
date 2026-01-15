#pragma once
#include "GameSystem/FScene.h"


class AppLogic
{
private:
    std::unique_ptr<FScene> Scene;



    void InitializeScene();

public:
    AppLogic();
    ~AppLogic();
};


