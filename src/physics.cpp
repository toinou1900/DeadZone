#include "physics.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Character/CharacterVirtual.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <memory>
#include <thread>
#include <vector>

namespace {

// Jolt separe les objets en "couches" : les corps fixes ne se testent jamais entre eux, ce qui economise du calcul.
namespace Layers {
constexpr JPH::ObjectLayer NON_MOVING = 0;
constexpr JPH::ObjectLayer MOVING     = 1;
constexpr JPH::uint        NUM        = 2;
}
namespace BroadPhaseLayers {
constexpr JPH::BroadPhaseLayer NON_MOVING(0);
constexpr JPH::BroadPhaseLayer MOVING(1);
constexpr JPH::uint            NUM = 2;
}

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        if (a == Layers::NON_MOVING) return b == Layers::MOVING;
        return true; // un corps mobile touche tout
    }
};

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
public:
    JPH::uint GetNumBroadPhaseLayers() const override { return BroadPhaseLayers::NUM; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return layer == Layers::NON_MOVING ? BroadPhaseLayers::NON_MOVING : BroadPhaseLayers::MOVING;
    }
#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        return layer == BroadPhaseLayers::NON_MOVING ? "NON_MOVING" : "MOVING";
    }
#endif
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
public:
    bool ShouldCollide(JPH::ObjectLayer layer, JPH::BroadPhaseLayer bpLayer) const override {
        if (layer == Layers::NON_MOVING) return bpLayer == BroadPhaseLayers::MOVING;
        return true;
    }
};

constexpr float    kFixedStep      = 1.0f / 60.0f;
constexpr int      kMaxStepsPerCall = 5;   // au-dela, on abandonne le retard plutot que de s'effondrer ("spirale de la mort")
// Capacites du moteur (memoire reservee a l'avance). Un gros tas de boites genere beaucoup de paires et de contacts.
constexpr JPH::uint kMaxBodyPairs  = 65536;
constexpr JPH::uint kMaxContacts   = 20480;
constexpr JPH::uint kTempMemoryBytes = 128u * 1024u * 1024u; // reserve de maniere paresseuse : seule la memoire utilisee est occupee

} // namespace

struct Physics::Impl {
    // Memoire de travail reutilisee a chaque pas de simulation. Elle doit etre assez grande pour les capacites ci-dessus
    // (paires de corps et contacts) : trop petite, Jolt plante des qu'un gros tas de corps se forme.
    JPH::TempAllocatorImpl                temp{kTempMemoryBytes};
    JPH::JobSystemThreadPool              jobs;
    BPLayerInterfaceImpl                  bpLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl     objectVsBroadPhaseFilter;
    ObjectLayerPairFilterImpl             objectLayerPairFilter;
    JPH::PhysicsSystem                    system;
    float                                 accumulator = 0.0f;

    // Un personnage = un CharacterVirtual de Jolt + la commande de deplacement demandee par le jeu.
    struct CharacterState {
        JPH::Ref<JPH::CharacterVirtual> character;
        float desiredVx = 0.0f, desiredVz = 0.0f; // vitesse horizontale souhaitee (m/s, repere du monde)
        bool  jumpRequested = false;
        float jumpSpeed     = 5.0f;
        float airControl    = 3.0f;
    };
    std::vector<std::unique_ptr<CharacterState>> characters; // l'identifiant d'un personnage = son index (nullptr = place libre)

    CharacterState* character(uint32_t id) const {
        return id < characters.size() ? characters[id].get() : nullptr;
    }

    // Un pas de simulation pour un personnage : gravite, saut, vitesse voulue, puis deplacement avec collisions.
    void stepCharacter(CharacterState& c, float dt) {
        const bool ground = c.character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
        const JPH::Vec3 gravity        = system.GetGravity();
        const JPH::Vec3 current        = c.character->GetLinearVelocity();
        const JPH::Vec3 groundVelocity = ground ? c.character->GetGroundVelocity() : JPH::Vec3::sZero();

        float vy;
        if (ground && current.GetY() - groundVelocity.GetY() < 0.1f) { // au sol (et ne monte pas) : suit le sol
            vy = groundVelocity.GetY();
            if (c.jumpRequested) vy += c.jumpSpeed;
        } else {                                                       // en l'air : gravite
            vy = current.GetY() + gravity.GetY() * dt;
        }

        float vx, vz;
        if (ground) {                                                  // au sol : controle total
            vx = groundVelocity.GetX() + c.desiredVx;
            vz = groundVelocity.GetZ() + c.desiredVz;
        } else {                                                       // en l'air : on garde l'elan, on corrige doucement
            const float k = 1.0f - std::exp(-c.airControl * dt);
            vx = current.GetX() + (c.desiredVx - current.GetX()) * k;
            vz = current.GetZ() + (c.desiredVz - current.GetZ()) * k;
        }
        c.character->SetLinearVelocity(JPH::Vec3(vx, vy, vz));
        c.jumpRequested = false;

        JPH::CharacterVirtual::ExtendedUpdateSettings updateSettings; // colle au sol dans les descentes, monte les marches
        c.character->ExtendedUpdate(dt, gravity, updateSettings,
                                    system.GetDefaultBroadPhaseLayerFilter(Layers::MOVING),
                                    system.GetDefaultLayerFilter(Layers::MOVING),
                                    JPH::BodyFilter(), JPH::ShapeFilter(), temp);
    }

