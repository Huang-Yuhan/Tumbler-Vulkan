#pragma once
#include <Core/Math/Math.h>
#include <cstdint>

/**
 * @brief 光源类型枚举
 */
enum class ELightType : uint8_t {
    Point = 0,
    Directional = 1,
};

/**
 * @brief 光源数据 POD 结构体
 */
struct LightData {
    ELightType Type = ELightType::Point;

    Tumbler::Math::Vector3f Position = Tumbler::Math::Vector3f{0.0f, 4.0f, 0.0f};
    Tumbler::Math::Vector3f Direction = Tumbler::Math::Vector3f{0.0f, -1.0f, 0.0f};

    Tumbler::Math::Vector3f Color = Tumbler::Math::Vector3f{1.0f, 1.0f, 1.0f};
    float Intensity = 50.0f;
    float Range = 20.0f;
};
