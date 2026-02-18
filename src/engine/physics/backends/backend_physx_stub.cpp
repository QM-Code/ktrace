#include "physics/backends/backend_factory_internal.hpp"

#include "karma/common/logging/logging.hpp"

#include <string>
#include <unordered_map>
#include <vector>

#if defined(KARMA_HAS_PHYSICS_PHYSX)
#include <PxPhysicsAPI.h>

#include <algorithm>
#include <cmath>
#include <thread>
#endif

namespace karma::physics_backend {
namespace {

#if defined(KARMA_HAS_PHYSICS_PHYSX)

physx::PxVec3 ToPxVec3(const glm::vec3& value) {
    return physx::PxVec3(value.x, value.y, value.z);
}

physx::PxQuat ToPxQuat(const glm::quat& value) {
    return physx::PxQuat(value.x, value.y, value.z, value.w);
}

glm::vec3 ToGlmVec3(const physx::PxVec3& value) {
    return glm::vec3(value.x, value.y, value.z);
}

glm::quat ToGlmQuat(const physx::PxQuat& value) {
    return glm::quat(value.w, value.x, value.y, value.z);
}

bool IsValidCollisionMask(const CollisionMask& mask) {
    return mask.layer != 0u && mask.collides_with != 0u;
}

bool IsFiniteVec3(const glm::vec3& value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
}

bool IsZeroVec3(const glm::vec3& value, float epsilon = 1e-6f) {
    return std::fabs(value.x) <= epsilon
           && std::fabs(value.y) <= epsilon
           && std::fabs(value.z) <= epsilon;
}

physx::PxFilterData ToPxFilterData(const CollisionMask& mask) {
    physx::PxFilterData data{};
    data.word0 = mask.layer;
    data.word1 = mask.collides_with;
    return data;
}

class PhysXBackend final : public Backend {
 public:
    const char* name() const override {
        return "physx";
    }

    bool init() override {
        if (initialized_) {
            return true;
        }

        foundation_ = PxCreateFoundation(PX_PHYSICS_VERSION, allocator_, error_callback_);
        if (!foundation_) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create foundation");
            shutdown();
            return false;
        }

        physics_ = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation_, physx::PxTolerancesScale(), true);
        if (!physics_) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create physics");
            shutdown();
            return false;
        }

