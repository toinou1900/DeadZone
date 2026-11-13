#include "scene.h"

#include "renderer.h"

#include <bx/math.h>

namespace {

void toMatrix(const Transform& t, float* out) {
    bx::mtxSRT(out,
               t.scale[0],    t.scale[1],    t.scale[2],
               t.rotation[0], t.rotation[1], t.rotation[2],
               t.position[0], t.position[1], t.position[2]);
}

// Systeme : fait avancer la rotation de chaque entite qui a Transform + Spin.
void spinSystem(entt::registry& registry, float dt) {
    registry.view<Transform, Spin>().each([dt](Transform& t, const Spin& s) {
        for (int i = 0; i < 3; ++i) t.rotation[i] += s.speed[i] * dt;
    });
}

// Systeme : dessine chaque entite qui a Transform + MeshRenderer (ou CubeRenderer).
void renderSystem(entt::registry& registry, Renderer& renderer) {
    float world[16];
    registry.view<Transform, MeshRenderer>().each([&](const Transform& t, const MeshRenderer& mr) {
        if (!mr.model || !mr.model->valid()) return;
        toMatrix(t, world);
        renderer.drawModel(*mr.model, world);
    });
    // CubeRenderer est un composant vide : EnTT ne le passe pas a la fonction, seulement Transform.
    registry.view<Transform, CubeRenderer>().each([&](const Transform& t) {
        toMatrix(t, world);
        renderer.drawCube(world);
    });
}

} // namespace

entt::entity Scene::createCube(const std::string& name, float x, float y, float z) {
    const entt::entity e = m_registry.create();
    m_registry.emplace<Name>(e, Name{name});
    Transform t;
    t.position[0] = x; t.position[1] = y; t.position[2] = z;
    m_registry.emplace<Transform>(e, t);
    Spin spin;
    spin.speed[0] = 0.6f; spin.speed[1] = 0.9f;
    m_registry.emplace<Spin>(e, spin);
    m_registry.emplace<CubeRenderer>(e);
    return e;
}

entt::entity Scene::createModel(std::shared_ptr<Model> model, const std::string& name, float x, float y, float z) {
    const entt::entity e = m_registry.create();
    m_registry.emplace<Name>(e, Name{name});
    Transform t;
    t.position[0] = x; t.position[1] = y; t.position[2] = z;
    m_registry.emplace<Transform>(e, t);
    Spin spin;
    spin.speed[1] = 0.5f; // plateau tournant
    m_registry.emplace<Spin>(e, spin);
    m_registry.emplace<MeshRenderer>(e, MeshRenderer{std::move(model)});
    m_models.push_back(e);
    return e;
}

bool Scene::destroyLastModel() {
    if (m_models.empty()) return false;
    m_registry.destroy(m_models.back());
    m_models.pop_back();
    return true;
}

void Scene::clear() {
    m_registry.clear();
    m_models.clear();
}

void Scene::update(float dt) {
    spinSystem(m_registry, dt);
}

void Scene::render(Renderer& renderer) {
    renderSystem(m_registry, renderer);
}
