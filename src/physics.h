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

// Personnage en capsule. `position` = position des PIEDS.
struct CharacterDesc {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float radius      = 0.4f;
    float halfHeight  = 0.5f; // demi-hauteur du cylindre ; hauteur totale = 2 * (halfHeight + radius)
    float mass        = 70.0f;
    // Force maximale (newtons) avec laquelle il peut pousser les objets : un caisson leger bouge, un gros bloc non.
    float maxPushForce = 3000.0f;
};

class Physics {
public:
    static constexpr uint32_t kInvalidBody = 0xFFFFFFFFu;
    static constexpr uint32_t kMaxBodies   = 10240; // nombre maximal de corps simultanes (fixe a l'initialisation)

    Physics();
    ~Physics();
    Physics(const Physics&)            = delete;
    Physics& operator=(const Physics&) = delete;

    bool init();
    void shutdown(); // detruit tous les corps

    // Cree un corps en boite. Retourne kInvalidBody si le moteur est plein (kMaxBodies corps).
    uint32_t createBox(const BoxBodyDesc& desc);
    void     destroyBody(uint32_t body);

    // Fait avancer la simulation de `dt` secondes reelles, par pas fixes de 1/60 s (resultat reproductible,
    // quel que soit le nombre d'images par seconde). Retourne le nombre de pas effectues (souvent 0 a haut FPS).
    int update(float dt);

    // Fraction (0..1) du pas suivant deja ecoulee : sert a interpoler l'affichage entre les deux derniers etats.
    float interpolationAlpha() const;

    // Position et orientation (quaternion x, y, z, w) d'un corps. Retourne false si le corps n'existe pas.
    bool getTransform(uint32_t body, float position[3], float rotation[4]) const;

    uint32_t bodyCount() const;

    // ---- Personnages (capsule qui marche, saute, pousse les objets ; ne bascule jamais)
    // Un personnage est aussi un corps : il compte dans bodyCount(). Retourne kInvalidBody si le moteur est plein.
    uint32_t createCharacter(const CharacterDesc& desc);
    void     destroyCharacter(uint32_t character);

    // Applique une commande de deplacement : vitesse horizontale souhaitee (m/s, repere du monde), saut eventuel.
    // Au sol, la vitesse est appliquee directement ; en l'air elle est rejointe progressivement (`airControl`).
    // A appeler a chaque image, avant update(). La commande est appliquee a chaque pas de simulation.
    // Le personnage est "cinematique" (Jolt CharacterVirtual) : les objets ne le repoussent pas, il glisse le long
    // des obstacles, monte les marches et pousse les objets avec une force limitee.
    void moveCharacter(uint32_t character, float vx, float vz, bool jump, float jumpSpeed, float airControl);

    bool characterOnGround(uint32_t character) const;
    bool getCharacterPosition(uint32_t character, float feet[3]) const; // position des pieds
    void teleportCharacter(uint32_t character, const float feet[3]);    // deplace les pieds, annule la vitesse

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};