        physx::PxSceneDesc scene_desc(physics_->getTolerancesScale());
        scene_desc.gravity = physx::PxVec3(0.0f, -9.8f, 0.0f);
        const uint32_t worker_count = std::max(1u, std::thread::hardware_concurrency());
        dispatcher_ = physx::PxDefaultCpuDispatcherCreate(static_cast<physx::PxU32>(worker_count));
        if (!dispatcher_) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create dispatcher");
            shutdown();
            return false;
        }
        scene_desc.cpuDispatcher = dispatcher_;
        scene_desc.filterShader = physx::PxDefaultSimulationFilterShader;
        scene_ = physics_->createScene(scene_desc);
        if (!scene_) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create scene");
            shutdown();
            return false;
        }

        default_material_ = physics_->createMaterial(0.5f, 0.5f, 0.0f);
        if (!default_material_) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create default material");
            shutdown();
            return false;
        }

        initialized_ = true;
        KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: initialized");
        return true;
    }

    void shutdown() override {
        for (const auto& [id, actor] : bodies_) {
            (void)id;
            if (scene_ && actor) {
                scene_->removeActor(*actor);
            }
            if (actor) {
                actor->release();
            }
        }
        bodies_.clear();
        dynamic_bodies_.clear();
        gravity_enabled_.clear();
        trigger_enabled_.clear();
        collision_masks_.clear();
        kinematic_enabled_.clear();
        for (const auto& [id, material] : body_materials_) {
            (void)id;
            if (material) {
                material->release();
            }
        }
        body_materials_.clear();
        friction_.clear();
        restitution_.clear();
        linear_damping_.clear();
        angular_damping_.clear();

        if (default_material_) {
            default_material_->release();
            default_material_ = nullptr;
        }
        if (scene_) {
            scene_->release();
            scene_ = nullptr;
        }
        if (dispatcher_) {
            dispatcher_->release();
            dispatcher_ = nullptr;
        }
        if (physics_) {
            physics_->release();
            physics_ = nullptr;
        }
        if (foundation_) {
            foundation_->release();
            foundation_ = nullptr;
        }

        initialized_ = false;
        KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: shutdown");
    }

    void beginFrame(float dt) override {
        frame_dt_ = dt;
    }

    void simulateFixedStep(float fixed_dt) override {
        if (!scene_ || fixed_dt <= 0.0f) {
            return;
        }

        scene_->simulate(fixed_dt);
        scene_->fetchResults(true);
    }

    void endFrame() override {
        (void)frame_dt_;
        KARMA_TRACE_CHANGED("physics.physx",
                            std::to_string(bodies_.size()),
                            "PhysicsBackend[physx]: active bodies={}",
                            bodies_.size());
    }

    BodyId createBody(const BodyDesc& desc) override {
        if (!physics_ || !scene_ || !default_material_) {
            return kInvalidBodyId;
        }
        if (!IsValidCollisionMask(desc.collision_mask)) {
            return kInvalidBodyId;
        }
        if (!IsFiniteVec3(desc.collider_shape.local_center)) {
            return kInvalidBodyId;
        }
        if (!std::isfinite(desc.linear_damping)
            || !std::isfinite(desc.angular_damping)
            || desc.linear_damping < 0.0f
            || desc.angular_damping < 0.0f) {
            return kInvalidBodyId;
        }
        if (!std::isfinite(desc.friction)
            || !std::isfinite(desc.restitution)
            || desc.friction < 0.0f
            || desc.restitution < 0.0f
            || desc.restitution > 1.0f) {
            return kInvalidBodyId;
        }
        if (desc.is_static && desc.is_kinematic) {
            return kInvalidBodyId;
        }
        if (!desc.is_static && desc.rotation_locked && desc.translation_locked) {
            return kInvalidBodyId;
        }

        const physx::PxTransform transform(ToPxVec3(desc.transform.position), ToPxQuat(desc.transform.rotation));
        physx::PxGeometryHolder geometry{};
        switch (desc.collider_shape.kind) {
            case ColliderShapeKind::Box:
                if (!IsFiniteVec3(desc.collider_shape.box_half_extents)
                    || desc.collider_shape.box_half_extents.x <= 0.0f
                    || desc.collider_shape.box_half_extents.y <= 0.0f
                    || desc.collider_shape.box_half_extents.z <= 0.0f) {
                    return kInvalidBodyId;
                }
                geometry.storeAny(physx::PxBoxGeometry(desc.collider_shape.box_half_extents.x,
                                                       desc.collider_shape.box_half_extents.y,
                                                       desc.collider_shape.box_half_extents.z));
                break;
            case ColliderShapeKind::Sphere:
                if (!std::isfinite(desc.collider_shape.sphere_radius) || desc.collider_shape.sphere_radius <= 0.0f) {
                    return kInvalidBodyId;
                }
                geometry.storeAny(physx::PxSphereGeometry(desc.collider_shape.sphere_radius));
                break;
            case ColliderShapeKind::Capsule:
                if (!std::isfinite(desc.collider_shape.capsule_radius)
                    || !std::isfinite(desc.collider_shape.capsule_half_height)
                    || desc.collider_shape.capsule_radius <= 0.0f
                    || desc.collider_shape.capsule_half_height <= 0.0f) {
                    return kInvalidBodyId;
                }
                geometry.storeAny(physx::PxCapsuleGeometry(desc.collider_shape.capsule_radius,
                                                           desc.collider_shape.capsule_half_height));
                break;
            default:
                return kInvalidBodyId;
        }

        physx::PxMaterial* material =
            physics_->createMaterial(desc.friction, desc.friction, desc.restitution);
        if (!material) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create per-body material");
            return kInvalidBodyId;
        }

        physx::PxShape* shape = physics_->createShape(geometry.any(), *material);
        if (!shape) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create shape");
            material->release();
            return kInvalidBodyId;
        }

        shape->setSimulationFilterData(ToPxFilterData(desc.collision_mask));
        shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, desc.is_trigger);
        shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !desc.is_trigger);
        shape->setLocalPose(physx::PxTransform(ToPxVec3(desc.collider_shape.local_center)));

        physx::PxRigidActor* actor = nullptr;
        if (desc.is_static) {
            auto* static_actor = physics_->createRigidStatic(transform);
            if (static_actor) {
                static_actor->attachShape(*shape);
                actor = static_actor;
            }
        } else {
            auto* dynamic_actor = physics_->createRigidDynamic(transform);
            if (dynamic_actor) {
                dynamic_actor->attachShape(*shape);
                physx::PxRigidBodyExt::updateMassAndInertia(*dynamic_actor, std::max(desc.mass, 0.001f));
                dynamic_actor->setLinearVelocity(ToPxVec3(desc.linear_velocity));
                dynamic_actor->setAngularVelocity(ToPxVec3(desc.angular_velocity));
                dynamic_actor->setLinearDamping(desc.linear_damping);
                dynamic_actor->setAngularDamping(desc.angular_damping);
                dynamic_actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !desc.gravity_enabled);
                dynamic_actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, desc.is_kinematic);
                physx::PxRigidDynamicLockFlags lock_flags{};
                bool has_lock_flags = false;
                if (desc.rotation_locked) {
                    lock_flags |=
                        physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X
                        | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y
                        | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
                    has_lock_flags = true;
                }
                if (desc.translation_locked) {
                    lock_flags |=
                        physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_X
                        | physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Y
                        | physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;
                    has_lock_flags = true;
                }
                if (has_lock_flags) {
                    dynamic_actor->setRigidDynamicLockFlags(lock_flags);
                }
                actor = dynamic_actor;
            }
        }

        shape->release();
        if (!actor) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create actor");
            material->release();
            return kInvalidBodyId;
        }

        const bool is_dynamic = actor->is<physx::PxRigidDynamic>() != nullptr;
        scene_->addActor(*actor);
        if (is_dynamic) {
            auto* dynamic_actor = actor->is<physx::PxRigidDynamic>();
            if (dynamic_actor) {
                if (desc.awake) {
                    dynamic_actor->wakeUp();
                } else {
                    dynamic_actor->putToSleep();
                }
            }
        }
        const BodyId id = next_body_id_++;
        bodies_.emplace(id, actor);
        dynamic_bodies_.emplace(id, is_dynamic);
        gravity_enabled_.emplace(id, is_dynamic ? desc.gravity_enabled : false);
        trigger_enabled_.emplace(id, desc.is_trigger);
        collision_masks_.emplace(id, desc.collision_mask);
        kinematic_enabled_.emplace(id, is_dynamic && desc.is_kinematic);
        body_materials_.emplace(id, material);
        friction_.emplace(id, desc.friction);
        restitution_.emplace(id, desc.restitution);
        if (is_dynamic) {
            linear_damping_.emplace(id, desc.linear_damping);
            angular_damping_.emplace(id, desc.angular_damping);
        }
        return id;
    }

    void destroyBody(BodyId body) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return;
        }

        physx::PxRigidActor* actor = it->second;
        if (scene_ && actor) {
            scene_->removeActor(*actor);
        }
        if (actor) {
            actor->release();
        }
        dynamic_bodies_.erase(body);
        gravity_enabled_.erase(body);
        trigger_enabled_.erase(body);
        collision_masks_.erase(body);
        kinematic_enabled_.erase(body);
        const auto material_it = body_materials_.find(body);
        if (material_it != body_materials_.end()) {
            if (material_it->second) {
                material_it->second->release();
            }
            body_materials_.erase(material_it);
        }
        friction_.erase(body);
        restitution_.erase(body);
        linear_damping_.erase(body);
        angular_damping_.erase(body);
        bodies_.erase(it);
    }

    bool setBodyTransform(BodyId body, const BodyTransform& transform) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second) {
            return false;
        }

        it->second->setGlobalPose(physx::PxTransform(ToPxVec3(transform.position), ToPxQuat(transform.rotation)), true);
        if (auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>()) {
            dynamic_actor->wakeUp();
        }
        return true;
    }

    bool getBodyTransform(BodyId body, BodyTransform& out_transform) const override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second) {
            return false;
        }

        const physx::PxTransform transform = it->second->getGlobalPose();
        out_transform.position = ToGlmVec3(transform.p);
        out_transform.rotation = ToGlmQuat(transform.q);
        return true;
    }

    bool setBodyGravityEnabled(BodyId body, bool enabled) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        dynamic_actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !enabled);
        dynamic_actor->wakeUp();
        gravity_enabled_[body] = enabled;
        return true;
    }

    bool getBodyGravityEnabled(BodyId body, bool& out_enabled) const override {
        const auto it = gravity_enabled_.find(body);
        if (it == gravity_enabled_.end() || !isDynamicBody(body)) {
            return false;
        }

        out_enabled = it->second;
        return true;
    }

    bool setBodyKinematic(BodyId body, bool enabled) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        const auto kinematic_it = kinematic_enabled_.find(body);
        if (kinematic_it == kinematic_enabled_.end()) {
            return false;
        }
        if (kinematic_it->second == enabled) {
            return true;
        }

        dynamic_actor->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, enabled);
        dynamic_actor->wakeUp();
        kinematic_enabled_[body] = enabled;
        return true;
    }

    bool getBodyKinematic(BodyId body, bool& out_enabled) const override {
        if (!isDynamicBody(body)) {
            return false;
        }

        const auto it = kinematic_enabled_.find(body);
        if (it == kinematic_enabled_.end()) {
            return false;
        }
        out_enabled = it->second;
        return true;
    }

    bool setBodyAwake(BodyId body, bool enabled) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        if (enabled) {
            dynamic_actor->wakeUp();
        } else {
            dynamic_actor->putToSleep();
        }
        return true;
    }

    bool getBodyAwake(BodyId body, bool& out_enabled) const override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        out_enabled = !dynamic_actor->isSleeping();
        return true;
    }

    bool addBodyForce(BodyId body, const glm::vec3& force) override {
        if (!IsFiniteVec3(force)) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto kinematic_it = kinematic_enabled_.find(body);
        if (kinematic_it == kinematic_enabled_.end() || kinematic_it->second) {
            return false;
        }
        if (IsZeroVec3(force)) {
            return true;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        dynamic_actor->addForce(ToPxVec3(force), physx::PxForceMode::eFORCE, true);
        return true;
    }

    bool addBodyLinearImpulse(BodyId body, const glm::vec3& impulse) override {
        if (!IsFiniteVec3(impulse)) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto kinematic_it = kinematic_enabled_.find(body);
        if (kinematic_it == kinematic_enabled_.end() || kinematic_it->second) {
            return false;
        }
        if (IsZeroVec3(impulse)) {
            return true;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        dynamic_actor->addForce(ToPxVec3(impulse), physx::PxForceMode::eIMPULSE, true);
        return true;
    }

    bool addBodyTorque(BodyId body, const glm::vec3& torque) override {
        if (!IsFiniteVec3(torque)) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto kinematic_it = kinematic_enabled_.find(body);
        if (kinematic_it == kinematic_enabled_.end() || kinematic_it->second) {
            return false;
        }
        if (IsZeroVec3(torque)) {
            return true;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        dynamic_actor->addTorque(ToPxVec3(torque), physx::PxForceMode::eFORCE, true);
        return true;
    }

    bool addBodyAngularImpulse(BodyId body, const glm::vec3& impulse) override {
        if (!IsFiniteVec3(impulse)) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto kinematic_it = kinematic_enabled_.find(body);
        if (kinematic_it == kinematic_enabled_.end() || kinematic_it->second) {
            return false;
        }
        if (IsZeroVec3(impulse)) {
            return true;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        dynamic_actor->addTorque(ToPxVec3(impulse), physx::PxForceMode::eIMPULSE, true);
        return true;
    }

    bool setBodyLinearVelocity(BodyId body, const glm::vec3& velocity) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        dynamic_actor->setLinearVelocity(ToPxVec3(velocity));
        dynamic_actor->wakeUp();
        return true;
    }

    bool getBodyLinearVelocity(BodyId body, glm::vec3& out_velocity) const override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        out_velocity = ToGlmVec3(dynamic_actor->getLinearVelocity());
        return true;
    }

    bool setBodyAngularVelocity(BodyId body, const glm::vec3& velocity) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        dynamic_actor->setAngularVelocity(ToPxVec3(velocity));
        dynamic_actor->wakeUp();
        return true;
    }

    bool getBodyAngularVelocity(BodyId body, glm::vec3& out_velocity) const override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        out_velocity = ToGlmVec3(dynamic_actor->getAngularVelocity());
        return true;
    }

    bool setBodyLinearDamping(BodyId body, float damping) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }
        if (!std::isfinite(damping) || damping < 0.0f) {
            return false;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        dynamic_actor->setLinearDamping(damping);
        dynamic_actor->wakeUp();
        linear_damping_[body] = damping;
        return true;
    }

    bool getBodyLinearDamping(BodyId body, float& out_damping) const override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        out_damping = dynamic_actor->getLinearDamping();
        return true;
    }

    bool setBodyAngularDamping(BodyId body, float damping) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }
        if (!std::isfinite(damping) || damping < 0.0f) {
            return false;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        dynamic_actor->setAngularDamping(damping);
        dynamic_actor->wakeUp();
        angular_damping_[body] = damping;
        return true;
    }

    bool getBodyAngularDamping(BodyId body, float& out_damping) const override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        out_damping = dynamic_actor->getAngularDamping();
        return true;
    }

    bool setBodyRotationLocked(BodyId body, bool locked) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        physx::PxRigidDynamicLockFlags lock_flags = dynamic_actor->getRigidDynamicLockFlags();
        const physx::PxRigidDynamicLockFlags rotation_mask =
            physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X
            | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y
            | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
        const physx::PxRigidDynamicLockFlags translation_mask =
            physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_X
            | physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Y
            | physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;
        const bool translation_locked = (lock_flags & translation_mask) == translation_mask;
        if (locked && translation_locked) {
            return false;
        }
        if (locked) {
            lock_flags |= rotation_mask;
        } else {
            lock_flags &= ~rotation_mask;
        }

        dynamic_actor->setRigidDynamicLockFlags(lock_flags);
        dynamic_actor->wakeUp();
        return true;
    }

    bool getBodyRotationLocked(BodyId body, bool& out_locked) const override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        const physx::PxRigidDynamicLockFlags lock_flags = dynamic_actor->getRigidDynamicLockFlags();
        const physx::PxRigidDynamicLockFlags rotation_mask =
            physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X
            | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y
            | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
        out_locked = (lock_flags & rotation_mask) == rotation_mask;
        return true;
    }

    bool setBodyTranslationLocked(BodyId body, bool locked) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        physx::PxRigidDynamicLockFlags lock_flags = dynamic_actor->getRigidDynamicLockFlags();
        const physx::PxRigidDynamicLockFlags rotation_mask =
            physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_X
            | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Y
            | physx::PxRigidDynamicLockFlag::eLOCK_ANGULAR_Z;
        const physx::PxRigidDynamicLockFlags translation_mask =
            physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_X
            | physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Y
            | physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;
        const bool rotation_locked = (lock_flags & rotation_mask) == rotation_mask;
        if (locked && rotation_locked) {
            return false;
        }
        if (locked) {
            lock_flags |= translation_mask;
        } else {
            lock_flags &= ~translation_mask;
        }

        dynamic_actor->setRigidDynamicLockFlags(lock_flags);
        dynamic_actor->wakeUp();
        return true;
    }

    bool getBodyTranslationLocked(BodyId body, bool& out_locked) const override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second || !isDynamicBody(body)) {
            return false;
        }

        const auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>();
        if (!dynamic_actor) {
            return false;
        }

        const physx::PxRigidDynamicLockFlags lock_flags = dynamic_actor->getRigidDynamicLockFlags();
        const physx::PxRigidDynamicLockFlags translation_mask =
            physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_X
            | physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Y
            | physx::PxRigidDynamicLockFlag::eLOCK_LINEAR_Z;
        out_locked = (lock_flags & translation_mask) == translation_mask;
        return true;
    }

    bool setBodyTrigger(BodyId body, bool enabled) override {
        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end() || !body_it->second) {
            return false;
        }

        const auto trigger_it = trigger_enabled_.find(body);
        if (trigger_it == trigger_enabled_.end()) {
            return false;
        }

        const physx::PxU32 shape_count = body_it->second->getNbShapes();
        if (shape_count == 0) {
            return false;
        }
        std::vector<physx::PxShape*> shapes(shape_count, nullptr);
        body_it->second->getShapes(shapes.data(), shape_count);
        for (physx::PxShape* shape : shapes) {
            if (!shape) {
                continue;
            }
            shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, enabled);
            shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, !enabled);
        }
        if (scene_) {
            (void)scene_->resetFiltering(*body_it->second);
        }

        trigger_enabled_[body] = enabled;
        return true;
    }

    bool getBodyTrigger(BodyId body, bool& out_enabled) const override {
        const auto it = trigger_enabled_.find(body);
        if (it == trigger_enabled_.end()) {
            return false;
        }
        out_enabled = it->second;
        return true;
    }

    bool setBodyCollisionMask(BodyId body, const CollisionMask& mask) override {
        if (!IsValidCollisionMask(mask)) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end() || !body_it->second) {
            return false;
        }

        const auto mask_it = collision_masks_.find(body);
        if (mask_it == collision_masks_.end()) {
            return false;
        }

        const physx::PxU32 shape_count = body_it->second->getNbShapes();
        if (shape_count == 0) {
            return false;
        }
        std::vector<physx::PxShape*> shapes(shape_count, nullptr);
        body_it->second->getShapes(shapes.data(), shape_count);
        const physx::PxFilterData filter_data = ToPxFilterData(mask);
        for (physx::PxShape* shape : shapes) {
            if (!shape) {
                continue;
            }
            shape->setSimulationFilterData(filter_data);
        }
        if (scene_) {
            (void)scene_->resetFiltering(*body_it->second);
        }

        collision_masks_[body] = mask;
        return true;
    }

    bool getBodyCollisionMask(BodyId body, CollisionMask& out_mask) const override {
        const auto it = collision_masks_.find(body);
        if (it == collision_masks_.end()) {
            return false;
        }
        out_mask = it->second;
        return true;
    }

    bool setBodyFriction(BodyId body, float friction) override {
        if (!std::isfinite(friction) || friction < 0.0f) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end() || !body_it->second) {
            return false;
        }
        const auto material_it = body_materials_.find(body);
        if (material_it == body_materials_.end() || !material_it->second) {
            return false;
        }

        material_it->second->setStaticFriction(friction);
        material_it->second->setDynamicFriction(friction);
        friction_[body] = friction;
        if (auto* dynamic_actor = body_it->second->is<physx::PxRigidDynamic>()) {
            dynamic_actor->wakeUp();
        }
        return true;
    }

    bool getBodyFriction(BodyId body, float& out_friction) const override {
        const auto it = friction_.find(body);
        if (it == friction_.end()) {
            return false;
        }
        out_friction = it->second;
        return true;
    }

    bool setBodyRestitution(BodyId body, float restitution) override {
        if (!std::isfinite(restitution) || restitution < 0.0f || restitution > 1.0f) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end() || !body_it->second) {
            return false;
        }
        const auto material_it = body_materials_.find(body);
        if (material_it == body_materials_.end() || !material_it->second) {
            return false;
        }

        material_it->second->setRestitution(restitution);
        restitution_[body] = restitution;
        if (auto* dynamic_actor = body_it->second->is<physx::PxRigidDynamic>()) {
            dynamic_actor->wakeUp();
        }
        return true;
    }

    bool getBodyRestitution(BodyId body, float& out_restitution) const override {
        const auto it = restitution_.find(body);
        if (it == restitution_.end()) {
            return false;
        }
        out_restitution = it->second;
        return true;
    }

    bool raycastClosest(const glm::vec3& origin,
                        const glm::vec3& direction,
                        float max_distance,
                        RaycastHit& out_hit) const override {
        if (!scene_ || max_distance <= 0.0f) {
            return false;
        }

        const float direction_length = glm::length(direction);
        if (!std::isfinite(direction_length) || direction_length <= 1e-6f) {
            return false;
        }

        const glm::vec3 unit_direction = direction / direction_length;
        physx::PxRaycastBuffer hit_buffer;
        const bool has_hit = scene_->raycast(ToPxVec3(origin), ToPxVec3(unit_direction), max_distance, hit_buffer);
        if (!has_hit || !hit_buffer.hasBlock) {
            return false;
        }

        const BodyId hit_body = findBodyId(hit_buffer.block.actor);
        if (hit_body == kInvalidBodyId) {
            return false;
        }

        out_hit.body = hit_body;
        out_hit.position = ToGlmVec3(hit_buffer.block.position);
        out_hit.distance = hit_buffer.block.distance;
        out_hit.fraction = hit_buffer.block.distance / max_distance;
        return true;
    }

 private:
    BodyId findBodyId(const physx::PxRigidActor* actor) const {
        if (!actor) {
            return kInvalidBodyId;
        }

        for (const auto& [id, candidate] : bodies_) {
            if (candidate == actor) {
                return id;
            }
        }
        return kInvalidBodyId;
    }

    bool isDynamicBody(BodyId body) const {
        const auto it = dynamic_bodies_.find(body);
        return it != dynamic_bodies_.end() && it->second;
    }

    physx::PxDefaultAllocator allocator_{};
    physx::PxDefaultErrorCallback error_callback_{};
    physx::PxFoundation* foundation_ = nullptr;
    physx::PxPhysics* physics_ = nullptr;
    physx::PxDefaultCpuDispatcher* dispatcher_ = nullptr;
    physx::PxScene* scene_ = nullptr;
    physx::PxMaterial* default_material_ = nullptr;
    BodyId next_body_id_ = 1;
    std::unordered_map<BodyId, physx::PxRigidActor*> bodies_{};
    std::unordered_map<BodyId, bool> dynamic_bodies_{};
    std::unordered_map<BodyId, bool> gravity_enabled_{};
    std::unordered_map<BodyId, bool> trigger_enabled_{};
    std::unordered_map<BodyId, CollisionMask> collision_masks_{};
    std::unordered_map<BodyId, bool> kinematic_enabled_{};
    std::unordered_map<BodyId, physx::PxMaterial*> body_materials_{};
    std::unordered_map<BodyId, float> friction_{};
    std::unordered_map<BodyId, float> restitution_{};
    std::unordered_map<BodyId, float> linear_damping_{};
    std::unordered_map<BodyId, float> angular_damping_{};
    float frame_dt_ = 0.0f;
    bool initialized_ = false;
};

