// DeadZone Engine - composants ECS : de simples donnees, sans logique.
// Une entite = un identifiant auquel on attache des composants (voir scene.h).
#pragma once

#include "model.h"

#include <cstdint>
#include <memory>
#include <string>

// Nom lisible (affichage, debug).
struct Name {
    std::string value;
};

// Position / orientation (quaternion x, y, z, w) / echelle dans le monde.
struct Transform {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float scale[3]    = {1.0f, 1.0f, 1.0f};
};

// Rotation continue, en radians par seconde, autour de chaque axe (ignoree pour les entites qui ont un RigidBody).
struct Spin {
    float speed[3] = {0.0f, 0.0f, 0.0f};
};

// Corps physique (collision en boite). Le corps est cree dans le moteur physique au premier tour de
// boucle ; ensuite, pour un corps dynamique, c'est la physique qui pilote le Transform de l'entite.
struct RigidBody {
    static constexpr uint32_t kNoBody = 0xFFFFFFFFu;

    bool     dynamic         = true;                    // false : fixe (sol, murs)
    float    halfExtents[3]  = {1.0f, 1.0f, 1.0f};      // demi-dimensions de la boite, avant echelle du Transform
    float    friction        = 0.5f;
    float    restitution     = 0.3f;                    // rebond : 0 = aucun, 1 = parfait
    float    linearVelocity[3]  = {0.0f, 0.0f, 0.0f};   // vitesse initiale
    float    angularVelocity[3] = {0.0f, 0.0f, 0.0f};   // rotation initiale (rad/s)
    uint32_t body            = kNoBody;                 // identifiant dans le moteur physique (rempli par le systeme)
};

// Dessine un modele 3D. Le modele est partage : plusieurs entites peuvent utiliser le meme.
struct MeshRenderer {
    std::shared_ptr<Model> model;
};

// Dessine le cube de test, colore par sommets (composant "etiquette", sans donnee).
struct CubeRenderer {};

// Dessine une boite eclairee de couleur unie (taille = Transform.scale). `checker` : motif en damier,
// repete `repeat` fois, pour mieux voir le mouvement (sol).
struct BoxRenderer {
    float color[4] = {0.8f, 0.8f, 0.8f, 1.0f};
    bool  checker  = false;
    float repeat   = 1.0f;
};
