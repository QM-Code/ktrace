#include "physics/backends/backend_factory_internal.hpp"

#include "karma/common/logging/logging.hpp"

#include <string>
#include <unordered_map>

#if defined(KARMA_HAS_PHYSICS_JOLT)
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <thread>

#include <spdlog/spdlog.h>
#endif

namespace karma::physics_backend {
namespace {

#if defined(KARMA_HAS_PHYSICS_JOLT)

constexpr JPH::ObjectLayer kObjectLayerNonMoving = 0;
constexpr JPH::ObjectLayer kObjectLayerMoving = 1;
constexpr JPH::BroadPhaseLayer kBroadPhaseNonMoving(0);
constexpr JPH::BroadPhaseLayer kBroadPhaseMoving(1);
constexpr uint kBroadPhaseLayerCount = 2;

constexpr uint32_t kMaxBodies = 4096;
constexpr uint32_t kNumBodyMutexes = 0;
constexpr uint32_t kMaxBodyPairs = 65536;
constexpr uint32_t kMaxContactConstraints = 8192;

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
 public:
    BroadPhaseLayerInterfaceImpl() {
        object_to_broad_phase_[kObjectLayerNonMoving] = kBroadPhaseNonMoving;
        object_to_broad_phase_[kObjectLayerMoving] = kBroadPhaseMoving;
    }

    uint GetNumBroadPhaseLayers() const override {
        return kBroadPhaseLayerCount;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return object_to_broad_phase_[static_cast<size_t>(layer)];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        if (layer == kBroadPhaseNonMoving) {
            return "NonMoving";
        }
        if (layer == kBroadPhaseMoving) {
            return "Moving";
        }
        return "Unknown";
    }
#endif

 private:
    JPH::BroadPhaseLayer object_to_broad_phase_[2];
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
 public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override {
        return true;
    }
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
 public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer layer) const override {
        return layer == kBroadPhaseNonMoving || layer == kBroadPhaseMoving;
    }
};

JPH::RVec3 ToJoltRVec3(const glm::vec3& value) {
    return JPH::RVec3(value.x, value.y, value.z);
}

JPH::Vec3 ToJoltVec3(const glm::vec3& value) {
    return JPH::Vec3(value.x, value.y, value.z);
}

JPH::Quat ToJoltQuat(const glm::quat& value) {
    return JPH::Quat(value.x, value.y, value.z, value.w);
}

glm::vec3 ToGlmVec3(const JPH::RVec3& value) {
    return glm::vec3(static_cast<float>(value.GetX()),
                     static_cast<float>(value.GetY()),
                     static_cast<float>(value.GetZ()));
}

