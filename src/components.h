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

// Deux derniers etats calcules par la physique (elle avance par pas fixes de 1/60 s). On dessine un etat
// interpole entre les deux : sans cela, a 200 FPS le mouvement avancerait par a-coups de 1/60 s.
struct PhysicsState {
    bool  valid           = false;
    float prevPosition[3] = {0.0f, 0.0f, 0.0f};
    float prevRotation[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    float position[3]     = {0.0f, 0.0f, 0.0f};
    float rotation[4]     = {0.0f, 0.0f, 0.0f, 1.0f};
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
    PhysicsState state;                                 // rempli par le systeme (interpolation)
};

// Personnage physique en capsule (un joueur, un PNJ...). Le Transform de l'entite est le centre de la capsule.
// Hauteur totale = 2 * (halfHeight + radius). Le personnage marche, saute, pousse les objets et ne tombe pas du sol.
struct CharacterBody {
    static constexpr uint32_t kNoBody = 0xFFFFFFFFu;

    float    radius     = 0.4f;
    float    halfHeight = 0.5f;   // demi-hauteur de la partie cylindrique (entre les deux demi-spheres)
    float    mass       = 70.0f;
    uint32_t body       = kNoBody; // identifiant dans le moteur physique (rempli par le systeme)
    PhysicsState state;
};

// Controle d'un personnage par le joueur : l'entree (clavier) est ecrite dans les champs `input*`, le systeme
// transforme cela en deplacement. Necessite un CharacterBody.
struct Player {
    float walkSpeed   = 5.0f;   // m/s
    float sprintSpeed = 9.0f;
    float jumpSpeed   = 5.0f;   // vitesse verticale au saut (hauteur ~ v^2 / 2g = 1,3 m)
    float airControl  = 3.0f;   // 0 = aucun controle en l'air ; plus grand = plus reactif
    float eyeHeight   = 1.6f;   // hauteur des yeux au-dessus des pieds
    float spawn[3]    = {0.0f, 0.0f, 0.0f}; // position des pieds a la reapparition (si on tombe du monde)

    // Entree (ecrite par le moteur a chaque image)
    float inputForward = 0.0f;  // -1..1 (avant / arriere)
    float inputRight   = 0.0f;  // -1..1
    float inputYaw     = 0.0f;  // direction du regard (radians)
    bool  inputSprint  = false;
    bool  inputJump    = false;

    bool  onGround     = false; // resultat : le joueur est-il au sol ?
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