    Impl()
        : jobs(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
               static_cast<int>(std::max(1u, std::thread::hardware_concurrency()) - 1)) {
        system.Init(Physics::kMaxBodies, 0, kMaxBodyPairs, kMaxContacts,
                    bpLayerInterface, objectVsBroadPhaseFilter, objectLayerPairFilter);
        system.SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
    }
};

Physics::Physics()  = default;
Physics::~Physics() { shutdown(); }

bool Physics::init() {
    if (m_impl) return true;
    JPH::RegisterDefaultAllocator();
    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();
    m_impl = std::make_unique<Impl>();
    return true;
}

void Physics::shutdown() {
    if (!m_impl) return;
    m_impl.reset(); // le PhysicsSystem et ses corps sont detruits ici
    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;
}

uint32_t Physics::createBox(const BoxBodyDesc& d) {
    if (!m_impl) return kInvalidBody;

    // Jolt arrondit legerement les aretes ("rayon de convexite") : il ne doit pas depasser la plus petite demi-dimension.
    const float minHalf = std::max(0.01f, std::min({d.halfExtents[0], d.halfExtents[1], d.halfExtents[2]}));
    const float radius  = std::min(JPH::cDefaultConvexRadius, minHalf * 0.5f);
    const JPH::Vec3 half(std::max(d.halfExtents[0], 0.01f), std::max(d.halfExtents[1], 0.01f),
                         std::max(d.halfExtents[2], 0.01f));

    JPH::BodyCreationSettings settings(
        new JPH::BoxShape(half, radius),
        JPH::RVec3(d.position[0], d.position[1], d.position[2]),
        JPH::Quat(d.rotation[0], d.rotation[1], d.rotation[2], d.rotation[3]).Normalized(),
        d.dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
        d.dynamic ? Layers::MOVING : Layers::NON_MOVING);
    settings.mFriction    = d.friction;
    settings.mRestitution = d.restitution;
    if (d.dynamic) {
        settings.mLinearVelocity  = JPH::Vec3(d.linearVelocity[0],  d.linearVelocity[1],  d.linearVelocity[2]);
        settings.mAngularVelocity = JPH::Vec3(d.angularVelocity[0], d.angularVelocity[1], d.angularVelocity[2]);
    }

    JPH::BodyInterface& bodies = m_impl->system.GetBodyInterface();
    const JPH::BodyID id = bodies.CreateAndAddBody(settings,
        d.dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    return id.IsInvalid() ? kInvalidBody : id.GetIndexAndSequenceNumber();
}

void Physics::destroyBody(uint32_t body) {
    if (!m_impl || body == kInvalidBody) return;
    JPH::BodyInterface& bodies = m_impl->system.GetBodyInterface();
    const JPH::BodyID id(body);
    if (bodies.IsAdded(id)) bodies.RemoveBody(id);
    bodies.DestroyBody(id);
}

int Physics::update(float dt) {
    if (!m_impl) return 0;
    m_impl->accumulator += dt;
    int steps = 0;
    while (m_impl->accumulator >= kFixedStep && steps < kMaxStepsPerCall) {
        for (auto& c : m_impl->characters)       // les personnages se deplacent avant le pas de simulation
            if (c) m_impl->stepCharacter(*c, kFixedStep);
        m_impl->system.Update(kFixedStep, 1, &m_impl->temp, &m_impl->jobs);
        m_impl->accumulator -= kFixedStep;
        ++steps;
    }
    if (steps == kMaxStepsPerCall) m_impl->accumulator = 0.0f;
    return steps;
}

float Physics::interpolationAlpha() const {
    return m_impl ? std::min(1.0f, std::max(0.0f, m_impl->accumulator / kFixedStep)) : 0.0f;
}

bool Physics::getTransform(uint32_t body, float position[3], float rotation[4]) const {
    if (!m_impl || body == kInvalidBody) return false;
    const JPH::BodyInterface& bodies = m_impl->system.GetBodyInterface();
    const JPH::BodyID id(body);
    if (!bodies.IsAdded(id)) return false;
    const JPH::RVec3 p = bodies.GetPosition(id);
    const JPH::Quat  q = bodies.GetRotation(id);
    position[0] = static_cast<float>(p.GetX()); position[1] = static_cast<float>(p.GetY()); position[2] = static_cast<float>(p.GetZ());
    rotation[0] = q.GetX(); rotation[1] = q.GetY(); rotation[2] = q.GetZ(); rotation[3] = q.GetW();
    return true;
}

uint32_t Physics::bodyCount() const {
    return m_impl ? m_impl->system.GetNumBodies() : 0;
}

// ------------------------------------------------------------------ Personnages
uint32_t Physics::createCharacter(const CharacterDesc& d) {
    if (!m_impl) return kInvalidBody;

    // Capsule decalee vers le haut pour que l'origine du personnage soit ses PIEDS.
    JPH::RotatedTranslatedShapeSettings shapeSettings(
        JPH::Vec3(0.0f, d.halfHeight + d.radius, 0.0f), JPH::Quat::sIdentity(),
        new JPH::CapsuleShape(d.halfHeight, d.radius));
    const JPH::ShapeSettings::ShapeResult shape = shapeSettings.Create();
    if (shape.HasError()) return kInvalidBody;

    JPH::Ref<JPH::CharacterVirtualSettings> settings = new JPH::CharacterVirtualSettings;
    settings->mShape             = shape.Get();
    settings->mMass              = d.mass;
    settings->mMaxStrength       = d.maxPushForce;                    // force maximale de poussee des objets (N)
    settings->mUp                = JPH::Vec3::sAxisY();
    settings->mSupportingVolume  = JPH::Plane(JPH::Vec3::sAxisY(), -d.radius); // seule la demi-sphere du bas compte comme "appui"
    settings->mInnerBodyShape    = shape.Get();                       // corps "interieur" : les objets lances le heurtent
    settings->mInnerBodyLayer    = Layers::MOVING;

    JPH::Ref<JPH::CharacterVirtual> character = new JPH::CharacterVirtual(
        settings, JPH::RVec3(d.position[0], d.position[1], d.position[2]), JPH::Quat::sIdentity(), 0, &m_impl->system);

    auto state = std::make_unique<Impl::CharacterState>();
    state->character = character;

    // Reutilise une place libre sinon ajoute a la fin
    for (uint32_t i = 0; i < m_impl->characters.size(); ++i) {
        if (!m_impl->characters[i]) { m_impl->characters[i] = std::move(state); return i; }
    }
    m_impl->characters.push_back(std::move(state));
    return static_cast<uint32_t>(m_impl->characters.size() - 1);
}

void Physics::destroyCharacter(uint32_t id) {
    if (!m_impl || id >= m_impl->characters.size()) return;
    m_impl->characters[id].reset(); // le corps interieur est retire du systeme avec le CharacterVirtual
}

void Physics::moveCharacter(uint32_t id, float vx, float vz, bool jump, float jumpSpeed, float airControl) {
    Impl::CharacterState* c = m_impl ? m_impl->character(id) : nullptr;
    if (!c) return;
    c->desiredVx = vx; c->desiredVz = vz;
    c->jumpSpeed = jumpSpeed; c->airControl = airControl;
    c->jumpRequested = c->jumpRequested || jump; // memorise un appui court (images sans pas de simulation)
}

bool Physics::characterOnGround(uint32_t id) const {
    const Impl::CharacterState* c = m_impl ? m_impl->character(id) : nullptr;
    return c && c->character->GetGroundState() == JPH::CharacterBase::EGroundState::OnGround;
}

bool Physics::getCharacterPosition(uint32_t id, float feet[3]) const {
    const Impl::CharacterState* c = m_impl ? m_impl->character(id) : nullptr;
    if (!c) return false;
    const JPH::RVec3 p = c->character->GetPosition();
    feet[0] = static_cast<float>(p.GetX()); feet[1] = static_cast<float>(p.GetY()); feet[2] = static_cast<float>(p.GetZ());
    return true;
}

void Physics::teleportCharacter(uint32_t id, const float feet[3]) {
    Impl::CharacterState* c = m_impl ? m_impl->character(id) : nullptr;
    if (!c) return;
    c->character->SetPosition(JPH::RVec3(feet[0], feet[1], feet[2]));
    c->character->SetLinearVelocity(JPH::Vec3::sZero());
}
