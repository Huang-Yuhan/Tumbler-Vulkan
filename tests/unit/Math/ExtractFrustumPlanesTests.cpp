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
    // 相机在 (0,0,5) 看向原点, 距离 5, 半高 = 5, y=10 远超上平面
    auto planes = ExtractFrustumPlanes(MakeTestViewProj());
    glm::vec3 highPoint(0.0f, 10.0f, 0.0f);

    // 上平面 (index 3) 应该拒绝: dot(n_up, P) + d < 0
    float sd = SignedDistance(planes[3], highPoint);
    EXPECT_LT(sd, 0.0f);
}

TEST(Math, ExtractFrustumPlanes_PointRightOfFrustumOutside) {
    // 相机在 (0,0,5) 看向原点, 距离 5, 半宽 = 5.
    // x=10 超出 frustum, 被右平面 (index 1) 拒绝.
    auto planes = ExtractFrustumPlanes(MakeTestViewProj());
    glm::vec3 leftPoint(10.0f, 0.0f, 0.0f);

    float sd = SignedDistance(planes[1], leftPoint);
    EXPECT_LT(sd, 0.0f);
}

TEST(Math, ExtractFrustumPlanes_PointBeyondFarPlaneOutside) {
    // 相机在 (0,0,5), far=100, 点 (0,0,-200) 距离相机 205, 远超远平面
    auto planes = ExtractFrustumPlanes(MakeTestViewProj());
    glm::vec3 farPoint(0.0f, 0.0f, -200.0f);

    float sd = SignedDistance(planes[5], farPoint);
    EXPECT_LT(sd, 0.0f);
}

TEST(Math, ExtractFrustumPlanes_PointWellInsideAllInside) {
    // 相机在 (0,0,5) 看向原点, 点 (1,-1,-3) 距离 ~8.5, 在视锥体深处
    auto planes = ExtractFrustumPlanes(MakeTestViewProj());
    glm::vec3 insidePoint(1.0f, -1.0f, -3.0f);

    for (const auto& p : planes) {
        EXPECT_GE(SignedDistance(p, insidePoint), 0.0f);
    }
}
