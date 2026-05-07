#include "SceneDataManager.h"
#include "DescriptorManager.h"
#include "LightData.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>

void SceneDataManager::Init(DescriptorManager* descMgr)
{
    mSceneBuffer = &descMgr->GetSceneParameterBuffer();
}

void SceneDataManager::Update(const SceneViewData& viewData)
{
    if (!mSceneBuffer || !mSceneBuffer->Info.pMappedData) return;

    SceneDataUBO sceneData{};

    glm::mat4 viewProj = viewData.ProjectionMatrix * viewData.ViewMatrix;
    sceneData.ViewProjection = viewProj;
    sceneData.InvViewProj = glm::inverse(viewProj);
    sceneData.CameraPosition = glm::vec4(viewData.CameraPosition, 1.0f);

    int count = static_cast<int>(viewData.Lights.size());
    sceneData.LightCount = count < MAX_SCENE_LIGHTS ? count : MAX_SCENE_LIGHTS;

    for (int i = 0; i < sceneData.LightCount; ++i) {
        const auto& cpuLight = viewData.Lights[i];
        float type = (cpuLight.Type == ELightType::Directional) ? 1.0f : 0.0f;
        sceneData.Lights[i].Position = glm::vec4(cpuLight.Position, type);
        sceneData.Lights[i].Color = glm::vec4(cpuLight.Color, cpuLight.Intensity);
        sceneData.Lights[i].Direction = glm::vec4(cpuLight.Direction, cpuLight.Range);
    }
    for (int i = sceneData.LightCount; i < MAX_SCENE_LIGHTS; ++i) {
        sceneData.Lights[i].Position = glm::vec4(0.0f);
        sceneData.Lights[i].Color = glm::vec4(0.0f);
        sceneData.Lights[i].Direction = glm::vec4(0.0f);
    }

    sceneData.LightViewProj = viewData.LightViewProj;

    memcpy(mSceneBuffer->Info.pMappedData, &sceneData, sizeof(SceneDataUBO));
}
