#pragma once
#include "VulkanTypes.h"
#include "SceneViewData.h"

class DescriptorManager;

class SceneDataManager
{
public:
    void Init(DescriptorManager* descMgr);
    void Update(const SceneViewData& viewData);

private:
    const AllocatedBuffer* mSceneBuffer = nullptr;
};