glm::quat ToGlmQuat(const JPH::Quat& value) {
    return glm::quat(value.GetW(), value.GetX(), value.GetY(), value.GetZ());
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

void JoltTrace(const char* fmt, ...) {
    if (!karma::common::logging::ShouldTraceChannel("physics.jolt")) {
        return;
    }

    va_list list;
    va_start(list, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, list);
    va_end(list);
    KARMA_TRACE("physics.jolt", "{}", buffer);
}

std::mutex g_jolt_runtime_mutex;
int g_jolt_runtime_users = 0;

bool AcquireJoltRuntime() {
    std::lock_guard<std::mutex> lock(g_jolt_runtime_mutex);
    if (g_jolt_runtime_users == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = &JoltTrace;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = [](const char* expr, const char* msg, const char* file, uint line) {
            spdlog::error("Jolt assert failed: {} {} ({}:{})", expr, msg ? msg : "", file, line);
            return true;
        });

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
    ++g_jolt_runtime_users;
    return true;
}

void ReleaseJoltRuntime() {
    std::lock_guard<std::mutex> lock(g_jolt_runtime_mutex);
    if (g_jolt_runtime_users <= 0) {
        return;
    }

    --g_jolt_runtime_users;
    if (g_jolt_runtime_users == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

class JoltBackend final : public Backend {
 public:
    const char* name() const override {
        return "jolt";
    }

    bool init() override {
        if (initialized_) {
            return true;
        }

        if (!AcquireJoltRuntime()) {
            return false;
        }
        runtime_acquired_ = true;

        temp_allocator_ = std::make_unique<JPH::TempAllocatorImpl>(32u * 1024u * 1024u);
        const uint32_t worker_count = std::max(1u, std::thread::hardware_concurrency());
        job_system_ = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers,
            static_cast<int>(worker_count));

        static BroadPhaseLayerInterfaceImpl broad_phase_layers;
        static ObjectVsBroadPhaseLayerFilterImpl object_vs_broad_phase_filter;
        static ObjectLayerPairFilterImpl object_layer_pair_filter;

        physics_system_ = std::make_unique<JPH::PhysicsSystem>();
        physics_system_->Init(
            kMaxBodies,
            kNumBodyMutexes,
            kMaxBodyPairs,
            kMaxContactConstraints,
            broad_phase_layers,
            object_vs_broad_phase_filter,
            object_layer_pair_filter);
        physics_system_->SetGravity(JPH::Vec3(0.0f, -9.8f, 0.0f));

        initialized_ = true;
        KARMA_TRACE("physics.jolt", "PhysicsBackend[jolt]: initialized");
        return true;
    }

    void shutdown() override {
        if (physics_system_) {
            JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
            for (const auto& [id, body_id] : bodies_) {
                (void)id;
                if (body_interface.IsAdded(body_id)) {
                    body_interface.RemoveBody(body_id);
                }
                body_interface.DestroyBody(body_id);
            }
            bodies_.clear();
            dynamic_bodies_.clear();
            gravity_enabled_.clear();
            trigger_enabled_.clear();
            collision_masks_.clear();
            kinematic_enabled_.clear();
            linear_damping_.clear();
            angular_damping_.clear();
            rotation_locked_.clear();
            translation_locked_.clear();
            friction_.clear();
            restitution_.clear();
        }

        physics_system_.reset();
        job_system_.reset();
        temp_allocator_.reset();
        initialized_ = false;

        if (runtime_acquired_) {
            ReleaseJoltRuntime();
            runtime_acquired_ = false;
        }
        KARMA_TRACE("physics.jolt", "PhysicsBackend[jolt]: shutdown");
    }

    void beginFrame(float dt) override {
        frame_dt_ = dt;
    }

    void simulateFixedStep(float fixed_dt) override {
        if (!physics_system_ || fixed_dt <= 0.0f) {
            return;
        }

        const JPH::EPhysicsUpdateError update_error =
            physics_system_->Update(fixed_dt, 1, temp_allocator_.get(), job_system_.get());
        if (update_error != JPH::EPhysicsUpdateError::None) {
            KARMA_TRACE("physics.jolt",
                        "PhysicsBackend[jolt]: update warning dt={:.4f} err=0x{:x}",
                        fixed_dt,
                        static_cast<uint32_t>(update_error));
        }
    }

    void endFrame() override {
        (void)frame_dt_;
        KARMA_TRACE_CHANGED("physics.jolt",
                            std::to_string(bodies_.size()),
                            "PhysicsBackend[jolt]: active bodies={}",
                            bodies_.size());
    }

    BodyId createBody(const BodyDesc& desc) override {
        if (!physics_system_) {
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

        const bool is_dynamic = !desc.is_static;
        if (desc.is_static && desc.is_kinematic) {
            return kInvalidBodyId;
        }
        if (is_dynamic && desc.rotation_locked && desc.translation_locked) {
            return kInvalidBodyId;
        }
        JPH::RefConst<JPH::Shape> shape{};
        switch (desc.collider_shape.kind) {
            case ColliderShapeKind::Box:
                if (!IsFiniteVec3(desc.collider_shape.box_half_extents)
                    || desc.collider_shape.box_half_extents.x <= 0.0f
                    || desc.collider_shape.box_half_extents.y <= 0.0f
                    || desc.collider_shape.box_half_extents.z <= 0.0f) {
                    return kInvalidBodyId;
                }
                shape = new JPH::BoxShape(ToJoltVec3(desc.collider_shape.box_half_extents));
                break;
            case ColliderShapeKind::Sphere:
                if (!std::isfinite(desc.collider_shape.sphere_radius) || desc.collider_shape.sphere_radius <= 0.0f) {
                    return kInvalidBodyId;
                }
                shape = new JPH::SphereShape(desc.collider_shape.sphere_radius);
                break;
            case ColliderShapeKind::Capsule:
                if (!std::isfinite(desc.collider_shape.capsule_radius)
                    || !std::isfinite(desc.collider_shape.capsule_half_height)
                    || desc.collider_shape.capsule_radius <= 0.0f
                    || desc.collider_shape.capsule_half_height <= 0.0f) {
                    return kInvalidBodyId;
                }
                shape = new JPH::CapsuleShape(desc.collider_shape.capsule_half_height,
                                              desc.collider_shape.capsule_radius);
                break;
            default:
                return kInvalidBodyId;
        }

        if (!shape) {
            return kInvalidBodyId;
        }
        if (!IsZeroVec3(desc.collider_shape.local_center)) {
            shape = new JPH::RotatedTranslatedShape(ToJoltVec3(desc.collider_shape.local_center),
                                                    JPH::Quat::sIdentity(),
                                                    shape.GetPtr());
            if (!shape) {
                return kInvalidBodyId;
            }
        }

        JPH::BodyCreationSettings settings(
            shape,
            ToJoltRVec3(desc.transform.position),
            ToJoltQuat(desc.transform.rotation),
            is_dynamic
                ? (desc.is_kinematic ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic)
                : JPH::EMotionType::Static,
            is_dynamic ? kObjectLayerMoving : kObjectLayerNonMoving);
        if (is_dynamic) {
            if (desc.rotation_locked && desc.translation_locked) {
                settings.mAllowedDOFs = JPH::EAllowedDOFs::None;
            } else if (desc.rotation_locked) {
                settings.mAllowedDOFs = JPH::EAllowedDOFs::TranslationX
                                        | JPH::EAllowedDOFs::TranslationY
                                        | JPH::EAllowedDOFs::TranslationZ;
            } else if (desc.translation_locked) {
                settings.mAllowedDOFs = JPH::EAllowedDOFs::RotationX
                                        | JPH::EAllowedDOFs::RotationY
                                        | JPH::EAllowedDOFs::RotationZ;
            }
        }
        settings.mLinearVelocity = ToJoltVec3(desc.linear_velocity);
        settings.mAngularVelocity = ToJoltVec3(desc.angular_velocity);
        settings.mLinearDamping = desc.linear_damping;
        settings.mAngularDamping = desc.angular_damping;
        settings.mGravityFactor = (is_dynamic && desc.gravity_enabled) ? 1.0f : 0.0f;
        if (is_dynamic && desc.mass > 0.0f) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = desc.mass;
        }

        JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
        JPH::Body* body = body_interface.CreateBody(settings);
        if (!body) {
            KARMA_TRACE("physics.jolt", "PhysicsBackend[jolt]: failed to create body");
            return kInvalidBodyId;
        }

        body_interface.AddBody(
            body->GetID(),
            (is_dynamic && desc.awake) ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
        if (is_dynamic && !desc.awake) {
            body_interface.DeactivateBody(body->GetID());
        }

        const BodyId id = next_body_id_++;
        bodies_.emplace(id, body->GetID());
        dynamic_bodies_.emplace(id, is_dynamic);
        gravity_enabled_.emplace(id, is_dynamic && desc.gravity_enabled);
        trigger_enabled_.emplace(id, desc.is_trigger);
        collision_masks_.emplace(id, desc.collision_mask);
        kinematic_enabled_.emplace(id, is_dynamic && desc.is_kinematic);
        if (is_dynamic) {
            linear_damping_.emplace(id, desc.linear_damping);
            angular_damping_.emplace(id, desc.angular_damping);
            rotation_locked_.emplace(id, desc.rotation_locked);
            translation_locked_.emplace(id, desc.translation_locked);
        }
        friction_.emplace(id, desc.friction);
        restitution_.emplace(id, desc.restitution);
        body_interface.SetIsSensor(body->GetID(), desc.is_trigger);
        body_interface.SetFriction(body->GetID(), desc.friction);
        body_interface.SetRestitution(body->GetID(), desc.restitution);
        return id;
    }

    void destroyBody(BodyId body) override {
        if (!physics_system_) {
            return;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return;
        }

        JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
        if (body_interface.IsAdded(it->second)) {
            body_interface.RemoveBody(it->second);
        }
        body_interface.DestroyBody(it->second);
        dynamic_bodies_.erase(body);
        gravity_enabled_.erase(body);
        trigger_enabled_.erase(body);
        collision_masks_.erase(body);
        kinematic_enabled_.erase(body);
        linear_damping_.erase(body);
        angular_damping_.erase(body);
        rotation_locked_.erase(body);
        translation_locked_.erase(body);
        friction_.erase(body);
        restitution_.erase(body);
        bodies_.erase(it);
    }

    bool setBodyTransform(BodyId body, const BodyTransform& transform) override {
        if (!physics_system_) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }

        JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
        body_interface.SetPositionAndRotation(
            it->second,
            ToJoltRVec3(transform.position),
            ToJoltQuat(transform.rotation),
            JPH::EActivation::Activate);
        return true;
    }

    bool getBodyTransform(BodyId body, BodyTransform& out_transform) const override {
        if (!physics_system_) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }

        JPH::BodyLockRead lock(physics_system_->GetBodyLockInterface(), it->second);
        if (!lock.Succeeded()) {
            return false;
        }

        const JPH::Body& physics_body = lock.GetBody();
        out_transform.position = ToGlmVec3(physics_body.GetPosition());
        out_transform.rotation = ToGlmQuat(physics_body.GetRotation());
        return true;
    }

    bool setBodyGravityEnabled(BodyId body, bool enabled) override {
        if (!physics_system_) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !isDynamicBody(body)) {
            return false;
        }

        JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
        body_interface.SetGravityFactor(it->second, enabled ? 1.0f : 0.0f);
        gravity_enabled_[body] = enabled;
        return true;
    }

    bool getBodyGravityEnabled(BodyId body, bool& out_enabled) const override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }

        const auto it = gravity_enabled_.find(body);
        if (it == gravity_enabled_.end()) {
            return false;
        }

        out_enabled = it->second;
        return true;
    }

    bool setBodyKinematic(BodyId body, bool enabled) override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }

        const auto kinematic_it = kinematic_enabled_.find(body);
        if (kinematic_it == kinematic_enabled_.end()) {
            return false;
        }
        if (kinematic_it->second == enabled) {
            return true;
        }

        JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
        const bool currently_awake = body_interface.IsActive(body_it->second);
        body_interface.SetMotionType(body_it->second,
                                     enabled ? JPH::EMotionType::Kinematic : JPH::EMotionType::Dynamic,
                                     currently_awake ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
        kinematic_enabled_[body] = enabled;
        return true;
    }

    bool getBodyKinematic(BodyId body, bool& out_enabled) const override {
        if (!physics_system_ || !isDynamicBody(body)) {
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
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }

        JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
        if (enabled) {
            body_interface.ActivateBody(body_it->second);
        } else {
            body_interface.DeactivateBody(body_it->second);
        }
        return true;
    }

    bool getBodyAwake(BodyId body, bool& out_enabled) const override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }

        out_enabled = physics_system_->GetBodyInterface().IsActive(body_it->second);
        return true;
    }

    bool addBodyForce(BodyId body, const glm::vec3& force) override {
        if (!physics_system_ || !isDynamicBody(body) || !IsFiniteVec3(force)) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }
        const auto kinematic_it = kinematic_enabled_.find(body);
        if (kinematic_it == kinematic_enabled_.end() || kinematic_it->second) {
            return false;
        }
        if (IsZeroVec3(force)) {
            return true;
        }

        physics_system_->GetBodyInterface().AddForce(body_it->second, ToJoltVec3(force));
        return true;
    }

    bool addBodyLinearImpulse(BodyId body, const glm::vec3& impulse) override {
        if (!physics_system_ || !isDynamicBody(body) || !IsFiniteVec3(impulse)) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }
        const auto kinematic_it = kinematic_enabled_.find(body);
        if (kinematic_it == kinematic_enabled_.end() || kinematic_it->second) {
            return false;
        }
        if (IsZeroVec3(impulse)) {
            return true;
        }

        physics_system_->GetBodyInterface().AddImpulse(body_it->second, ToJoltVec3(impulse));
        return true;
    }

    bool addBodyTorque(BodyId body, const glm::vec3& torque) override {
        if (!physics_system_ || !isDynamicBody(body) || !IsFiniteVec3(torque)) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }
        const auto kinematic_it = kinematic_enabled_.find(body);
        if (kinematic_it == kinematic_enabled_.end() || kinematic_it->second) {
            return false;
        }
        if (IsZeroVec3(torque)) {
            return true;
        }

        physics_system_->GetBodyInterface().AddTorque(body_it->second, ToJoltVec3(torque));
        return true;
    }

    bool addBodyAngularImpulse(BodyId body, const glm::vec3& impulse) override {
        if (!physics_system_ || !isDynamicBody(body) || !IsFiniteVec3(impulse)) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }
        const auto kinematic_it = kinematic_enabled_.find(body);
        if (kinematic_it == kinematic_enabled_.end() || kinematic_it->second) {
            return false;
        }
        if (IsZeroVec3(impulse)) {
            return true;
        }

        physics_system_->GetBodyInterface().AddAngularImpulse(body_it->second, ToJoltVec3(impulse));
        return true;
    }

    bool setBodyLinearVelocity(BodyId body, const glm::vec3& velocity) override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }

        physics_system_->GetBodyInterface().SetLinearVelocity(it->second, ToJoltVec3(velocity));
        return true;
    }

    bool getBodyLinearVelocity(BodyId body, glm::vec3& out_velocity) const override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }

        JPH::BodyLockRead lock(physics_system_->GetBodyLockInterface(), it->second);
        if (!lock.Succeeded()) {
            return false;
        }

        out_velocity = ToGlmVec3(lock.GetBody().GetLinearVelocity());
        return true;
    }

    bool setBodyAngularVelocity(BodyId body, const glm::vec3& velocity) override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }

        physics_system_->GetBodyInterface().SetAngularVelocity(it->second, ToJoltVec3(velocity));
        return true;
    }

    bool getBodyAngularVelocity(BodyId body, glm::vec3& out_velocity) const override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }

        JPH::BodyLockRead lock(physics_system_->GetBodyLockInterface(), it->second);
        if (!lock.Succeeded()) {
            return false;
        }

        out_velocity = ToGlmVec3(lock.GetBody().GetAngularVelocity());
        return true;
    }

    bool setBodyTrigger(BodyId body, bool enabled) override {
        if (!physics_system_) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }

        const auto trigger_it = trigger_enabled_.find(body);
        if (trigger_it == trigger_enabled_.end()) {
            return false;
        }

        physics_system_->GetBodyInterface().SetIsSensor(body_it->second, enabled);
        trigger_enabled_[body] = enabled;
        return true;
    }

    bool setBodyLinearDamping(BodyId body, float damping) override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }
        if (!std::isfinite(damping) || damping < 0.0f) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }

        JPH::BodyLockWrite lock(physics_system_->GetBodyLockInterface(), it->second);
        if (!lock.Succeeded()) {
            return false;
        }
        JPH::MotionProperties* motion = lock.GetBody().GetMotionPropertiesUnchecked();
        if (!motion) {
            return false;
        }
        motion->SetLinearDamping(damping);
        linear_damping_[body] = damping;
        return true;
    }

    bool getBodyLinearDamping(BodyId body, float& out_damping) const override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }

        const auto it = linear_damping_.find(body);
        if (it == linear_damping_.end()) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }

        JPH::BodyLockRead lock(physics_system_->GetBodyLockInterface(), body_it->second);
        if (!lock.Succeeded()) {
            return false;
        }
        const JPH::MotionProperties* motion = lock.GetBody().GetMotionPropertiesUnchecked();
        if (!motion) {
            return false;
        }

        out_damping = motion->GetLinearDamping();
        return true;
    }

    bool setBodyAngularDamping(BodyId body, float damping) override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }
        if (!std::isfinite(damping) || damping < 0.0f) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }

        JPH::BodyLockWrite lock(physics_system_->GetBodyLockInterface(), it->second);
        if (!lock.Succeeded()) {
            return false;
        }
        JPH::MotionProperties* motion = lock.GetBody().GetMotionPropertiesUnchecked();
        if (!motion) {
            return false;
        }
        motion->SetAngularDamping(damping);
        angular_damping_[body] = damping;
        return true;
    }

    bool getBodyAngularDamping(BodyId body, float& out_damping) const override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }

        const auto it = angular_damping_.find(body);
        if (it == angular_damping_.end()) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }

        JPH::BodyLockRead lock(physics_system_->GetBodyLockInterface(), body_it->second);
        if (!lock.Succeeded()) {
            return false;
        }
        const JPH::MotionProperties* motion = lock.GetBody().GetMotionPropertiesUnchecked();
        if (!motion) {
            return false;
        }

        out_damping = motion->GetAngularDamping();
        return true;
    }

    bool setBodyRotationLocked(BodyId body, bool locked) override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }
        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }
        const auto lock_it = rotation_locked_.find(body);
        if (lock_it == rotation_locked_.end()) {
            return false;
        }
        if (lock_it->second == locked) {
            return true;
        }
        // Runtime DOF mutation is not exposed cleanly in this bounded Jolt path.
        return false;
    }

    bool getBodyRotationLocked(BodyId body, bool& out_locked) const override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }
        const auto it = rotation_locked_.find(body);
        if (it == rotation_locked_.end()) {
            return false;
        }
        out_locked = it->second;
        return true;
    }

    bool setBodyTranslationLocked(BodyId body, bool locked) override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }
        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }
        const auto lock_it = translation_locked_.find(body);
        if (lock_it == translation_locked_.end()) {
            return false;
        }
        if (lock_it->second == locked) {
            return true;
        }
        // Runtime DOF mutation is not exposed cleanly in this bounded Jolt path.
        return false;
    }

    bool getBodyTranslationLocked(BodyId body, bool& out_locked) const override {
        if (!physics_system_ || !isDynamicBody(body)) {
            return false;
        }
        const auto it = translation_locked_.find(body);
        if (it == translation_locked_.end()) {
            return false;
        }
        out_locked = it->second;
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
        if (!physics_system_) {
            return false;
        }
        if (!IsValidCollisionMask(mask)) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }

        const auto existing_it = collision_masks_.find(body);
        if (existing_it == collision_masks_.end()) {
            return false;
        }

        // Jolt layer-pair filtering in this bounded slice does not expose per-body collides_with mutation.
        // We report unsupported runtime mask transitions so callers can apply deterministic fallback (for example rebuild).
        if (mask.layer != existing_it->second.layer || mask.collides_with != existing_it->second.collides_with) {
            return false;
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
        if (!physics_system_) {
            return false;
        }
        if (!std::isfinite(friction) || friction < 0.0f) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }

        const auto value_it = friction_.find(body);
        if (value_it == friction_.end()) {
            return false;
        }

        physics_system_->GetBodyInterface().SetFriction(body_it->second, friction);
        friction_[body] = friction;
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
        if (!physics_system_) {
            return false;
        }
        if (!std::isfinite(restitution) || restitution < 0.0f || restitution > 1.0f) {
            return false;
        }

        const auto body_it = bodies_.find(body);
        if (body_it == bodies_.end()) {
            return false;
        }

        const auto value_it = restitution_.find(body);
        if (value_it == restitution_.end()) {
            return false;
        }

        // Bounded Jolt path: runtime restitution mutation is treated as unsupported.
        // Callers use false to apply deterministic rebuild fallback.
        if (std::fabs(value_it->second - restitution) > 1e-6f) {
            return false;
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
        if (!physics_system_ || max_distance <= 0.0f) {
            return false;
        }

        const float direction_length = glm::length(direction);
        if (!std::isfinite(direction_length) || direction_length <= 1e-6f) {
            return false;
        }

        const glm::vec3 unit_direction = direction / direction_length;
        const JPH::RRayCast ray(ToJoltRVec3(origin), ToJoltVec3(unit_direction * max_distance));
        JPH::RayCastResult result;
        if (!physics_system_->GetNarrowPhaseQuery().CastRay(ray, result)) {
            return false;
        }

        const BodyId hit_body = findBodyId(result.mBodyID);
        if (hit_body == kInvalidBodyId) {
            return false;
        }

        out_hit.body = hit_body;
        out_hit.fraction = result.mFraction;
        out_hit.distance = max_distance * result.mFraction;
        out_hit.position = ToGlmVec3(ray.GetPointOnRay(result.mFraction));
        return true;
    }

 private:
    BodyId findBodyId(JPH::BodyID body_id) const {
        for (const auto& [id, internal_id] : bodies_) {
            if (internal_id == body_id) {
                return id;
            }
        }
        return kInvalidBodyId;
    }

    bool isDynamicBody(BodyId body) const {
        const auto it = dynamic_bodies_.find(body);
        return it != dynamic_bodies_.end() && it->second;
    }

    std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator_{};
    std::unique_ptr<JPH::JobSystemThreadPool> job_system_{};
    std::unique_ptr<JPH::PhysicsSystem> physics_system_{};
    std::unordered_map<BodyId, JPH::BodyID> bodies_{};
    std::unordered_map<BodyId, bool> dynamic_bodies_{};
    std::unordered_map<BodyId, bool> gravity_enabled_{};
    std::unordered_map<BodyId, bool> trigger_enabled_{};
    std::unordered_map<BodyId, CollisionMask> collision_masks_{};
    std::unordered_map<BodyId, bool> kinematic_enabled_{};
    std::unordered_map<BodyId, float> linear_damping_{};
    std::unordered_map<BodyId, float> angular_damping_{};
    std::unordered_map<BodyId, bool> rotation_locked_{};
    std::unordered_map<BodyId, bool> translation_locked_{};
    std::unordered_map<BodyId, float> friction_{};
    std::unordered_map<BodyId, float> restitution_{};
    BodyId next_body_id_ = 1;
    float frame_dt_ = 0.0f;
    bool initialized_ = false;
    bool runtime_acquired_ = false;
};

#else

class JoltBackendStub final : public Backend {
 public:
    const char* name() const override { return "jolt"; }
    bool init() override {
        KARMA_TRACE("physics.jolt", "PhysicsBackend[jolt]: unavailable (not compiled)");
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

std::unique_ptr<Backend> CreateJoltBackend() {
#if defined(KARMA_HAS_PHYSICS_JOLT)
    return std::make_unique<JoltBackend>();
#else
    return std::make_unique<JoltBackendStub>();
#endif
}

} // namespace karma::physics_backend
