#include "SceneDataManager.h"
#include "DescriptorManager.h"
#include "LightData.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstring>

using namespace Tumbler::Math;

// Helper: convert Math::Matrix4f to glm::mat4
static glm::mat4 ToGlm(const Matrix4f& m) {
    return glm::mat4(
        m[0][0], m[0][1], m[0][2], m[0][3],
        m[1][0], m[1][1], m[1][2], m[1][3],
        m[2][0], m[2][1], m[2][2], m[2][3],
        m[3][0], m[3][1], m[3][2], m[3][3]
    );
}

static glm::vec4 ToGlm(const Vector4f& v) {
    return glm::vec4(v.X, v.Y, v.Z, v.W);
}

static glm::vec4 ToGlmVec4(const Vector3f& v, float w) {
    return glm::vec4(v.X, v.Y, v.Z, w);
}

void SceneDataManager::Init(DescriptorManager* descMgr)
{
    mSceneBuffer = &descMgr->GetSceneParameterBuffer();
}

void SceneDataManager::Update(const SceneViewData& viewData)
{
    if (!mSceneBuffer || !mSceneBuffer->Info.pMappedData) return;

    SceneDataUBO sceneData{};

    glm::mat4 viewProj = ToGlm(viewData.ProjectionMatrix) * ToGlm(viewData.ViewMatrix);
    sceneData.ViewProjection = viewProj;
    sceneData.InvViewProj = glm::inverse(viewProj);
    sceneData.CameraPosition = ToGlmVec4(viewData.CameraPosition, 1.0f);

    int count = static_cast<int>(viewData.Lights.size());
    sceneData.LightCount = count < MAX_SCENE_LIGHTS ? count : MAX_SCENE_LIGHTS;

    for (int i = 0; i < sceneData.LightCount; ++i) {
        const auto& cpuLight = viewData.Lights[i];
        float type = (cpuLight.Type == ELightType::Directional) ? 1.0f : 0.0f;
        sceneData.Lights[i].Position = ToGlmVec4(cpuLight.Position, type);
        sceneData.Lights[i].Color = ToGlmVec4(cpuLight.Color, cpuLight.Intensity);
        sceneData.Lights[i].Direction = ToGlmVec4(cpuLight.Direction, cpuLight.Range);
    }
    for (int i = sceneData.LightCount; i < MAX_SCENE_LIGHTS; ++i) {
        sceneData.Lights[i].Position = glm::vec4(0.0f);
        sceneData.Lights[i].Color = glm::vec4(0.0f);
        sceneData.Lights[i].Direction = glm::vec4(0.0f);
    }

    sceneData.LightViewProj = ToGlm(viewData.LightViewProj);

    memcpy(mSceneBuffer->Info.pMappedData, &sceneData, sizeof(SceneDataUBO));
}
