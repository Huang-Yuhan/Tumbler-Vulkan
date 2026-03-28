#pragma once
#include <glm/glm.hpp>
#include <vector>
#include "LightData.h"

enum class ERenderPath {
    Forward,
    Deferred,
    GPUDriven
};

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
    // ==========================================
    std::vector<LightData> Lights;

    // 选定的渲染管线路径
    ERenderPath RenderPath = ERenderPath::Forward;
};