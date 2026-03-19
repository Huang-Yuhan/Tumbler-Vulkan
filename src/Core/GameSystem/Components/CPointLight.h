#pragma once
#include "Component.h"
#include <glm/glm.hpp>

/**
 * @brief 点光源组件 (Point Light Component)
 *
 * 向四面八方均匀发光，强度随距离平方衰减 (Inverse Square Law)。
 * 光源的【位置】直接从宿主 Actor 的 Transform.GetPosition() 读取，
 * 本组件只需存储光源本身的物理属性。
 *
 * 使用方式：
 *   FActor* light = Scene->CreateActor("MainLight");
 *   light->Transform.SetPosition(glm::vec3(0.0f, 4.0f, 0.0f));
 *   auto* pl = light->AddComponent<CPointLight>();
 *   pl->Color     = glm::vec3(1.0f, 0.95f, 0.8f); // 暖白光
 *   pl->Intensity = 80.0f;
 */
class CPointLight final : public Component
{
public:
    glm::vec3 Color     = glm::vec3(1.0f, 1.0f, 1.0f); // 光源颜色 (Linear 空间)
    float     Intensity = 50.0f;                         // 光源强度 (对应 LightColor.w)
};
