#include "scene.h"

#include "physics.h"
#include "renderer.h"
#include "transform_math.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kFallLimit = -40.0f; // sous cette hauteur (pieds), le joueur reapparait

void setPosition(Transform& t, float x, float y, float z) {
    t.position[0] = x; t.position[1] = y; t.position[2] = z;
}

// ------------------------------------------------------------------ Interpolation
// La physique avance par pas fixes de 1/60 s ; on garde les deux derniers etats et on dessine entre les deux.
void pushState(PhysicsState& s, const float position[3], const float rotation[4], bool newStep) {
    if (!s.valid) { // premier etat : rien a interpoler
        for (int i = 0; i < 3; ++i) s.prevPosition[i] = s.position[i] = position[i];
        for (int i = 0; i < 4; ++i) s.prevRotation[i] = s.rotation[i] = rotation[i];
        s.valid = true;
    } else if (newStep) {
        for (int i = 0; i < 3; ++i) { s.prevPosition[i] = s.position[i]; s.position[i] = position[i]; }
        for (int i = 0; i < 4; ++i) { s.prevRotation[i] = s.rotation[i]; s.rotation[i] = rotation[i]; }
    }
}

void applyState(const PhysicsState& s, float alpha, Transform& t) {
    for (int i = 0; i < 3; ++i) t.position[i] = s.prevPosition[i] + (s.position[i] - s.prevPosition[i]) * alpha;
    quatNlerp(s.prevRotation, s.rotation, alpha, t.rotation);
}

// ------------------------------------------------------------------ Systemes
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
// Retourne les entites dont le corps n'a pas pu etre cree (moteur plein) : sans corps, elles resteraient figees.
std::vector<entt::entity> createBodiesSystem(entt::registry& registry, Physics& physics) {
    std::vector<entt::entity> failed;
    registry.view<Transform, RigidBody>().each([&](entt::entity entity, const Transform& t, RigidBody& rb) {
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
        rb.state.valid = false;
        if (rb.body == RigidBody::kNoBody) failed.push_back(entity);
    });
    registry.view<Transform, CharacterBody>().each([&](entt::entity entity, const Transform& t, CharacterBody& cb) {
        if (cb.body != CharacterBody::kNoBody) return;
        CharacterDesc d;
        d.position[0] = t.position[0];
        d.position[1] = t.position[1] - (cb.halfHeight + cb.radius); // le Transform est le centre ; Jolt veut les pieds
        d.position[2] = t.position[2];
        d.radius = cb.radius; d.halfHeight = cb.halfHeight; d.mass = cb.mass;
        cb.body = physics.createCharacter(d);
        cb.state.valid = false;
        if (cb.body == CharacterBody::kNoBody) failed.push_back(entity);
    });
    return failed;
}

// Systeme : transforme l'entree du joueur en commande de deplacement du personnage.
void playerSystem(entt::registry& registry, Physics& physics, float dt) {
    registry.view<CharacterBody, Player>().each([&](CharacterBody& cb, Player& p) {
        if (cb.body == CharacterBody::kNoBody) return;
        // Direction horizontale dans le repere du monde (meme convention que la camera : avant = (sin yaw, cos yaw))
        const float sy = std::sin(p.inputYaw), cy = std::cos(p.inputYaw);
        float dx = p.inputForward * sy + p.inputRight * cy;
        float dz = p.inputForward * cy - p.inputRight * sy;
        const float len = std::sqrt(dx * dx + dz * dz);
        if (len > 1.0f) { dx /= len; dz /= len; } // pas plus vite en diagonale
        const float speed = p.inputSprint ? p.sprintSpeed : p.walkSpeed;
        physics.moveCharacter(cb.body, dx * speed, dz * speed, p.inputJump, p.jumpSpeed, p.airControl, dt);
        p.onGround = physics.characterOnGround(cb.body);

        float feet[3];
        if (physics.getCharacterPosition(cb.body, feet) && feet[1] < kFallLimit) { // tombe hors du monde : reapparition
            physics.teleportCharacter(cb.body, p.spawn);
            cb.state.valid = false;
        }
    });
}

