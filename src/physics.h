// DeadZone Engine - moteur physique (Jolt Physics) : corps rigides en boite, gravite, collisions
#pragma once

#include <cstdint>
#include <memory>

struct BoxBodyDesc {
    float position[3]        = {0.0f, 0.0f, 0.0f};
    float rotation[4]        = {0.0f, 0.0f, 0.0f, 1.0f}; // quaternion x, y, z, w
    float halfExtents[3]     = {0.5f, 0.5f, 0.5f};
    bool  dynamic            = true;                      // false : corps fixe
    float friction           = 0.5f;
    float restitution        = 0.3f;
    float linearVelocity[3]  = {0.0f, 0.0f, 0.0f};
    float angularVelocity[3] = {0.0f, 0.0f, 0.0f};
};

class Physics {
public:
    static constexpr uint32_t kInvalidBody = 0xFFFFFFFFu;

    Physics();
    ~Physics();
    Physics(const Physics&)            = delete;
    Physics& operator=(const Physics&) = delete;

    bool init();
    void shutdown(); // detruit tous les corps

    // Cree un corps en boite. Retourne kInvalidBody si le moteur est plein.
    uint32_t createBox(const BoxBodyDesc& desc);
    void     destroyBody(uint32_t body);

    // Fait avancer la simulation de `dt` secondes reelles, par pas fixes de 1/60 s (resultat reproductible,
    // quel que soit le nombre d'images par seconde).
    void update(float dt);

    // Position et orientation (quaternion x, y, z, w) d'un corps. Retourne false si le corps n'existe pas.
    bool getTransform(uint32_t body, float position[3], float rotation[4]) const;

    uint32_t bodyCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
