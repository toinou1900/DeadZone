#include "scene.h"

#include "physics.h"
#include "renderer.h"
#include "transform_math.h"

#include <algorithm>

namespace {

// Systeme : fait tourner les entites qui ont Transform + Spin (et pas de corps physique, sinon c'est la physique qui decide).
void spinSystem(entt::registry& registry, float dt) {
    registry.view<Transform, Spin>(entt::exclude<RigidBody>).each([dt](Transform& t, const Spin& s) {
        float dq[4], q[4];
        quatFromRotationVector(s.speed, dt, dq);
        quatMul(dq, t.rotation, q);
        quatNormalize(q);
        for (int i = 0; i < 4; ++i) t.rotation[i] = q[i];
    });
}

// Systeme : cree dans le moteur physique le corps des entites qui n'en ont pas encore.
void createBodiesSystem(entt::registry& registry, Physics& physics) {
    registry.view<Transform, RigidBody>().each([&](const Transform& t, RigidBody& rb) {
        if (rb.body != RigidBody::kNoBody) return;
        BoxBodyDesc d;
        for (int i = 0; i < 3; ++i) {
            d.position[i]        = t.position[i];
            d.halfExtents[i]     = rb.halfExtents[i] * t.scale[i]; // la boite de collision suit l'echelle de l'entite
            d.linearVelocity[i]  = rb.linearVelocity[i];
            d.angularVelocity[i] = rb.angularVelocity[i];
        }
        for (int i = 0; i < 4; ++i) d.rotation[i] = t.rotation[i];
        d.dynamic     = rb.dynamic;
        d.friction    = rb.friction;
        d.restitution = rb.restitution;
        rb.body = physics.createBox(d);
    });
}

// Systeme : recopie la position et l'orientation calculees par la physique dans le Transform (corps dynamiques).
void syncTransformsSystem(entt::registry& registry, const Physics& physics) {
    registry.view<Transform, RigidBody>().each([&](Transform& t, const RigidBody& rb) {
        if (rb.dynamic && rb.body != RigidBody::kNoBody) physics.getTransform(rb.body, t.position, t.rotation);
    });
}

// Systeme : dessine les entites qui ont Transform + un composant de rendu.
void renderSystem(entt::registry& registry, Renderer& renderer) {
    float world[16];
    registry.view<Transform, MeshRenderer>().each([&](const Transform& t, const MeshRenderer& mr) {
        if (!mr.model || !mr.model->valid()) return;
        transformToMatrix(t, world);
        renderer.drawModel(*mr.model, world);
    });
    // CubeRenderer est un composant vide : EnTT ne le passe pas a la fonction, seulement Transform.
    registry.view<Transform, CubeRenderer>().each([&](const Transform& t) {
        transformToMatrix(t, world);
        renderer.drawCube(world);
    });
    registry.view<Transform, BoxRenderer>().each([&](const Transform& t, const BoxRenderer& b) {
        transformToMatrix(t, world);
        renderer.drawBox(world, b.color, b.checker, b.repeat);
    });
}

void setPosition(Transform& t, float x, float y, float z) {
    t.position[0] = x; t.position[1] = y; t.position[2] = z;
}

} // namespace

void Scene::setPhysics(Physics* physics) {
    m_physics = physics;
    if (!m_hookConnected) {
        // Quand une entite qui a un RigidBody est detruite, son corps physique l'est aussi.
        m_registry.on_destroy<RigidBody>().connect<&Scene::onRigidBodyDestroyed>(this);
        m_hookConnected = true;
    }
}

void Scene::onRigidBodyDestroyed(entt::registry& registry, entt::entity entity) {
    RigidBody& rb = registry.get<RigidBody>(entity);
    if (m_physics && rb.body != RigidBody::kNoBody) m_physics->destroyBody(rb.body);
    rb.body = RigidBody::kNoBody;
}

