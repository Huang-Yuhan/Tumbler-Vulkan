#include "FScene.h"
#include <algorithm>
#include <iostream>

FActor* FScene::CreateActor(const std::string& name) {
    auto NewActor = FActor::CreateActor(name);
    NewActor->Name = name;
    Actors.push_back(std::unique_ptr<FActor>(NewActor));
    return NewActor;
}