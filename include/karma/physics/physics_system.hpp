#pragma once

#include "karma/physics/backend.hpp"

#include <memory>
#include <string>

namespace karma::physics {

class PhysicsSystem {
 public:
    void setBackend(physics_backend::BackendKind backend) { requested_backend_ = backend; }
    physics_backend::BackendKind requestedBackend() const { return requested_backend_; }
    physics_backend::BackendKind selectedBackend() const { return selected_backend_; }
    const char* selectedBackendName() const;
    bool isInitialized() const { return initialized_; }

    void init();
    void shutdown();
    void beginFrame(float dt);
    void simulateFixedStep(float fixed_dt);
    void endFrame();

    // Forwards the full backend-neutral body descriptor, including collider local-center shape offset + material.
    physics_backend::BodyId createBody(const physics_backend::BodyDesc& desc);
    void destroyBody(physics_backend::BodyId body);
    bool setBodyTransform(physics_backend::BodyId body, const physics_backend::BodyTransform& transform);
    bool getBodyTransform(physics_backend::BodyId body, physics_backend::BodyTransform& out_transform) const;
    bool setBodyGravityEnabled(physics_backend::BodyId body, bool enabled);
    bool getBodyGravityEnabled(physics_backend::BodyId body, bool& out_enabled) const;
    bool setBodyKinematic(physics_backend::BodyId body, bool enabled);
    bool getBodyKinematic(physics_backend::BodyId body, bool& out_enabled) const;
    bool setBodyAwake(physics_backend::BodyId body, bool enabled);
    bool getBodyAwake(physics_backend::BodyId body, bool& out_enabled) const;
    bool addBodyForce(physics_backend::BodyId body, const glm::vec3& force);
    bool addBodyLinearImpulse(physics_backend::BodyId body, const glm::vec3& impulse);
    bool addBodyTorque(physics_backend::BodyId body, const glm::vec3& torque);
    bool addBodyAngularImpulse(physics_backend::BodyId body, const glm::vec3& impulse);
    bool setBodyLinearVelocity(physics_backend::BodyId body, const glm::vec3& velocity);
    bool getBodyLinearVelocity(physics_backend::BodyId body, glm::vec3& out_velocity) const;
    bool setBodyAngularVelocity(physics_backend::BodyId body, const glm::vec3& velocity);
    bool getBodyAngularVelocity(physics_backend::BodyId body, glm::vec3& out_velocity) const;
    bool setBodyLinearDamping(physics_backend::BodyId body, float damping);
    bool getBodyLinearDamping(physics_backend::BodyId body, float& out_damping) const;
    bool setBodyAngularDamping(physics_backend::BodyId body, float damping);
    bool getBodyAngularDamping(physics_backend::BodyId body, float& out_damping) const;
    bool setBodyRotationLocked(physics_backend::BodyId body, bool locked);
    bool getBodyRotationLocked(physics_backend::BodyId body, bool& out_locked) const;
    bool setBodyTranslationLocked(physics_backend::BodyId body, bool locked);
    bool getBodyTranslationLocked(physics_backend::BodyId body, bool& out_locked) const;
    bool setBodyTrigger(physics_backend::BodyId body, bool enabled);
    bool getBodyTrigger(physics_backend::BodyId body, bool& out_enabled) const;
    bool setBodyCollisionMask(physics_backend::BodyId body, const physics_backend::CollisionMask& mask);
    bool getBodyCollisionMask(physics_backend::BodyId body, physics_backend::CollisionMask& out_mask) const;
    bool setBodyFriction(physics_backend::BodyId body, float friction);
    bool getBodyFriction(physics_backend::BodyId body, float& out_friction) const;
    bool setBodyRestitution(physics_backend::BodyId body, float restitution);
    bool getBodyRestitution(physics_backend::BodyId body, float& out_restitution) const;
    bool raycastClosest(const glm::vec3& origin,
                        const glm::vec3& direction,
                        float max_distance,
                        physics_backend::RaycastHit& out_hit) const;

 private:
    physics_backend::BackendKind requested_backend_ = physics_backend::BackendKind::Auto;
    physics_backend::BackendKind selected_backend_ = physics_backend::BackendKind::Auto;
    std::unique_ptr<physics_backend::Backend> backend_{};
    bool initialized_ = false;
};

} // namespace karma::physics
