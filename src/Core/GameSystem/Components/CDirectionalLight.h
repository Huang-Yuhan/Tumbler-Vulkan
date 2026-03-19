#pragma once
#include "Component.h"
#include <glm/glm.hpp>

/**
 * @brief 平行光组件 (Directional Light Component)
 *
 * 模拟无限远处的太阳光，所有光线平行入射，无距离衰减。
 * 光源的【方向】直接从宿主 Actor 的 Transform.GetForwardVector() 读取，
 * 通过旋转 Actor 即可控制光照方向。
 *
 * 使用方式：
 *   FActor* sun = Scene->CreateActor("Sun");
 *   sun->Transform.SetRotation(glm::vec3(45.0f, 0.0f, 0.0f)); // 从斜上方俯射
 *   auto* dl = sun->AddComponent<CDirectionalLight>();
 *   dl->Color     = glm::vec3(1.0f, 0.98f, 0.95f); // 日光色
 *   dl->Intensity = 5.0f;
 *
 * 注意：Shader 侧当前只支持单光源 (PointLight 模型)，
 * DirectionalLight 的支持需在 Stage 11 (多光源) 时扩展 Shader。
 */
class CDirectionalLight final : public Component
{
public:
    glm::vec3 Color     = glm::vec3(1.0f, 1.0f, 1.0f); // 光源颜色 (Linear 空间)
    float     Intensity = 5.0f;                          // 光源强度
};
