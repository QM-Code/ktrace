#include "karma/physics/physics_system.hpp"

#include "karma/common/logging/logging.hpp"
#include "karma/physics/backend.hpp"

#include <sstream>

namespace karma::physics {
namespace {

std::string CompiledBackendList() {
    const auto compiled = physics_backend::CompiledBackends();
    if (compiled.empty()) {
        return "(none)";
    }

    std::ostringstream out;
    for (size_t i = 0; i < compiled.size(); ++i) {
        if (i != 0) {
            out << ",";
        }
        out << physics_backend::BackendKindName(compiled[i]);
    }
    return out.str();
}

} // namespace

const char* PhysicsSystem::selectedBackendName() const {
    return physics_backend::BackendKindName(selected_backend_);
}

void PhysicsSystem::init() {
    if (initialized_) {
        return;
    }

    KARMA_TRACE("physics.system",
                "PhysicsSystem: creating backend requested='{}' compiled='{}'",
                physics_backend::BackendKindName(requested_backend_),
                CompiledBackendList());
    backend_ = physics_backend::CreateBackend(requested_backend_, &selected_backend_);
    if (!backend_) {
        selected_backend_ = physics_backend::BackendKind::Auto;
        return;
    }

    if (!backend_->init()) {
        KARMA_TRACE("physics.system",
                    "PhysicsSystem: backend '{}' init failed",
                    backend_->name());
        backend_.reset();
        selected_backend_ = physics_backend::BackendKind::Auto;
        return;
    }

    initialized_ = true;
    KARMA_TRACE("physics.system",
                "PhysicsSystem: backend ready selected='{}'",
                backend_->name());
}

void PhysicsSystem::shutdown() {
    if (!backend_) {
        initialized_ = false;
        selected_backend_ = physics_backend::BackendKind::Auto;
        return;
    }

    backend_->shutdown();
    backend_.reset();
    initialized_ = false;
    selected_backend_ = physics_backend::BackendKind::Auto;
}

void PhysicsSystem::beginFrame(float dt) {
    if (!backend_) {
        return;
    }
    backend_->beginFrame(dt);
}

void PhysicsSystem::simulateFixedStep(float fixed_dt) {
    if (!backend_) {
        return;
    }
    backend_->simulateFixedStep(fixed_dt);
}

void PhysicsSystem::endFrame() {
    if (!backend_) {
        return;
    }
    backend_->endFrame();
}

physics_backend::BodyId PhysicsSystem::createBody(const physics_backend::BodyDesc& desc) {
    if (!backend_) {
        return physics_backend::kInvalidBodyId;
    }
    // BodyDesc is passed through as-is so backend shape-local offsets stay substrate-owned.
    return backend_->createBody(desc);
}

void PhysicsSystem::destroyBody(physics_backend::BodyId body) {
    if (!backend_) {
        return;
    }
    backend_->destroyBody(body);
}

bool PhysicsSystem::setBodyTransform(physics_backend::BodyId body,
                                     const physics_backend::BodyTransform& transform) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyTransform(body, transform);
}

bool PhysicsSystem::getBodyTransform(physics_backend::BodyId body,
                                     physics_backend::BodyTransform& out_transform) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyTransform(body, out_transform);
}

bool PhysicsSystem::setBodyGravityEnabled(physics_backend::BodyId body, bool enabled) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyGravityEnabled(body, enabled);
}

bool PhysicsSystem::getBodyGravityEnabled(physics_backend::BodyId body, bool& out_enabled) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyGravityEnabled(body, out_enabled);
}

bool PhysicsSystem::setBodyKinematic(physics_backend::BodyId body, bool enabled) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyKinematic(body, enabled);
}

bool PhysicsSystem::getBodyKinematic(physics_backend::BodyId body, bool& out_enabled) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyKinematic(body, out_enabled);
}

bool PhysicsSystem::setBodyAwake(physics_backend::BodyId body, bool enabled) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyAwake(body, enabled);
}

bool PhysicsSystem::getBodyAwake(physics_backend::BodyId body, bool& out_enabled) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyAwake(body, out_enabled);
}

bool PhysicsSystem::addBodyForce(physics_backend::BodyId body, const glm::vec3& force) {
    if (!backend_) {
        return false;
    }
    return backend_->addBodyForce(body, force);
}