// Systeme : recopie la position et l'orientation calculees par la physique dans le Transform,
// en interpolant entre les deux derniers pas de simulation.
void syncTransformsSystem(entt::registry& registry, const Physics& physics, int steps, float alpha) {
    const bool newStep = steps > 0;
    registry.view<Transform, RigidBody>().each([&](Transform& t, RigidBody& rb) {
        if (!rb.dynamic || rb.body == RigidBody::kNoBody) return;
        float p[3], r[4];
        if (!physics.getTransform(rb.body, p, r)) return;
        pushState(rb.state, p, r, newStep);
        applyState(rb.state, alpha, t);
    });
    registry.view<Transform, CharacterBody>().each([&](Transform& t, CharacterBody& cb) {
        float feet[3];
        if (cb.body == CharacterBody::kNoBody || !physics.getCharacterPosition(cb.body, feet)) return;
        const float center[3] = {feet[0], feet[1] + cb.halfHeight + cb.radius, feet[2]};
        const float identity[4] = {0.0f, 0.0f, 0.0f, 1.0f}; // le personnage ne tourne jamais
        pushState(cb.state, center, identity, newStep);
        applyState(cb.state, alpha, t);
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

} // namespace

// ------------------------------------------------------------------ Scene
void Scene::setPhysics(Physics* physics) {
    m_physics = physics;
    if (!m_hookConnected) {
        // Quand une entite qui a un corps physique est detruite, son corps l'est aussi.
        m_registry.on_destroy<RigidBody>().connect<&Scene::onRigidBodyDestroyed>(this);
        m_registry.on_destroy<CharacterBody>().connect<&Scene::onCharacterDestroyed>(this);
        m_hookConnected = true;
    }
}

void Scene::onRigidBodyDestroyed(entt::registry& registry, entt::entity entity) {
    RigidBody& rb = registry.get<RigidBody>(entity);
    if (m_physics && rb.body != RigidBody::kNoBody) m_physics->destroyBody(rb.body);
    rb.body = RigidBody::kNoBody;
}

void Scene::onCharacterDestroyed(entt::registry& registry, entt::entity entity) {
    CharacterBody& cb = registry.get<CharacterBody>(entity);
    if (m_physics && cb.body != CharacterBody::kNoBody) m_physics->destroyCharacter(cb.body);
    cb.body = CharacterBody::kNoBody;
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

entt::entity Scene::createPlayer(float x, float feetY, float z) {
    const entt::entity e = m_registry.create();
    m_registry.emplace<Name>(e, Name{"Joueur"});
    CharacterBody cb;
    Transform t;
    setPosition(t, x, feetY + cb.halfHeight + cb.radius, z); // le Transform est le centre de la capsule
    t.scale[0] = cb.radius; t.scale[1] = cb.halfHeight + cb.radius; t.scale[2] = cb.radius; // taille de la boite d'affichage
    m_registry.emplace<Transform>(e, t);
    m_registry.emplace<CharacterBody>(e, cb);
    Player player;
    player.spawn[0] = x; player.spawn[1] = feetY; player.spawn[2] = z;
    m_registry.emplace<Player>(e, player);
    m_player = e;
    return e;
}

bool Scene::destroyLastSpawned() {
    if (m_spawned.empty()) return false;
    removeEntity(m_spawned.back());
    return true;
}

void Scene::removeEntity(entt::entity entity) {
    m_spawned.erase(std::remove(m_spawned.begin(), m_spawned.end(), entity), m_spawned.end());
    m_models.erase(std::remove(m_models.begin(), m_models.end(), entity), m_models.end());
    if (entity == m_player) m_player = entt::null;
    m_registry.destroy(entity);
}

void Scene::clear() {
    // On retire explicitement les corps physiques d'abord : fonctionne meme si la scene n'a pas (ou plus) de crochet.
    if (m_physics) {
        m_registry.view<RigidBody>().each([&](RigidBody& rb) {
            if (rb.body != RigidBody::kNoBody) m_physics->destroyBody(rb.body);
            rb.body = RigidBody::kNoBody;
        });
        m_registry.view<CharacterBody>().each([&](CharacterBody& cb) {
            if (cb.body != CharacterBody::kNoBody) m_physics->destroyCharacter(cb.body);
            cb.body = CharacterBody::kNoBody;
        });
    }
    m_registry.clear();
    m_models.clear();
    m_spawned.clear();
    m_player = entt::null;
}

// ------------------------------------------------------------------ Joueur
void Scene::setPlayerInput(float forward, float right, float yaw, bool sprint, bool jump) {
    if (m_player == entt::null || !m_registry.valid(m_player)) return;
    Player& p = m_registry.get<Player>(m_player);
    p.inputForward = forward; p.inputRight = right; p.inputYaw = yaw;
    p.inputSprint = sprint;   p.inputJump = jump;
}

bool Scene::playerEyePosition(float out[3]) const {
    if (m_player == entt::null || !m_registry.valid(m_player)) return false;
    const Transform&     t  = m_registry.get<Transform>(m_player);
    const CharacterBody& cb = m_registry.get<CharacterBody>(m_player);
    const Player&        p  = m_registry.get<Player>(m_player);
    out[0] = t.position[0];
    out[1] = t.position[1] - (cb.halfHeight + cb.radius) + p.eyeHeight; // pieds + hauteur des yeux
    out[2] = t.position[2];
    return true;
}

bool Scene::playerOnGround() const {
    return m_player != entt::null && m_registry.valid(m_player) && m_registry.get<Player>(m_player).onGround;
}

float Scene::playerEyeHeight() const {
    return (m_player != entt::null && m_registry.valid(m_player)) ? m_registry.get<Player>(m_player).eyeHeight : 0.0f;
}

void Scene::teleportPlayer(const float feet[3]) {
    if (m_player == entt::null || !m_registry.valid(m_player)) return;
    Transform&     t  = m_registry.get<Transform>(m_player);
    CharacterBody& cb = m_registry.get<CharacterBody>(m_player);
    setPosition(t, feet[0], feet[1] + cb.halfHeight + cb.radius, feet[2]);
    cb.state.valid = false;
    if (m_physics && cb.body != CharacterBody::kNoBody) m_physics->teleportCharacter(cb.body, feet);
}

void Scene::setPlayerVisible(bool visible) {
    if (m_player == entt::null || !m_registry.valid(m_player)) return;
    if (visible) {
        BoxRenderer box;
        box.color[0] = 0.95f; box.color[1] = 0.60f; box.color[2] = 0.20f;
        m_registry.emplace_or_replace<BoxRenderer>(m_player, box);
    } else {
        m_registry.remove<BoxRenderer>(m_player);
    }
}

// ------------------------------------------------------------------ Systemes
void Scene::update(float dt) {
    spinSystem(m_registry, dt);
}

void Scene::updatePhysics(float dt) {
    if (!m_physics) return;
    for (const entt::entity e : createBodiesSystem(m_registry, *m_physics)) removeEntity(e); // moteur plein
    playerSystem(m_registry, *m_physics, dt);
    const int steps = m_physics->update(dt);
    syncTransformsSystem(m_registry, *m_physics, steps, m_physics->interpolationAlpha());
}

void Scene::render(Renderer& renderer) {
    renderSystem(m_registry, renderer);
}
