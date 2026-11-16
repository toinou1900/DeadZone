// DeadZone Engine - petites fonctions de maths pour les transformations (quaternions, matrices)
#pragma once

#include "components.h"

#include <cmath>

// Convention des matrices bgfx : tableau de 16 floats, m[colonne * 4 + ligne], translation en m[12..14].
// Un point se transforme donc par v' = R * S * v + t, avec R la rotation "classique" d'un quaternion.

// Quaternions (x, y, z, w)
inline void quatMul(const float a[4], const float b[4], float out[4]) { // out = a * b (applique b, puis a)
    out[0] = a[3] * b[0] + a[0] * b[3] + a[1] * b[2] - a[2] * b[1];
    out[1] = a[3] * b[1] - a[0] * b[2] + a[1] * b[3] + a[2] * b[0];
    out[2] = a[3] * b[2] + a[0] * b[1] - a[1] * b[0] + a[2] * b[3];
    out[3] = a[3] * b[3] - a[0] * b[0] - a[1] * b[1] - a[2] * b[2];
}

inline void quatNormalize(float q[4]) {
    const float len = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    if (len > 0.0f) for (int i = 0; i < 4; ++i) q[i] /= len;
    else { q[0] = q[1] = q[2] = 0.0f; q[3] = 1.0f; }
}

// Interpolation lineaire normalisee entre deux quaternions (par le chemin le plus court).
inline void quatNlerp(const float a[4], const float b[4], float t, float out[4]) {
    const float dot  = a[0] * b[0] + a[1] * b[1] + a[2] * b[2] + a[3] * b[3];
    const float sign = dot < 0.0f ? -1.0f : 1.0f;
    for (int i = 0; i < 4; ++i) out[i] = a[i] * (1.0f - t) + b[i] * sign * t;
    quatNormalize(out);
}

// Quaternion d'une rotation de vecteur `w` (axe = direction, angle = longueur, en radians) multipliee par dt.
inline void quatFromRotationVector(const float w[3], float dt, float out[4]) {
    const float len = std::sqrt(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
    const float angle = len * dt;
    if (len < 1e-8f || angle < 1e-8f) { out[0] = out[1] = out[2] = 0.0f; out[3] = 1.0f; return; }
    const float s = std::sin(0.5f * angle) / len;
    out[0] = w[0] * s; out[1] = w[1] * s; out[2] = w[2] * s; out[3] = std::cos(0.5f * angle);
}

// Matrice monde (16 floats) d'une Transform : echelle, puis rotation, puis translation.
inline void transformToMatrix(const Transform& t, float* m) {
    const float x = t.rotation[0], y = t.rotation[1], z = t.rotation[2], w = t.rotation[3];
    const float xx = x * x, yy = y * y, zz = z * z;
    const float xy = x * y, xz = x * z, yz = y * z, wx = w * x, wy = w * y, wz = w * z;
    const float r[3][3] = {
        {1.0f - 2.0f * (yy + zz), 2.0f * (xy - wz),        2.0f * (xz + wy)},
        {2.0f * (xy + wz),        1.0f - 2.0f * (xx + zz), 2.0f * (yz - wx)},
        {2.0f * (xz - wy),        2.0f * (yz + wx),        1.0f - 2.0f * (xx + yy)},
    };
    for (int c = 0; c < 3; ++c) {
        for (int row = 0; row < 3; ++row) m[c * 4 + row] = r[row][c] * t.scale[c];
        m[c * 4 + 3] = 0.0f;
    }
    m[12] = t.position[0]; m[13] = t.position[1]; m[14] = t.position[2]; m[15] = 1.0f;
}
