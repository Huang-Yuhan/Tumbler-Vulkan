#pragma once
#include "CLightComponent.h"

/**
 * @brief 点光源组件 (Point Light Component)
 *
 * 向四面八方均匀发光，强度随距离平方衰减 (Inverse Square Law)。
 * 光源的【位置】直接从宿主 Actor 的 Transform.GetPosition() 读取。
 */
class CPointLight final : public CLightComponent
{
public:
    CPointLight() { Intensity = 50.0f; }

    float Range = 20.0f;

    void OnDrawUI() override;
};
