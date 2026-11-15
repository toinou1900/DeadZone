// DeadZone Engine - la scene : un registre ECS (EnTT) + les systemes qui le font vivre
#pragma once

#include "components.h"

#include <entt/entt.hpp>

#include <memory>
#include <string>
#include <vector>

class Physics;
class Renderer;

class Scene {
public:
    Scene() = default;
    Scene(const Scene&)            = delete;
    Scene& operator=(const Scene&) = delete;

    // Moteur physique utilise par updatePhysics(). Doit rester valide tant que la scene contient des corps.
    void setPhysics(Physics* physics);

    // --- Creation d'entites (tous les objets "dynamiques" tombent et rebondissent)
    entt::entity createFloor();
    entt::entity createCube(const std::string& name, float x, float y, float z);
    entt::entity createModel(std::shared_ptr<Model> model, const std::string& name, float x, float y, float z);
    entt::entity createBox(const float position[3], const float velocity[3], const float angularVelocity[3],
                           float halfSize, const float color[4]);

    // Supprime le dernier objet ajoute par l'utilisateur (modele ou boite lancee). Retourne false s'il n'y en a pas.
    bool destroyLastSpawned();
    void clear(); // detruit toutes les entites (et leurs corps physiques)

    // --- Systemes
    void update(float dt);        // animations (Spin) des entites sans corps physique
    void updatePhysics(float dt); // cree les corps manquants, avance la simulation, recopie les positions
    void render(Renderer& renderer);

    // --- Acces
    size_t entityCount() const { return m_registry.view<Transform>().size(); }
    const std::vector<entt::entity>& models() const { return m_models; } // dans l'ordre de creation
    const entt::registry& registry() const { return m_registry; }

private:
    void onRigidBodyDestroyed(entt::registry& registry, entt::entity entity);
    void removeEntity(entt::entity entity); // detruit l'entite et la retire des listes

    entt::registry            m_registry;
    std::vector<entt::entity> m_models;  // modeles 3D charges (pour l'affichage)
    std::vector<entt::entity> m_spawned; // objets ajoutes par l'utilisateur, dans l'ordre (pour "retirer le dernier")
    Physics*                  m_physics = nullptr;
    bool                      m_hookConnected = false;
};
