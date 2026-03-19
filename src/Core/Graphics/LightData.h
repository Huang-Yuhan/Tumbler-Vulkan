#pragma once
#include <glm/glm.hpp>

/**
 * @brief 光源类型枚举
 */
enum class ELightType : uint8_t
{
    Point       = 0, // 点光源：从一点向四面八方发光，有距离衰减
    Directional = 1, // 平行光：模拟无限远的太阳光，无衰减
};

/**
 * @brief 光源数据 POD 结构体 (Light Data)
 * 由 FScene::GenerateSceneView() 从 CPointLight / CDirectionalLight 组件收集，
 * 存入 SceneViewData::Lights 数组，最终由 VulkanRenderer 写入 GPU UBO。
 *
 * 【类比 RenderPacket】：RenderPacket 是 CMeshRenderer 的"渲染代理"，
 * LightData 则是光源组件的"渲染代理"，两者都是纯数据、不含 Vulkan 资源。
 */
struct LightData
{
    ELightType Type        = ELightType::Point;

    // PointLight 使用：从 Actor 的 Transform.GetPosition() 获取
    // DirectionalLight 忽略此字段（平行光没有位置）
    glm::vec3 Position     = glm::vec3(0.0f, 4.0f, 0.0f);

    // DirectionalLight 使用：从 Actor 的 Transform.GetForwardVector() 获取
    // PointLight 忽略此字段（点光源各方向均匀照射）
    glm::vec3 Direction    = glm::vec3(0.0f, -1.0f, 0.0f);

    glm::vec3 Color        = glm::vec3(1.0f, 1.0f, 1.0f);
    float     Intensity    = 50.0f;
};
