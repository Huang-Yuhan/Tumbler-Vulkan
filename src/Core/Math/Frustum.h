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

namespace Detail {

inline Planef MakePlaneFromRows(const Matrix4f& matrix, float row0Scale, float row1Scale, float row2Scale,
                                float row3Scale) {
    return Planef{
        row0Scale * matrix[0][0] + row1Scale * matrix[1][0] + row2Scale * matrix[2][0] + row3Scale * matrix[3][0],
        row0Scale * matrix[0][1] + row1Scale * matrix[1][1] + row2Scale * matrix[2][1] + row3Scale * matrix[3][1],
        row0Scale * matrix[0][2] + row1Scale * matrix[1][2] + row2Scale * matrix[2][2] + row3Scale * matrix[3][2],
        row0Scale * matrix[0][3] + row1Scale * matrix[1][3] + row2Scale * matrix[2][3] + row3Scale * matrix[3][3]};
}

inline bool NormalizeAndStore(Planef plane, FrustumPlane index, Frustum& outFrustum) {
    if (!plane.Normalize()) {
        return false;
    }

    outFrustum[index] = plane;
    return true;
}

} // namespace Detail

inline bool ExtractFrustumPlanes(const Matrix4f& viewProj, Frustum& outFrustum,
                                 DepthConvention convention = kDefaultDepthConvention) {
    if (!Detail::NormalizeAndStore(Detail::MakePlaneFromRows(viewProj, 1.0f, 0.0f, 0.0f, 1.0f), FrustumPlane::Left,
                                   outFrustum)) {
        return false;
    }
    if (!Detail::NormalizeAndStore(Detail::MakePlaneFromRows(viewProj, -1.0f, 0.0f, 0.0f, 1.0f), FrustumPlane::Right,
                                   outFrustum)) {
        return false;
    }
    if (!Detail::NormalizeAndStore(Detail::MakePlaneFromRows(viewProj, 0.0f, 1.0f, 0.0f, 1.0f), FrustumPlane::Bottom,
                                   outFrustum)) {
        return false;
    }
    if (!Detail::NormalizeAndStore(Detail::MakePlaneFromRows(viewProj, 0.0f, -1.0f, 0.0f, 1.0f), FrustumPlane::Top,
                                   outFrustum)) {
        return false;
    }

    if (convention == DepthConvention::ReverseZZeroToOne) {
        if (!Detail::NormalizeAndStore(Detail::MakePlaneFromRows(viewProj, 0.0f, 0.0f, -1.0f, 1.0f), FrustumPlane::Near,
                                       outFrustum)) {
            return false;
        }
        return Detail::NormalizeAndStore(Detail::MakePlaneFromRows(viewProj, 0.0f, 0.0f, 1.0f, 0.0f), FrustumPlane::Far,
                                         outFrustum);
    }

    if (!Detail::NormalizeAndStore(Detail::MakePlaneFromRows(viewProj, 0.0f, 0.0f, 1.0f, 0.0f), FrustumPlane::Near,
                                   outFrustum)) {
        return false;
    }
    return Detail::NormalizeAndStore(Detail::MakePlaneFromRows(viewProj, 0.0f, 0.0f, -1.0f, 1.0f), FrustumPlane::Far,
                                     outFrustum);
}

} // namespace Tumbler::Math
