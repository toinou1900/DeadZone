#include "physics.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSettings.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <thread>

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
constexpr JPH::uint kMaxBodies     = 4096;
constexpr JPH::uint kMaxBodyPairs  = 8192;
constexpr JPH::uint kMaxContacts   = 8192;

} // namespace

struct Physics::Impl {
    JPH::TempAllocatorImpl                temp{16 * 1024 * 1024};
    JPH::JobSystemThreadPool              jobs;
    BPLayerInterfaceImpl                  bpLayerInterface;
    ObjectVsBroadPhaseLayerFilterImpl     objectVsBroadPhaseFilter;
    ObjectLayerPairFilterImpl             objectLayerPairFilter;
    JPH::PhysicsSystem                    system;
    float                                 accumulator = 0.0f;

    Impl()
        : jobs(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers,
               static_cast<int>(std::max(1u, std::thread::hardware_concurrency()) - 1)) {
        system.Init(kMaxBodies, 0, kMaxBodyPairs, kMaxContacts,
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

void Physics::update(float dt) {
    if (!m_impl) return;
    m_impl->accumulator += dt;
    int steps = 0;
    while (m_impl->accumulator >= kFixedStep && steps < kMaxStepsPerCall) {
        m_impl->system.Update(kFixedStep, 1, &m_impl->temp, &m_impl->jobs);
        m_impl->accumulator -= kFixedStep;
        ++steps;
    }
    if (steps == kMaxStepsPerCall) m_impl->accumulator = 0.0f;
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
