// DeadZone Engine - composants ECS : de simples donnees, sans logique.
// Une entite = un identifiant auquel on attache des composants (voir scene.h).
#pragma once

#include "model.h"

#include <memory>
#include <string>

// Nom lisible (affichage, debug).
struct Name {
    std::string value;
};

// Position / rotation (Euler, radians) / echelle dans le monde.
struct Transform {
    float position[3] = {0.0f, 0.0f, 0.0f};
    float rotation[3] = {0.0f, 0.0f, 0.0f};
    float scale[3]    = {1.0f, 1.0f, 1.0f};
};

// Rotation continue, en radians par seconde, autour de chaque axe.
struct Spin {
    float speed[3] = {0.0f, 0.0f, 0.0f};
};

// Dessine un modele 3D. Le modele est partage : plusieurs entites peuvent utiliser le meme.
struct MeshRenderer {
    std::shared_ptr<Model> model;
};

// Dessine le cube de test (composant "etiquette", sans donnee).
struct CubeRenderer {};
