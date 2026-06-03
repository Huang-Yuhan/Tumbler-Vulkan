// Math.h — 数学工具 (Header-only)
//
// 职责: 视锥体平面提取等 GPU-Driven 所需的 CPU 侧数学运算。
//       纯函数，无状态。
//
// 依赖: glm
// 层级: 平台/工具层 (Phase 1)

#pragma once

#include <glm/glm.hpp>
#include <array>

namespace Tumbler::Math {

// 从 ViewProj 矩阵提取 6 个视锥体平面 (Gribb/Hartmann 方法)
// 返回: {左, 右, 下, 上, 近, 远}，每个 vec4 为 (nx, ny, nz, d)，平面方程为 dot(n, P) + d >= 0
inline std::array<glm::vec4, 6> ExtractFrustumPlanes(const glm::mat4& viewProj) {
    std::array<glm::vec4, 6> planes;

    // 左平面:   row3 + row0
    planes[0] = glm::vec4(
        viewProj[0][3] + viewProj[0][0],
        viewProj[1][3] + viewProj[1][0],
        viewProj[2][3] + viewProj[2][0],
        viewProj[3][3] + viewProj[3][0]);

    // 右平面:   row3 - row0
    planes[1] = glm::vec4(
        viewProj[0][3] - viewProj[0][0],
        viewProj[1][3] - viewProj[1][0],
        viewProj[2][3] - viewProj[2][0],
        viewProj[3][3] - viewProj[3][0]);

    // 下平面:   row3 + row1
    planes[2] = glm::vec4(
        viewProj[0][3] + viewProj[0][1],
        viewProj[1][3] + viewProj[1][1],
        viewProj[2][3] + viewProj[2][1],
        viewProj[3][3] + viewProj[3][1]);

    // 上平面:   row3 - row1
    planes[3] = glm::vec4(
        viewProj[0][3] - viewProj[0][1],
        viewProj[1][3] - viewProj[1][1],
        viewProj[2][3] - viewProj[2][1],
        viewProj[3][3] - viewProj[3][1]);

    // 近平面:   row2
    planes[4] = glm::vec4(
        viewProj[0][2],
        viewProj[1][2],
        viewProj[2][2],
        viewProj[3][2]);

    // 远平面:   row3 - row2
    planes[5] = glm::vec4(
        viewProj[0][3] - viewProj[0][2],
        viewProj[1][3] - viewProj[1][2],
        viewProj[2][3] - viewProj[2][2],
        viewProj[3][3] - viewProj[3][2]);

    // 归一化所有平面
    for (auto& p : planes) {
        float len = glm::length(glm::vec3(p));
        p /= len;
    }

    return planes;
}

} // namespace Tumbler::Math