bool PhysicsSystem::addBodyLinearImpulse(physics_backend::BodyId body, const glm::vec3& impulse) {
    if (!backend_) {
        return false;
    }
    return backend_->addBodyLinearImpulse(body, impulse);
}

bool PhysicsSystem::addBodyTorque(physics_backend::BodyId body, const glm::vec3& torque) {
    if (!backend_) {
        return false;
    }
    return backend_->addBodyTorque(body, torque);
}

bool PhysicsSystem::addBodyAngularImpulse(physics_backend::BodyId body, const glm::vec3& impulse) {
    if (!backend_) {
        return false;
    }
    return backend_->addBodyAngularImpulse(body, impulse);
}

bool PhysicsSystem::setBodyLinearVelocity(physics_backend::BodyId body, const glm::vec3& velocity) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyLinearVelocity(body, velocity);
}

bool PhysicsSystem::getBodyLinearVelocity(physics_backend::BodyId body, glm::vec3& out_velocity) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyLinearVelocity(body, out_velocity);
}

bool PhysicsSystem::setBodyAngularVelocity(physics_backend::BodyId body, const glm::vec3& velocity) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyAngularVelocity(body, velocity);
}

bool PhysicsSystem::getBodyAngularVelocity(physics_backend::BodyId body, glm::vec3& out_velocity) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyAngularVelocity(body, out_velocity);
}

bool PhysicsSystem::setBodyLinearDamping(physics_backend::BodyId body, float damping) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyLinearDamping(body, damping);
}

bool PhysicsSystem::getBodyLinearDamping(physics_backend::BodyId body, float& out_damping) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyLinearDamping(body, out_damping);
}

bool PhysicsSystem::setBodyAngularDamping(physics_backend::BodyId body, float damping) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyAngularDamping(body, damping);
}

bool PhysicsSystem::getBodyAngularDamping(physics_backend::BodyId body, float& out_damping) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyAngularDamping(body, out_damping);
}

bool PhysicsSystem::setBodyRotationLocked(physics_backend::BodyId body, bool locked) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyRotationLocked(body, locked);
}

bool PhysicsSystem::getBodyRotationLocked(physics_backend::BodyId body, bool& out_locked) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyRotationLocked(body, out_locked);
}

bool PhysicsSystem::setBodyTranslationLocked(physics_backend::BodyId body, bool locked) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyTranslationLocked(body, locked);
}

bool PhysicsSystem::getBodyTranslationLocked(physics_backend::BodyId body, bool& out_locked) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyTranslationLocked(body, out_locked);
}

bool PhysicsSystem::setBodyTrigger(physics_backend::BodyId body, bool enabled) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyTrigger(body, enabled);
}

bool PhysicsSystem::getBodyTrigger(physics_backend::BodyId body, bool& out_enabled) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyTrigger(body, out_enabled);
}

bool PhysicsSystem::setBodyCollisionMask(physics_backend::BodyId body, const physics_backend::CollisionMask& mask) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyCollisionMask(body, mask);
}

bool PhysicsSystem::getBodyCollisionMask(physics_backend::BodyId body, physics_backend::CollisionMask& out_mask) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyCollisionMask(body, out_mask);
}

bool PhysicsSystem::setBodyFriction(physics_backend::BodyId body, float friction) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyFriction(body, friction);
}

bool PhysicsSystem::getBodyFriction(physics_backend::BodyId body, float& out_friction) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyFriction(body, out_friction);
}

bool PhysicsSystem::setBodyRestitution(physics_backend::BodyId body, float restitution) {
    if (!backend_) {
        return false;
    }
    return backend_->setBodyRestitution(body, restitution);
}

bool PhysicsSystem::getBodyRestitution(physics_backend::BodyId body, float& out_restitution) const {
    if (!backend_) {
        return false;
    }
    return backend_->getBodyRestitution(body, out_restitution);
}

bool PhysicsSystem::raycastClosest(const glm::vec3& origin,
                                   const glm::vec3& direction,
                                   float max_distance,
                                   physics_backend::RaycastHit& out_hit) const {
    if (!backend_) {
        return false;
    }
    return backend_->raycastClosest(origin, direction, max_distance, out_hit);
}

} // namespace karma::physics
