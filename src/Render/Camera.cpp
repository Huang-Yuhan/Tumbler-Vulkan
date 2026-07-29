#include "Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

#include <algorithm>
#include <cmath>

namespace Tumbler {

// ===================================================================
// Projection
// ===================================================================

void Camera::SetPerspective(float fovYRadians, float nearZ, float farZ) {
    m_FovY  = fovYRadians;
    m_NearZ = nearZ;
    m_FarZ  = farZ;
    m_ProjDirty = true;
}

void Camera::SetAspectRatio(float aspect) {
    m_Aspect = aspect;
    m_ProjDirty = true;
}

// ===================================================================
// View (orbit)
// ===================================================================

void Camera::SetTarget(const glm::vec3& target) {
    m_Target    = target;
    m_ViewDirty = true;
}

void Camera::SetDistance(float distance) {
    m_Distance  = std::max(distance, 0.1f);
    m_ViewDirty = true;
}

void Camera::Orbit(float deltaYaw, float deltaPitch) {
    m_Yaw   += deltaYaw;
    m_Pitch += deltaPitch;
    m_Pitch  = std::clamp(m_Pitch, -glm::half_pi<float>() + 0.001f,
                          glm::half_pi<float>() - 0.001f);
    m_ViewDirty = true;
}

void Camera::Zoom(float delta) {
    SetDistance(m_Distance - delta);
}

// ===================================================================
// Manual look-at
// ===================================================================

void Camera::LookAt(const glm::vec3& eye, const glm::vec3& center, const glm::vec3& up) {
    // Set orbit params to match this view for continuity
    m_Target   = center;
    glm::vec3 dir = eye - center;
    m_Distance = glm::length(dir);
    if (m_Distance > 0.001f) {
        glm::vec3 fwd = glm::normalize(dir);
        m_Yaw   = std::atan2(fwd.x, fwd.z);
        m_Pitch = std::asin(-fwd.y);
    }
    m_View     = glm::lookAt(eye, center, up);
    m_ViewDirty = false;
}

// ===================================================================
// Matrix accessors
// ===================================================================

const glm::mat4& Camera::GetView() const {
    if (m_ViewDirty) RecomputeView();
    return m_View;
}

const glm::mat4& Camera::GetProjection() const {
    if (m_ProjDirty) RecomputeProj();
    return m_Projection;
}

// ===================================================================
// Position
// ===================================================================

glm::vec3 Camera::GetPosition() const {
    return m_Target + ComputePosition();
}

// ===================================================================
// Internal
// ===================================================================

glm::vec3 Camera::ComputePosition() const {
    float cosPitch = std::cos(m_Pitch);
    return m_Distance * glm::vec3(
        cosPitch * std::sin(m_Yaw),
        std::sin(m_Pitch),
        cosPitch * std::cos(m_Yaw));
}

void Camera::RecomputeView() const {
    auto eye = m_Target + ComputePosition();
    m_View = glm::lookAt(eye, m_Target, glm::vec3(0.0f, 1.0f, 0.0f));
    m_ViewDirty = false;
}

void Camera::RecomputeProj() const {
    m_Projection = glm::perspective(m_FovY, m_Aspect, m_NearZ, m_FarZ);
    m_Projection[1][1] *= -1;  // Vulkan Y-flip
    m_ProjDirty = false;
}

} // namespace Tumbler
