#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "LightData.h"

/**
 * @brief 场景视图数据 (Scene View Data)
 * 代表渲染器"观察世界"的角度和环境参数，与客观物体数据 (RenderPacket) 严格分离。
 * 详见原注释。
 */
struct SceneViewData {

    // ==========================================
    // 观察者空间 (Observer Space)
    // ==========================================
    glm::mat4 ViewMatrix;
    glm::mat4 ProjectionMatrix;
    glm::vec3 CameraPosition;

    // ==========================================
    // 场景光源列表 (Scene Lights)
    // 由 FScene::GenerateSceneView() 从场景中所有光源组件收集而来。
    // VulkanRenderer 目前取 Lights[0] 兼容单光源 Shader。
    // Stage 11 多光源时，Shader 侧升级为循环即可。
    // ==========================================
    std::vector<LightData> Lights;
};