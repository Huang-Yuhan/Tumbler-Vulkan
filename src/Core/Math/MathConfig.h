#pragma once

#define TUMBLER_DEPTH_CONVENTION_VULKAN_ZERO_TO_ONE 0
#define TUMBLER_DEPTH_CONVENTION_REVERSE_Z_ZERO_TO_ONE 1

#ifndef TUMBLER_MATH_DEFAULT_DEPTH_CONVENTION
#define TUMBLER_MATH_DEFAULT_DEPTH_CONVENTION TUMBLER_DEPTH_CONVENTION_VULKAN_ZERO_TO_ONE
#endif

#if defined(__cplusplus)

namespace Tumbler::Math {

enum class DepthConvention {
    VulkanZeroToOne = TUMBLER_DEPTH_CONVENTION_VULKAN_ZERO_TO_ONE,
    ReverseZZeroToOne = TUMBLER_DEPTH_CONVENTION_REVERSE_Z_ZERO_TO_ONE,
};

#if TUMBLER_MATH_DEFAULT_DEPTH_CONVENTION == TUMBLER_DEPTH_CONVENTION_REVERSE_Z_ZERO_TO_ONE
inline constexpr DepthConvention kDefaultDepthConvention = DepthConvention::ReverseZZeroToOne;
#else
inline constexpr DepthConvention kDefaultDepthConvention = DepthConvention::VulkanZeroToOne;
#endif

inline constexpr float Pi = 3.14159265358979323846f;
inline constexpr float SmallNumber = 1.0e-8f;
inline constexpr float KindaSmallNumber = 1.0e-4f;

} // namespace Tumbler::Math

#elif defined(__HLSL_VERSION)

static const int TumblerDepthConventionVulkanZeroToOne = TUMBLER_DEPTH_CONVENTION_VULKAN_ZERO_TO_ONE;
static const int TumblerDepthConventionReverseZZeroToOne = TUMBLER_DEPTH_CONVENTION_REVERSE_Z_ZERO_TO_ONE;
static const int TumblerDefaultDepthConvention = TUMBLER_MATH_DEFAULT_DEPTH_CONVENTION;
static const float TumblerPi = 3.14159265358979323846f;
static const float TumblerSmallNumber = 1.0e-8f;
static const float TumblerKindaSmallNumber = 1.0e-4f;

#endif
