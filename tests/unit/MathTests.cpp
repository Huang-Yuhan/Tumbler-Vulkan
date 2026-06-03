#include <gtest/gtest.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>
#include <cmath>
#include "Core/Utils/Math.h"

using namespace Tumbler::Math;

// 构造一个简单的 ViewProj: 相机在原点看向 -Z, 90 度 FOV, 1:1 宽高比
static glm::mat4 MakeTestViewProj() {
    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 0.0f, 5.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f));
    glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
    return proj * view;
}

// 用平面方程测试点: dot(n, P) + d >= 0 表示在平面内侧
static float SignedDistance(const glm::vec4& plane, const glm::vec3& point) {
    return glm::dot(glm::vec3(plane), point) + plane.w;
}

TEST(Math, ExtractFrustumPlanes_ReturnsSixPlanes) {
    auto planes = ExtractFrustumPlanes(MakeTestViewProj());
    EXPECT_EQ(planes.size(), 6);
}

TEST(Math, ExtractFrustumPlanes_NormalizedPlanes) {
    auto planes = ExtractFrustumPlanes(MakeTestViewProj());
    for (const auto& p : planes) {
        float len = glm::length(glm::vec3(p));
        EXPECT_NEAR(len, 1.0f, 1e-5f);
    }
}

TEST(Math, ExtractFrustumPlanes_CameraOriginInside) {
    // 相机在 (0,0,5), 看向原点。原点应在视锥体内。
    auto planes = ExtractFrustumPlanes(MakeTestViewProj());
    glm::vec3 origin(0.0f, 0.0f, 0.0f);

    for (const auto& p : planes) {
        EXPECT_GE(SignedDistance(p, origin), 0.0f);
    }
}

TEST(Math, ExtractFrustumPlanes_PointFarBehindCameraOutside) {
    // 相机看向 -Z, 点 (0,0,10) 在相机后方，应被近平面或远平面剔除
    auto planes = ExtractFrustumPlanes(MakeTestViewProj());
    glm::vec3 behind(0.0f, 0.0f, 10.0f);

    // 至少有一个平面判定它在外面
    bool outside = false;
    for (const auto& p : planes) {
        if (SignedDistance(p, behind) < 0.0f) {
            outside = true;
            break;
        }
    }
    EXPECT_TRUE(outside);
}

TEST(Math, ExtractFrustumPlanes_PointAboveFrustumOutside) {
    // 90 度 FOV, near=0.1, 近平面半高 = 0.1.
    // 相机在 z=5, 看向原点, 近平面在 z=4.9, 半高约 0.1.
    // 所以 y=5 在近平面附近远超视锥体高度。
    auto planes = ExtractFrustumPlanes(MakeTestViewProj());
    glm::vec3 highPoint(0.0f, 10.0f, 0.0f);

    bool outside = false;
    for (const auto& p : planes) {
        if (SignedDistance(p, highPoint) < 0.0f) {
            outside = true;
            break;
        }
    }
    EXPECT_TRUE(outside);
}
