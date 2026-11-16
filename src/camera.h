// DeadZone Engine - camera libre (position + yaw/pitch)
#pragma once

class Camera {
public:
    // Tourne la camera. Les valeurs sont des deplacements de souris en pixels.
    void look(float dxPixels, float dyPixels);

    // Deplace la camera. `forward` suit la direction du regard (haut/bas inclus),
    // `right` reste horizontal, `up` est vertical. Valeurs deja multipliees par vitesse * dt.
    void move(float forward, float right, float up);

    // Place la camera (mode joueur : au niveau des yeux du personnage).
    void setPosition(float x, float y, float z) { m_x = x; m_y = y; m_z = z; }
    float yaw() const { return m_yaw; }

    // Position de la camera et direction du regard (vecteur unitaire).
    void position(float out[3]) const { out[0] = m_x; out[1] = m_y; out[2] = m_z; }
    void forward(float out[3]) const;

    // Matrices pour bgfx (tableaux de 16 floats).
    void viewMatrix(float* out) const;
    void projMatrix(float* out, float aspect, bool homogeneousDepth) const;

private:
    float m_x = 0.0f, m_y = 1.0f, m_z = -6.0f;
    float m_yaw   = 0.0f; // rotation autour de Y (radians)
    float m_pitch = 0.0f; // haut / bas (radians)
};
