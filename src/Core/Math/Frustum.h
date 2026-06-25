#pragma once

#include "Core/Math/MathConfig.h"
#include "Core/Math/Matrix.h"
#include "Core/Math/Plane.h"

#include <array>
#include <cstddef>

namespace Tumbler::Math {

enum class FrustumPlane : std::size_t {
    Left = 0,
    Right = 1,
    Bottom = 2,
    Top = 3,
    Near = 4,
    Far = 5,
};

enum class FrustumIntersection {
    Outside,
    Intersects,
    Inside,
};

struct Frustum {
    static constexpr std::size_t PlaneCount = 6;

    std::array<Planef, PlaneCount> Planes{};

    [[nodiscard]] Planef& operator[](FrustumPlane plane) { return Planes[static_cast<std::size_t>(plane)]; }
    [[nodiscard]] const Planef& operator[](FrustumPlane plane) const { return Planes[static_cast<std::size_t>(plane)]; }

    [[nodiscard]] FrustumIntersection TestSphere(const Vector3f& center, float radius) const {
        bool intersects = false;

        for (const Planef& plane : Planes) {
            const float distance = plane.SignedDistance(center);
            if (distance < -radius) {
                return FrustumIntersection::Outside;
            }
            if (distance < radius) {
                intersects = true;
            }
        }

        return intersects ? FrustumIntersection::Intersects : FrustumIntersection::Inside;
    }
};

// 基于 Gribb/Hartmann 论文 "Fast Extraction of Viewing Frustum Planes from the World-View-Projection Matrix"
// 从 ViewProj 矩阵的行线性组合提取 6 个平面 (Ax+By+Cz+D=0)，平面法线朝向视锥体内部。
inline bool ExtractFrustumPlanes(const Matrix4f& viewProj, Frustum& outFrustum,
                                 DepthConvention convention = kDefaultDepthConvention) {
    auto makePlane = [&](float r0, float r1, float r2, float r3) {
        return Planef{r0 * viewProj[0][0] + r1 * viewProj[1][0] + r2 * viewProj[2][0] + r3 * viewProj[3][0],
                      r0 * viewProj[0][1] + r1 * viewProj[1][1] + r2 * viewProj[2][1] + r3 * viewProj[3][1],
                      r0 * viewProj[0][2] + r1 * viewProj[1][2] + r2 * viewProj[2][2] + r3 * viewProj[3][2],
                      r0 * viewProj[0][3] + r1 * viewProj[1][3] + r2 * viewProj[2][3] + r3 * viewProj[3][3]};
    };

    auto normalizeAndStore = [&](const Planef& plane, FrustumPlane index) {
        Planef normalized = plane;
        if (!normalized.Normalize()) {
            return false;
        }
        outFrustum[index] = normalized;
        return true;
    };

    // 左/右/底/顶: ±Row3 ± Row[0-2]
    if (!normalizeAndStore(makePlane(1.0f, 0.0f, 0.0f, 1.0f), FrustumPlane::Left))
        return false;
    if (!normalizeAndStore(makePlane(-1.0f, 0.0f, 0.0f, 1.0f), FrustumPlane::Right))
        return false;
    if (!normalizeAndStore(makePlane(0.0f, 1.0f, 0.0f, 1.0f), FrustumPlane::Bottom))
        return false;
    if (!normalizeAndStore(makePlane(0.0f, -1.0f, 0.0f, 1.0f), FrustumPlane::Top))
        return false;

    // 近/远: 根据深度约定调整
    if (convention == DepthConvention::ReverseZZeroToOne) {
        if (!normalizeAndStore(makePlane(0.0f, 0.0f, -1.0f, 1.0f), FrustumPlane::Near))
            return false;
        return normalizeAndStore(makePlane(0.0f, 0.0f, 1.0f, 0.0f), FrustumPlane::Far);
    }

    if (!normalizeAndStore(makePlane(0.0f, 0.0f, 1.0f, 0.0f), FrustumPlane::Near))
        return false;
    return normalizeAndStore(makePlane(0.0f, 0.0f, -1.0f, 1.0f), FrustumPlane::Far);
}

} // namespace Tumbler::Math
