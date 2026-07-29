#pragma once

#include <glm/glm.hpp>

namespace Tumbler {

// Orbital camera with lazy matrix caching.
// Wraps the target point at a fixed distance; yaw/pitch control the orbit angle.
class Camera {
public:
    Camera() = default;

    // ---- Projection ----
    void SetPerspective(float fovYRadians, float nearZ, float farZ);
    void SetAspectRatio(float aspect);

    // ---- View (orbit controls) ----
    void SetTarget(const glm::vec3& target);
    void SetDistance(float distance);
    void Orbit(float deltaYaw, float deltaPitch);
    void Zoom(float delta);

    // ---- Manual look-at (bypasses orbit state) ----
    void LookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up);

    // ---- Matrix accessors (lazy recompute) ----
    const glm::mat4& GetView() const;
    const glm::mat4& GetProjection() const;
    glm::mat4 GetViewProjection() const { return GetProjection() * GetView(); }

    // ---- UI queries ----
    glm::vec3 GetPosition() const;
    float     GetYaw()      const { return m_Yaw; }
    float     GetPitch()    const { return m_Pitch; }
    float     GetDistance() const { return m_Distance; }

private:
    glm::vec3 ComputePosition() const;
    void      RecomputeView() const;
    void      RecomputeProj() const;

    // Orbit params
    glm::vec3 m_Target   = glm::vec3(0.0f);
    float     m_Yaw      = 0.0f;   // radians, 0 = +Z direction
    float     m_Pitch    = 0.0f;   // radians, clamped to (-π/2, π/2)
    float     m_Distance = 20.0f;

    // Projection params
    float m_FovY   = 1.04719755f;   // ~60 degrees
    float m_NearZ  = 0.1f;
    float m_FarZ   = 1000.0f;
    float m_Aspect = 16.0f / 9.0f;

    // Cached matrices
    mutable glm::mat4 m_View       = glm::mat4(1.0f);
    mutable glm::mat4 m_Projection = glm::mat4(1.0f);
    mutable bool      m_ViewDirty  = true;
    mutable bool      m_ProjDirty  = true;
};

} // namespace Tumbler
