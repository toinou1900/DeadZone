// DeadZone Engine - la scene : un registre ECS (EnTT) + les systemes qui le font vivre
#pragma once

#include "components.h"

#include <entt/entt.hpp>

#include <memory>
#include <string>
#include <vector>

class Renderer;

class Scene {
public:
    // --- Creation d'entites
    entt::entity createCube(const std::string& name, float x, float y, float z);
    entt::entity createModel(std::shared_ptr<Model> model, const std::string& name, float x, float y, float z);

    // Supprime le dernier modele ajoute (les cubes ne sont pas concernes). Retourne false s'il n'y en a pas.
    bool destroyLastModel();
    void clear(); // detruit toutes les entites (les modeles sont liberes quand plus aucune entite ne les utilise)

    // --- Systemes
    void update(float dt);       // fait tourner les entites qui ont un composant Spin
    void render(Renderer& renderer);

    // --- Acces
    size_t entityCount() const { return m_registry.view<Transform>().size(); }
    const std::vector<entt::entity>& models() const { return m_models; } // dans l'ordre de creation
    const entt::registry& registry() const { return m_registry; }

private:
    entt::registry            m_registry;
    std::vector<entt::entity> m_models;
};