#else

class PhysXBackendStub final : public Backend {
 public:
    const char* name() const override { return "physx"; }

    bool init() override {
        KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: unavailable (not compiled)");
        return false;
    }

    void shutdown() override {}
    void beginFrame(float) override {}
    void simulateFixedStep(float) override {}
    void endFrame() override {}
    BodyId createBody(const BodyDesc&) override { return kInvalidBodyId; }
    void destroyBody(BodyId) override {}
    bool setBodyTransform(BodyId, const BodyTransform&) override { return false; }
    bool getBodyTransform(BodyId, BodyTransform&) const override { return false; }
    bool setBodyGravityEnabled(BodyId, bool) override { return false; }
    bool getBodyGravityEnabled(BodyId, bool&) const override { return false; }
    bool setBodyKinematic(BodyId, bool) override { return false; }
    bool getBodyKinematic(BodyId, bool&) const override { return false; }
    bool setBodyAwake(BodyId, bool) override { return false; }
    bool getBodyAwake(BodyId, bool&) const override { return false; }
    bool addBodyForce(BodyId, const glm::vec3&) override { return false; }
    bool addBodyLinearImpulse(BodyId, const glm::vec3&) override { return false; }
    bool addBodyTorque(BodyId, const glm::vec3&) override { return false; }
    bool addBodyAngularImpulse(BodyId, const glm::vec3&) override { return false; }
    bool setBodyLinearVelocity(BodyId, const glm::vec3&) override { return false; }
    bool getBodyLinearVelocity(BodyId, glm::vec3&) const override { return false; }
    bool setBodyAngularVelocity(BodyId, const glm::vec3&) override { return false; }
    bool getBodyAngularVelocity(BodyId, glm::vec3&) const override { return false; }
    bool setBodyLinearDamping(BodyId, float) override { return false; }
    bool getBodyLinearDamping(BodyId, float&) const override { return false; }
    bool setBodyAngularDamping(BodyId, float) override { return false; }
    bool getBodyAngularDamping(BodyId, float&) const override { return false; }
    bool setBodyRotationLocked(BodyId, bool) override { return false; }
    bool getBodyRotationLocked(BodyId, bool&) const override { return false; }
    bool setBodyTranslationLocked(BodyId, bool) override { return false; }
    bool getBodyTranslationLocked(BodyId, bool&) const override { return false; }
    bool setBodyTrigger(BodyId, bool) override { return false; }
    bool getBodyTrigger(BodyId, bool&) const override { return false; }
    bool setBodyCollisionMask(BodyId, const CollisionMask&) override { return false; }
    bool getBodyCollisionMask(BodyId, CollisionMask&) const override { return false; }
    bool setBodyFriction(BodyId, float) override { return false; }
    bool getBodyFriction(BodyId, float&) const override { return false; }
    bool setBodyRestitution(BodyId, float) override { return false; }
    bool getBodyRestitution(BodyId, float&) const override { return false; }
    bool raycastClosest(const glm::vec3&, const glm::vec3&, float, RaycastHit&) const override { return false; }
};

#endif

} // namespace

std::unique_ptr<Backend> CreatePhysXBackend() {
#if defined(KARMA_HAS_PHYSICS_PHYSX)
    return std::make_unique<PhysXBackend>();
#else
    return std::make_unique<PhysXBackendStub>();
#endif
}

} // namespace karma::physics_backend
