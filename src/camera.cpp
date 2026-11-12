#include "camera.h"

#include <bx/math.h>

#include <cmath>

namespace {
constexpr float kMouseSensitivity = 0.0025f;
constexpr float kMaxPitch         = 1.5f;
constexpr float kFovDegrees       = 60.0f;
constexpr float kNear             = 0.1f;
constexpr float kFar              = 200.0f;
} // namespace

void Camera::look(float dxPixels, float dyPixels) {
    m_yaw   += dxPixels * kMouseSensitivity;
    m_pitch -= dyPixels * kMouseSensitivity;
    if (m_pitch >  kMaxPitch) m_pitch =  kMaxPitch;
    if (m_pitch < -kMaxPitch) m_pitch = -kMaxPitch;
}

void Camera::move(float forward, float right, float up) {
    const float fx = std::cos(m_pitch) * std::sin(m_yaw);
    const float fy = std::sin(m_pitch);
    const float fz = std::cos(m_pitch) * std::cos(m_yaw);
    const float rx = std::cos(m_yaw); // vecteur "droite" (horizontal)
    const float rz = -std::sin(m_yaw);
    m_x += fx * forward + rx * right;
    m_y += fy * forward + up;
    m_z += fz * forward + rz * right;
}

void Camera::viewMatrix(float* out) const {
    const float fx = std::cos(m_pitch) * std::sin(m_yaw);
    const float fy = std::sin(m_pitch);
    const float fz = std::cos(m_pitch) * std::cos(m_yaw);
    const bx::Vec3 eye = {m_x, m_y, m_z};
    const bx::Vec3 at  = {m_x + fx, m_y + fy, m_z + fz};
    bx::mtxLookAt(out, eye, at);
}

void Camera::projMatrix(float* out, float aspect, bool homogeneousDepth) const {
    bx::mtxProj(out, kFovDegrees, aspect, kNear, kFar, homogeneousDepth);
}