entt::entity Scene::createFloor() {
    const entt::entity e = m_registry.create();
    m_registry.emplace<Name>(e, Name{"Sol"});
    Transform t;
    setPosition(t, 0.0f, -2.5f, 0.0f);
    t.scale[0] = 30.0f; t.scale[1] = 0.5f; t.scale[2] = 30.0f; // boite de 60 x 1 x 60, dessus a y = -2
    m_registry.emplace<Transform>(e, t);
    RigidBody rb;
    rb.dynamic     = false;
    rb.friction    = 0.6f;
    rb.restitution = 0.2f;
    m_registry.emplace<RigidBody>(e, rb);
    BoxRenderer box;
    box.color[0] = 0.45f; box.color[1] = 0.50f; box.color[2] = 0.62f;
    box.checker = true;
    box.repeat  = 30.0f;
    m_registry.emplace<BoxRenderer>(e, box);
    return e;
}

entt::entity Scene::createCube(const std::string& name, float x, float y, float z) {
    const entt::entity e = m_registry.create();
    m_registry.emplace<Name>(e, Name{name});
    Transform t;
    setPosition(t, x, y, z);
    m_registry.emplace<Transform>(e, t);
    RigidBody rb;
    rb.restitution = 0.4f;
    rb.angularVelocity[0] = 0.6f; rb.angularVelocity[1] = 0.9f; rb.angularVelocity[2] = 0.3f; // il bascule en tombant
    m_registry.emplace<RigidBody>(e, rb);
    m_registry.emplace<CubeRenderer>(e);
    return e;
}

entt::entity Scene::createModel(std::shared_ptr<Model> model, const std::string& name, float x, float y, float z) {
    const entt::entity e = m_registry.create();
    m_registry.emplace<Name>(e, Name{name});
    Transform t;
    setPosition(t, x, y, z);
    m_registry.emplace<Transform>(e, t);
    RigidBody rb; // collision : boite englobante du modele
    for (int i = 0; i < 3; ++i) rb.halfExtents[i] = model->halfExtents()[i];
    rb.angularVelocity[1] = 0.8f;
    m_registry.emplace<RigidBody>(e, rb);
    m_registry.emplace<MeshRenderer>(e, MeshRenderer{std::move(model)});
    m_models.push_back(e);
    m_spawned.push_back(e);
    return e;
}

entt::entity Scene::createBox(const float position[3], const float velocity[3], const float angularVelocity[3],
                              float halfSize, const float color[4]) {
    const entt::entity e = m_registry.create();
    m_registry.emplace<Name>(e, Name{"Boite"});
    Transform t;
    setPosition(t, position[0], position[1], position[2]);
    t.scale[0] = t.scale[1] = t.scale[2] = halfSize; // la boite de base mesure 2 x 2 x 2
    m_registry.emplace<Transform>(e, t);
    RigidBody rb;
    for (int i = 0; i < 3; ++i) {
        rb.linearVelocity[i]  = velocity[i];
        rb.angularVelocity[i] = angularVelocity[i];
    }
    m_registry.emplace<RigidBody>(e, rb);
    BoxRenderer box;
    for (int i = 0; i < 4; ++i) box.color[i] = color[i];
    m_registry.emplace<BoxRenderer>(e, box);
    m_spawned.push_back(e);
    return e;
}

bool Scene::destroyLastSpawned() {
    if (m_spawned.empty()) return false;
    const entt::entity e = m_spawned.back();
    m_spawned.pop_back();
    m_models.erase(std::remove(m_models.begin(), m_models.end(), e), m_models.end());
    m_registry.destroy(e);
    return true;
}

void Scene::clear() {
    // On retire explicitement les corps physiques d'abord : fonctionne meme si la scene n'a pas (ou plus) de crochet.
    if (m_physics) {
        m_registry.view<RigidBody>().each([&](RigidBody& rb) {
            if (rb.body != RigidBody::kNoBody) m_physics->destroyBody(rb.body);
            rb.body = RigidBody::kNoBody;
        });
    }
    m_registry.clear();
    m_models.clear();
    m_spawned.clear();
}

void Scene::update(float dt) {
    spinSystem(m_registry, dt);
}

void Scene::updatePhysics(float dt) {
    if (!m_physics) return;
    createBodiesSystem(m_registry, *m_physics);
    m_physics->update(dt);
    syncTransformsSystem(m_registry, *m_physics);
}

void Scene::render(Renderer& renderer) {
    renderSystem(m_registry, renderer);
}
