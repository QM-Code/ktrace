#include "physics/ecs_sync_system.hpp"

#include "karma/common/logging/logging.hpp"
#include "karma/ecs/world.hpp"
#include "karma/physics/physics_system.hpp"
#include "karma/scene/components.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_set>
#include <vector>

#include <glm/gtc/quaternion.hpp>

namespace karma::physics {

namespace detail {

enum class RuntimeCommandTraceOutcome : uint8_t {
    None = 0,
    StaleRuntimeBindingBody,
    IneligibleNonDynamic,
    IneligibleKinematic,
    RuntimeApplyFailed,
    RecoveryApplied
};

enum class RuntimeCommandTraceOperation : uint8_t {
    None = 0,
    LinearForce,
    LinearImpulse,
    AngularTorque,
    AngularImpulse
};

enum class RuntimeCommandTraceStage : uint8_t {
    Create = 0,
    Update,
    Recovery
};

enum class RuntimeCommandTraceFailureCause : uint8_t {
    None = 0,
    StaleBinding,
    BackendReject
};

const char* RuntimeCommandTraceStageTag(RuntimeCommandTraceStage stage) {
    switch (stage) {
        case RuntimeCommandTraceStage::Create:
            return "create";
        case RuntimeCommandTraceStage::Update:
            return "update";
        case RuntimeCommandTraceStage::Recovery:
            return "recovery";
        default:
            return "unknown";
    }
}

RuntimeCommandTraceStage ClassifyRuntimeCommandTraceStage(bool has_existing_binding,
                                                          bool recovering_from_command_failure) {
    if (recovering_from_command_failure) {
        return RuntimeCommandTraceStage::Recovery;
    }
    return has_existing_binding ? RuntimeCommandTraceStage::Update : RuntimeCommandTraceStage::Create;
}

const char* ClassifyRuntimeCommandTraceStageTag(bool has_existing_binding, bool recovering_from_command_failure) {
    return RuntimeCommandTraceStageTag(
        ClassifyRuntimeCommandTraceStage(has_existing_binding, recovering_from_command_failure));
}

const char* RuntimeCommandTraceOperationTag(RuntimeCommandTraceOperation operation) {
    switch (operation) {
        case RuntimeCommandTraceOperation::None:
            return "none";
        case RuntimeCommandTraceOperation::LinearForce:
            return "linear_force";
        case RuntimeCommandTraceOperation::LinearImpulse:
            return "linear_impulse";
        case RuntimeCommandTraceOperation::AngularTorque:
            return "angular_torque";
        case RuntimeCommandTraceOperation::AngularImpulse:
            return "angular_impulse";
        default:
            return "unknown";
    }
}

const char* RuntimeCommandTraceOutcomeTag(RuntimeCommandTraceOutcome outcome) {
    switch (outcome) {
        case RuntimeCommandTraceOutcome::None:
            return "none";
        case RuntimeCommandTraceOutcome::StaleRuntimeBindingBody:
            return "stale_runtime_binding_body";
        case RuntimeCommandTraceOutcome::IneligibleNonDynamic:
            return "ineligible_non_dynamic";
        case RuntimeCommandTraceOutcome::IneligibleKinematic:
            return "ineligible_kinematic";
        case RuntimeCommandTraceOutcome::RuntimeApplyFailed:
            return "runtime_apply_failed";
        case RuntimeCommandTraceOutcome::RecoveryApplied:
            return "recovery_applied";
        default:
            return "unknown";
    }
}

const char* RuntimeCommandTraceFailureCauseTag(RuntimeCommandTraceFailureCause cause) {
    switch (cause) {
        case RuntimeCommandTraceFailureCause::None:
            return "none";
        case RuntimeCommandTraceFailureCause::StaleBinding:
            return "stale_binding";
        case RuntimeCommandTraceFailureCause::BackendReject:
            return "backend_reject";
        default:
            return "unknown";
    }
}

RuntimeCommandTraceOperation ClassifyRuntimeCommandTraceOperation(bool has_linear_force,
                                                                  bool has_linear_impulse,
                                                                  bool has_angular_torque,
                                                                  bool has_angular_impulse) {
    if (has_linear_force) {
        return RuntimeCommandTraceOperation::LinearForce;
    }
    if (has_linear_impulse) {
        return RuntimeCommandTraceOperation::LinearImpulse;
    }
    if (has_angular_torque) {
        return RuntimeCommandTraceOperation::AngularTorque;
    }
    if (has_angular_impulse) {
        return RuntimeCommandTraceOperation::AngularImpulse;
    }
    return RuntimeCommandTraceOperation::None;
}

const char* ClassifyRuntimeCommandTraceOperationTag(bool has_linear_force,
                                                    bool has_linear_impulse,
                                                    bool has_angular_torque,
                                                    bool has_angular_impulse) {
    return RuntimeCommandTraceOperationTag(ClassifyRuntimeCommandTraceOperation(has_linear_force,
                                                                                has_linear_impulse,
                                                                                has_angular_torque,
                                                                                has_angular_impulse));
}

RuntimeCommandTraceOutcome ClassifyRuntimeCommandTraceOutcome(bool has_pending_commands,
                                                             bool is_dynamic,
                                                             bool is_kinematic,
                                                             bool stale_runtime_binding_body,
                                                             bool runtime_apply_failed,
                                                             bool recovery_applied) {
    if (recovery_applied) {
        return RuntimeCommandTraceOutcome::RecoveryApplied;
    }
    if (!has_pending_commands) {
        return RuntimeCommandTraceOutcome::None;
    }
    if (stale_runtime_binding_body) {
        return RuntimeCommandTraceOutcome::StaleRuntimeBindingBody;
    }
    if (!is_dynamic) {
        return RuntimeCommandTraceOutcome::IneligibleNonDynamic;
    }
    if (is_kinematic) {
        return RuntimeCommandTraceOutcome::IneligibleKinematic;
    }
    if (runtime_apply_failed) {
        return RuntimeCommandTraceOutcome::RuntimeApplyFailed;
    }
    return RuntimeCommandTraceOutcome::None;
}

const char* ClassifyRuntimeCommandTraceOutcomeTag(bool has_pending_commands,
                                                  bool is_dynamic,
                                                  bool is_kinematic,
                                                  bool stale_runtime_binding_body,
                                                  bool runtime_apply_failed,
                                                  bool recovery_applied) {
    return RuntimeCommandTraceOutcomeTag(ClassifyRuntimeCommandTraceOutcome(has_pending_commands,
                                                                            is_dynamic,
                                                                            is_kinematic,
                                                                            stale_runtime_binding_body,
                                                                            runtime_apply_failed,
                                                                            recovery_applied));
}

RuntimeCommandTraceFailureCause ClassifyRuntimeCommandTraceFailureCause(bool stale_runtime_binding_body,
                                                                        bool runtime_apply_failed) {
    if (stale_runtime_binding_body) {
        return RuntimeCommandTraceFailureCause::StaleBinding;
    }
    if (runtime_apply_failed) {
        return RuntimeCommandTraceFailureCause::BackendReject;
    }
    return RuntimeCommandTraceFailureCause::None;
}

const char* ClassifyRuntimeCommandTraceFailureCauseTag(bool stale_runtime_binding_body, bool runtime_apply_failed) {
    return RuntimeCommandTraceFailureCauseTag(
        ClassifyRuntimeCommandTraceFailureCause(stale_runtime_binding_body, runtime_apply_failed));
}

bool IsRuntimeCommandTraceFailureOutcome(RuntimeCommandTraceOutcome outcome) {
    return outcome == RuntimeCommandTraceOutcome::StaleRuntimeBindingBody
           || outcome == RuntimeCommandTraceOutcome::RuntimeApplyFailed;
}

} // namespace detail

namespace {

bool EntityLess(const ecs::Entity& lhs, const ecs::Entity& rhs) {
    if (lhs.index != rhs.index) {
        return lhs.index < rhs.index;
    }
    return lhs.generation < rhs.generation;
}

bool IsFiniteMat4(const glm::mat4& value) {
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            if (!std::isfinite(value[c][r])) {
                return false;
            }
        }
    }
    return true;
}

bool ReadSceneTransform(const scene::TransformComponent& transform, physics_backend::BodyTransform& out_transform) {
    if (!IsFiniteMat4(transform.world)) {
        return false;
    }

    out_transform.position = glm::vec3(transform.world[3]);
    out_transform.rotation = glm::normalize(glm::quat_cast(glm::mat3(transform.world)));
    return scene::IsFiniteVec3(out_transform.position)
           && scene::IsFiniteScalar(out_transform.rotation.w)
           && scene::IsFiniteScalar(out_transform.rotation.x)
           && scene::IsFiniteScalar(out_transform.rotation.y)
           && scene::IsFiniteScalar(out_transform.rotation.z);
}

void WriteSceneTransform(scene::TransformComponent& transform, const physics_backend::BodyTransform& body_transform) {
    glm::mat4 matrix = glm::mat4_cast(glm::normalize(body_transform.rotation));
    matrix[3] = glm::vec4(body_transform.position, 1.0f);
    transform.local = matrix;
    transform.world = matrix;
}

bool IsColliderShapeSupportedForRuntime(const scene::RigidBodyIntentComponent& rigidbody,
                                        const scene::ColliderIntentComponent& collider) {
    switch (collider.shape) {
        case scene::ColliderShapeKind::Box:
        case scene::ColliderShapeKind::Sphere:
        case scene::ColliderShapeKind::Capsule:
            return true;
        case scene::ColliderShapeKind::Mesh:
            return !rigidbody.dynamic;
        default:
            return false;
    }
}

bool RequiresRigidBodyRebuild(const scene::RigidBodyIntentComponent& previous,
                              const scene::RigidBodyIntentComponent& desired) {
    if (previous.dynamic != desired.dynamic) {
        return true;
    }
    if (desired.dynamic && !scene::NearlyEqualFloat(previous.mass, desired.mass)) {
        return true;
    }
    return false;
}

bool IsRejectedControllerCompatibility(scene::ControllerColliderCompatibility compatibility) {
    return compatibility != scene::ControllerColliderCompatibility::Compatible
           && compatibility != scene::ControllerColliderCompatibility::CompatibleControllerDisabled
           && compatibility != scene::ControllerColliderCompatibility::EnabledControllerRequiresDynamicRigidBody;
}

bool ShouldRetainControllerRuntimeBinding(scene::ControllerColliderCompatibility compatibility) {
    return compatibility == scene::ControllerColliderCompatibility::Compatible
           || compatibility == scene::ControllerColliderCompatibility::CompatibleControllerDisabled;
}

bool ShouldUseControllerRuntimeGeometry(const scene::PlayerControllerIntentComponent* controller,
                                        scene::ControllerColliderCompatibility compatibility) {
    return controller != nullptr
           && controller->enabled
           && compatibility == scene::ControllerColliderCompatibility::Compatible;
}

void ResolveVelocityOwnership(const scene::RigidBodyIntentComponent& rigidbody,
                              const scene::PlayerControllerIntentComponent* controller,
                              scene::ControllerColliderCompatibility compatibility,
                              scene::ControllerVelocityOwnership& out_ownership,
                              glm::vec3& out_linear_velocity,
                              glm::vec3& out_angular_velocity) {
    out_ownership = scene::ClassifyControllerVelocityOwnership(controller, compatibility);
    if (scene::IsControllerVelocityOwner(out_ownership) && controller) {
        out_linear_velocity = controller->desired_velocity;
        out_angular_velocity = glm::vec3(0.0f, 0.0f, 0.0f);
        return;
    }

    out_linear_velocity = rigidbody.linear_velocity;
    out_angular_velocity = rigidbody.angular_velocity;
}

bool ApplyRuntimeVelocityPolicy(PhysicsSystem& physics_system,
                                physics_backend::BodyId body,
                                const scene::RigidBodyIntentComponent& rigidbody,
                                const scene::PlayerControllerIntentComponent* controller,
                                scene::ControllerColliderCompatibility compatibility,
                                scene::ControllerVelocityOwnership& out_ownership,
                                glm::vec3& out_linear_velocity,
                                glm::vec3& out_angular_velocity) {
    ResolveVelocityOwnership(
        rigidbody, controller, compatibility, out_ownership, out_linear_velocity, out_angular_velocity);
    if (!rigidbody.dynamic) {
        return true;
    }

    return physics_system.setBodyLinearVelocity(body, out_linear_velocity)
           && physics_system.setBodyAngularVelocity(body, out_angular_velocity);
}

struct RuntimeCommandApplyResult {
    bool success = true;
    bool has_pending_commands = false;
    detail::RuntimeCommandTraceOperation trace_operation = detail::RuntimeCommandTraceOperation::None;
    detail::RuntimeCommandTraceOutcome trace_outcome = detail::RuntimeCommandTraceOutcome::None;
    detail::RuntimeCommandTraceFailureCause trace_failure_cause = detail::RuntimeCommandTraceFailureCause::None;
};

bool IsStaleRuntimeBindingBody(PhysicsSystem& physics_system, physics_backend::BodyId body) {
    physics_backend::BodyTransform probe{};
    return !physics_system.getBodyTransform(body, probe);
}

void TraceRuntimeCommandOutcome(ecs::Entity entity,
                                physics_backend::BodyId body,
                                detail::RuntimeCommandTraceStage stage,
                                detail::RuntimeCommandTraceOperation operation,
                                detail::RuntimeCommandTraceOutcome outcome,
                                detail::RuntimeCommandTraceFailureCause failure_cause) {
    if (outcome == detail::RuntimeCommandTraceOutcome::None) {
        return;
    }
    if (detail::IsRuntimeCommandTraceFailureOutcome(outcome)) {
        KARMA_TRACE("physics.system",
                    "EcsSyncSystem: runtime-command stage='{}' operation='{}' outcome='{}' cause='{}' entity={} generation={} body={}",
                    detail::RuntimeCommandTraceStageTag(stage),
                    detail::RuntimeCommandTraceOperationTag(operation),
                    detail::RuntimeCommandTraceOutcomeTag(outcome),
                    detail::RuntimeCommandTraceFailureCauseTag(failure_cause),
                    entity.index,
                    entity.generation,
                    body);
        return;
    }
    KARMA_TRACE("physics.system",
                "EcsSyncSystem: runtime-command stage='{}' operation='{}' outcome='{}' entity={} generation={} body={}",
                detail::RuntimeCommandTraceStageTag(stage),
                detail::RuntimeCommandTraceOperationTag(operation),
                detail::RuntimeCommandTraceOutcomeTag(outcome),
                entity.index,
                entity.generation,
                body);
}

RuntimeCommandApplyResult ApplyRuntimeForceImpulseCommands(PhysicsSystem& physics_system,
                                                           physics_backend::BodyId body,
                                                           scene::RigidBodyIntentComponent& rigidbody) {
    RuntimeCommandApplyResult result{};
    if (scene::HasRuntimeCommandClearRequest(rigidbody)) {
        scene::ClearRuntimeCommandIntents(rigidbody);
        return result;
    }

    const bool has_force = scene::HasRuntimeLinearForceCommand(rigidbody);
    const bool has_impulse = scene::HasRuntimeLinearImpulseCommand(rigidbody);
    const bool has_torque = scene::HasRuntimeAngularTorqueCommand(rigidbody);
    const bool has_angular_impulse = scene::HasRuntimeAngularImpulseCommand(rigidbody);
    result.has_pending_commands = has_force || has_impulse || has_torque || has_angular_impulse;
    result.trace_operation =
        detail::ClassifyRuntimeCommandTraceOperation(has_force, has_impulse, has_torque, has_angular_impulse);
    if (!has_force && !has_impulse && !has_torque && !has_angular_impulse) {
        return result;
    }

    // Runtime command intents on ineligible bodies are stably preserved until clear/reset or eligibility transition.
    if (!rigidbody.dynamic) {
        result.trace_outcome = detail::RuntimeCommandTraceOutcome::IneligibleNonDynamic;
        return result;
    }
    if (rigidbody.kinematic) {
        result.trace_outcome = detail::RuntimeCommandTraceOutcome::IneligibleKinematic;
        return result;
    }

    if (has_force && !physics_system.addBodyForce(body, rigidbody.linear_force)) {
        const bool stale_runtime_binding_body = IsStaleRuntimeBindingBody(physics_system, body);
        result.success = false;
        result.trace_operation = detail::RuntimeCommandTraceOperation::LinearForce;
        result.trace_outcome = stale_runtime_binding_body
                                   ? detail::RuntimeCommandTraceOutcome::StaleRuntimeBindingBody
                                   : detail::RuntimeCommandTraceOutcome::RuntimeApplyFailed;
        result.trace_failure_cause = detail::ClassifyRuntimeCommandTraceFailureCause(stale_runtime_binding_body, true);
        return result;
    }
    if (has_impulse) {
        if (!physics_system.addBodyLinearImpulse(body, rigidbody.linear_impulse)) {
            const bool stale_runtime_binding_body = IsStaleRuntimeBindingBody(physics_system, body);
            result.success = false;
            result.trace_operation = detail::RuntimeCommandTraceOperation::LinearImpulse;
            result.trace_outcome = stale_runtime_binding_body
                                       ? detail::RuntimeCommandTraceOutcome::StaleRuntimeBindingBody
                                       : detail::RuntimeCommandTraceOutcome::RuntimeApplyFailed;
            result.trace_failure_cause =
                detail::ClassifyRuntimeCommandTraceFailureCause(stale_runtime_binding_body, true);
            return result;
        }
        rigidbody.linear_impulse = glm::vec3(0.0f, 0.0f, 0.0f);
    }
    if (has_torque && !physics_system.addBodyTorque(body, rigidbody.angular_torque)) {
        const bool stale_runtime_binding_body = IsStaleRuntimeBindingBody(physics_system, body);
        result.success = false;
        result.trace_operation = detail::RuntimeCommandTraceOperation::AngularTorque;
        result.trace_outcome = stale_runtime_binding_body
                                   ? detail::RuntimeCommandTraceOutcome::StaleRuntimeBindingBody
                                   : detail::RuntimeCommandTraceOutcome::RuntimeApplyFailed;
        result.trace_failure_cause = detail::ClassifyRuntimeCommandTraceFailureCause(stale_runtime_binding_body, true);
        return result;
    }
    if (has_angular_impulse) {
        if (!physics_system.addBodyAngularImpulse(body, rigidbody.angular_impulse)) {
            const bool stale_runtime_binding_body = IsStaleRuntimeBindingBody(physics_system, body);
            result.success = false;
            result.trace_operation = detail::RuntimeCommandTraceOperation::AngularImpulse;
            result.trace_outcome = stale_runtime_binding_body
                                       ? detail::RuntimeCommandTraceOutcome::StaleRuntimeBindingBody
                                       : detail::RuntimeCommandTraceOutcome::RuntimeApplyFailed;
            result.trace_failure_cause =
                detail::ClassifyRuntimeCommandTraceFailureCause(stale_runtime_binding_body, true);
            return result;
        }
        rigidbody.angular_impulse = glm::vec3(0.0f, 0.0f, 0.0f);
    }
    return result;
}

struct PreservedRuntimeState {
    physics_backend::BodyTransform transform{};
    glm::vec3 linear_velocity{0.0f, 0.0f, 0.0f};
    glm::vec3 angular_velocity{0.0f, 0.0f, 0.0f};
};

glm::vec3 ResolveControllerBoxHalfExtents(const scene::PlayerControllerIntentComponent& controller) {
    return controller.half_extents;
}

glm::vec3 ResolveControllerShapeLocalCenter(const scene::PlayerControllerIntentComponent& controller) {
    return controller.center;
}

void ResolveControllerCapsuleShape(const scene::PlayerControllerIntentComponent& controller,
                                   float& out_radius,
                                   float& out_half_height) {
    const glm::vec3 extents = controller.half_extents;
    out_radius = std::max(extents.x, extents.z);
    out_half_height = std::max(0.01f, extents.y - out_radius);
}

bool CaptureRuntimeStateForRebuild(PhysicsSystem& physics_system,
                                   physics_backend::BodyId body,
                                   const scene::RigidBodyIntentComponent& rigidbody,
                                   PreservedRuntimeState& out_state) {
    if (!physics_system.getBodyTransform(body, out_state.transform)) {
        return false;
    }

    if (!rigidbody.dynamic) {
        out_state.linear_velocity = rigidbody.linear_velocity;
        out_state.angular_velocity = rigidbody.angular_velocity;
        return true;
    }

    return physics_system.getBodyLinearVelocity(body, out_state.linear_velocity)
           && physics_system.getBodyAngularVelocity(body, out_state.angular_velocity);
}

void BuildBodyDesc(const scene::RigidBodyIntentComponent& rigidbody,
                   const scene::ColliderIntentComponent& collider,
                   const scene::PlayerControllerIntentComponent* controller,
                   scene::ControllerColliderCompatibility controller_compatibility,
                   const physics_backend::BodyTransform& transform,
                   const glm::vec3& linear_velocity,
                   const glm::vec3& angular_velocity,
                   physics_backend::BodyDesc& out_desc) {
    const bool use_controller_runtime_geometry =
        ShouldUseControllerRuntimeGeometry(controller, controller_compatibility);
    out_desc = physics_backend::BodyDesc{};
    out_desc.transform = transform;
    out_desc.linear_velocity = linear_velocity;
    out_desc.angular_velocity = angular_velocity;
    out_desc.linear_damping = rigidbody.linear_damping;
    out_desc.angular_damping = rigidbody.angular_damping;
    out_desc.mass = rigidbody.dynamic ? rigidbody.mass : 0.0f;
    out_desc.is_static = !rigidbody.dynamic;
    out_desc.is_kinematic = rigidbody.dynamic && rigidbody.kinematic;
    out_desc.awake = rigidbody.dynamic && rigidbody.awake;
    out_desc.gravity_enabled = rigidbody.dynamic && rigidbody.gravity_enabled;
    out_desc.rotation_locked = rigidbody.rotation_locked;
    out_desc.translation_locked = rigidbody.translation_locked;
    out_desc.is_trigger = collider.is_trigger;
    out_desc.friction = collider.friction;
    out_desc.restitution = collider.restitution;
    out_desc.collision_mask.layer = collider.mask.layer;
    out_desc.collision_mask.collides_with = collider.mask.collides_with;
    out_desc.collider_shape.local_center = glm::vec3(0.0f, 0.0f, 0.0f);

    switch (collider.shape) {
        case scene::ColliderShapeKind::Box:
            out_desc.collider_shape.kind = physics_backend::ColliderShapeKind::Box;
            if (use_controller_runtime_geometry && controller) {
                out_desc.collider_shape.box_half_extents = ResolveControllerBoxHalfExtents(*controller);
                out_desc.collider_shape.local_center = ResolveControllerShapeLocalCenter(*controller);
            } else {
                out_desc.collider_shape.box_half_extents = collider.half_extents;
            }
            break;
        case scene::ColliderShapeKind::Sphere:
            out_desc.collider_shape.kind = physics_backend::ColliderShapeKind::Sphere;
            out_desc.collider_shape.sphere_radius = collider.radius;
            break;
        case scene::ColliderShapeKind::Capsule:
            out_desc.collider_shape.kind = physics_backend::ColliderShapeKind::Capsule;
            if (use_controller_runtime_geometry && controller) {
                ResolveControllerCapsuleShape(
                    *controller, out_desc.collider_shape.capsule_radius, out_desc.collider_shape.capsule_half_height);
                out_desc.collider_shape.local_center = ResolveControllerShapeLocalCenter(*controller);
            } else {
                out_desc.collider_shape.capsule_radius = collider.radius;
                out_desc.collider_shape.capsule_half_height = collider.half_height;
            }
            break;
        case scene::ColliderShapeKind::Mesh:
            // Bounded placeholder path: mesh intent maps to static box proxy until mesh ingestion lands.
            out_desc.collider_shape.kind = physics_backend::ColliderShapeKind::Box;
            out_desc.collider_shape.box_half_extents = glm::vec3(0.5f, 0.5f, 0.5f);
            break;
        default:
            break;
    }
}

} // namespace

EcsSyncSystem::EcsSyncSystem(PhysicsSystem& physics_system)
    : physics_system_(physics_system) {}

size_t EcsSyncSystem::EntityHash::operator()(ecs::Entity entity) const noexcept {
    const uint64_t combined = (static_cast<uint64_t>(entity.generation) << 32u)
                              | static_cast<uint64_t>(entity.index);
    return std::hash<uint64_t>{}(combined);
}

void EcsSyncSystem::clear() {
    for (auto& [entity, binding] : bindings_) {
        (void)entity;
        if (binding.body != physics_backend::kInvalidBodyId) {
            physics_system_.destroyBody(binding.body);
        }
    }
    bindings_.clear();
}

bool EcsSyncSystem::hasRuntimeBinding(ecs::Entity entity) const {
    return bindings_.find(entity) != bindings_.end();
}

size_t EcsSyncSystem::runtimeBindingCount() const {
    return bindings_.size();
}

bool EcsSyncSystem::tryGetRuntimeBody(ecs::Entity entity, physics_backend::BodyId& out_body) const {
    const auto it = bindings_.find(entity);
    if (it == bindings_.end()) {
        return false;
    }
    out_body = it->second.body;
    return out_body != physics_backend::kInvalidBodyId;
}

bool EcsSyncSystem::tryGetRuntimeTransformSnapshot(ecs::Entity entity,
                                                   physics_backend::BodyTransform& out_transform) const {
    const auto it = bindings_.find(entity);
    if (it == bindings_.end()) {
        return false;
    }
    out_transform = it->second.last_transform;
    return true;
}

bool EcsSyncSystem::hasControllerRuntimeBinding(ecs::Entity entity) const {
    const auto it = bindings_.find(entity);
    return it != bindings_.end() && it->second.controller_runtime_bound;
}

size_t EcsSyncSystem::controllerRuntimeBindingCount() const {
    size_t count = 0;
    for (const auto& [entity, binding] : bindings_) {
        (void)entity;
        if (binding.controller_runtime_bound) {
            ++count;
        }
    }
    return count;
}

bool EcsSyncSystem::tryGetControllerCompatibility(
    ecs::Entity entity,
    scene::ControllerColliderCompatibility& out_compatibility) const {
    const auto it = bindings_.find(entity);
    if (it == bindings_.end() || !it->second.controller_runtime_bound) {
        return false;
    }
    out_compatibility = it->second.controller_compatibility;
    return true;
}

void EcsSyncSystem::preSimulate(ecs::World& world) {
    std::vector<ecs::Entity> candidates =
        world.view<scene::TransformComponent,
                   scene::RigidBodyIntentComponent,
                   scene::ColliderIntentComponent,
                   scene::PhysicsTransformOwnershipComponent>();
    std::sort(candidates.begin(), candidates.end(), EntityLess);

    std::unordered_set<ecs::Entity, EntityHash> accepted_entities{};
    std::vector<ecs::Entity> teardown_entities{};

    for (const ecs::Entity entity : candidates) {
        auto* transform = world.tryGet<scene::TransformComponent>(entity);
        auto* rigidbody = world.tryGet<scene::RigidBodyIntentComponent>(entity);
        auto* collider = world.tryGet<scene::ColliderIntentComponent>(entity);
        auto* ownership = world.tryGet<scene::PhysicsTransformOwnershipComponent>(entity);
        if (!transform || !rigidbody || !collider || !ownership) {
            teardown_entities.push_back(entity);
            continue;
        }

        if (!scene::ValidateRigidBodyIntent(*rigidbody)
            || !scene::ValidateColliderIntent(*collider)
            || !scene::ValidateTransformOwnership(*ownership)
            || !IsColliderShapeSupportedForRuntime(*rigidbody, *collider)) {
            teardown_entities.push_back(entity);
            continue;
        }

        if (!collider->enabled) {
            // Bounded Phase 3 contract: disabled colliders do not retain runtime objects.
            teardown_entities.push_back(entity);
            continue;
        }

        auto* controller = world.tryGet<scene::PlayerControllerIntentComponent>(entity);
        scene::ControllerColliderCompatibility controller_compatibility =
            scene::ControllerColliderCompatibility::Compatible;
        const bool has_controller_component = controller != nullptr;
        if (controller) {
            controller_compatibility = scene::ClassifyControllerColliderCompatibility(*controller, collider, rigidbody);
            if (IsRejectedControllerCompatibility(controller_compatibility)) {
                teardown_entities.push_back(entity);
                continue;
            }
        }
        const bool retain_controller_binding =
            has_controller_component && ShouldRetainControllerRuntimeBinding(controller_compatibility);

        physics_backend::BodyTransform scene_transform{};
        if (!ReadSceneTransform(*transform, scene_transform)) {
            teardown_entities.push_back(entity);
            continue;
        }

        accepted_entities.insert(entity);
        auto binding_it = bindings_.find(entity);

        const auto create_binding = [this,
                                     &entity,
                                     rigidbody,
                                     collider,
                                     ownership,
                                     controller,
                                     controller_compatibility,
                                     retain_controller_binding,
                                     &scene_transform,
                                     &world](const PreservedRuntimeState* preserved_state = nullptr,
                                             bool recovering_from_command_failure = false) -> bool {
            scene::ControllerVelocityOwnership create_velocity_ownership =
                scene::ControllerVelocityOwnership::RigidbodyIntent;
            glm::vec3 create_linear_velocity{0.0f, 0.0f, 0.0f};
            glm::vec3 create_angular_velocity{0.0f, 0.0f, 0.0f};
            ResolveVelocityOwnership(*rigidbody,
                                     controller,
                                     controller_compatibility,
                                     create_velocity_ownership,
                                     create_linear_velocity,
                                     create_angular_velocity);

            physics_backend::BodyTransform create_transform = scene_transform;
            if (preserved_state) {
                create_transform = preserved_state->transform;
                create_linear_velocity = preserved_state->linear_velocity;
                create_angular_velocity = preserved_state->angular_velocity;
            }

            physics_backend::BodyDesc desc{};
            BuildBodyDesc(*rigidbody,
                          *collider,
                          controller,
                          controller_compatibility,
                          create_transform,
                          create_linear_velocity,
                          create_angular_velocity,
                          desc);

            const physics_backend::BodyId body = physics_system_.createBody(desc);
            if (body == physics_backend::kInvalidBodyId) {
                return false;
            }

            RuntimeBinding binding{};
            binding.body = body;
            binding.rigidbody_intent = *rigidbody;
            binding.collider_intent = *collider;
            binding.transform_ownership = *ownership;
            binding.controller_runtime_bound = retain_controller_binding;
            if (retain_controller_binding && controller) {
                binding.controller_intent = *controller;
                binding.controller_compatibility = controller_compatibility;
            } else {
                binding.controller_intent = scene::PlayerControllerIntentComponent{};
                binding.controller_compatibility = scene::ControllerColliderCompatibility::Compatible;
            }
            binding.velocity_ownership = create_velocity_ownership;
            binding.runtime_linear_velocity = create_linear_velocity;
            binding.runtime_angular_velocity = create_angular_velocity;
            if (!preserved_state
                && !ApplyRuntimeVelocityPolicy(physics_system_,
                                               body,
                                               *rigidbody,
                                               controller,
                                               controller_compatibility,
                                               binding.velocity_ownership,
                                               binding.runtime_linear_velocity,
                                               binding.runtime_angular_velocity)) {
                physics_system_.destroyBody(body);
                return false;
            }
            if (rigidbody->dynamic && !physics_system_.setBodyAwake(body, rigidbody->awake)) {
                physics_system_.destroyBody(body);
                return false;
            }
            const RuntimeCommandApplyResult create_command_result =
                ApplyRuntimeForceImpulseCommands(physics_system_, body, *rigidbody);
            const detail::RuntimeCommandTraceStage create_trace_stage =
                detail::ClassifyRuntimeCommandTraceStage(false, recovering_from_command_failure);
            TraceRuntimeCommandOutcome(
                entity,
                body,
                create_trace_stage,
                create_command_result.trace_operation,
                create_command_result.trace_outcome,
                create_command_result.trace_failure_cause);
            if (!create_command_result.success) {
                physics_system_.destroyBody(body);
                return false;
            }
            if (recovering_from_command_failure) {
                TraceRuntimeCommandOutcome(
                    entity,
                    body,
                    detail::RuntimeCommandTraceStage::Recovery,
                    create_command_result.trace_operation,
                    detail::ClassifyRuntimeCommandTraceOutcome(create_command_result.has_pending_commands,
                                                               rigidbody->dynamic,
                                                               rigidbody->kinematic,
                                                               false,
                                                               false,
                                                               true),
                    detail::RuntimeCommandTraceFailureCause::None);
            }
            binding.last_transform = create_transform;
            bindings_[entity] = binding;

            if (!preserved_state && scene::ShouldPushSceneTransformToPhysics(*ownership)) {
                (void)physics_system_.setBodyTransform(body, scene_transform);
                auto* mutable_ownership = world.tryGet<scene::PhysicsTransformOwnershipComponent>(entity);
                if (mutable_ownership) {
                    mutable_ownership->scene_transform_dirty = false;
                    bindings_[entity].transform_ownership.scene_transform_dirty = false;
                }
            }
            return true;
        };

        if (binding_it == bindings_.end()) {
            (void)create_binding();
            continue;
        }

        RuntimeBinding& binding = binding_it->second;
        const scene::ColliderReconcileAction collider_action =
            scene::ClassifyColliderReconcileAction(binding.collider_intent, *collider);
        const bool rigidbody_rebuild = RequiresRigidBodyRebuild(binding.rigidbody_intent, *rigidbody);
        const bool collider_rebuild = collider_action == scene::ColliderReconcileAction::RebuildRuntimeShape;
        const bool previous_controller_runtime_geometry =
            binding.controller_runtime_bound
            && binding.controller_compatibility == scene::ControllerColliderCompatibility::Compatible;
        const bool current_controller_runtime_geometry =
            retain_controller_binding
            && controller_compatibility == scene::ControllerColliderCompatibility::Compatible;
        const bool controller_geometry_source_changed =
            previous_controller_runtime_geometry != current_controller_runtime_geometry;
        scene::ControllerGeometryReconcileAction controller_geometry_action =
            scene::ControllerGeometryReconcileAction::NoOp;
        if (current_controller_runtime_geometry && controller) {
            controller_geometry_action =
                scene::ClassifyControllerGeometryReconcileAction(binding.controller_intent,
                                                                 *controller,
                                                                 controller_compatibility);
        }
        const bool controller_geometry_change_rebuild =
            controller_geometry_action == scene::ControllerGeometryReconcileAction::RebuildRuntimeShape;
        const bool controller_geometry_rebuild =
            controller_geometry_source_changed || controller_geometry_change_rebuild;

        if (collider_action == scene::ColliderReconcileAction::RejectInvalidIntent) {
            teardown_entities.push_back(entity);
            continue;
        }
        if (controller_geometry_action == scene::ControllerGeometryReconcileAction::RejectInvalidIntent) {
            teardown_entities.push_back(entity);
            continue;
        }

        if (collider_action == scene::ColliderReconcileAction::UpdateRuntimeProperties && !collider->enabled) {
            // Deterministic UpdateRuntimeProperties handling: disable means teardown/no-runtime.
            teardown_entities.push_back(entity);
            continue;
        }

        if (rigidbody_rebuild || collider_rebuild || controller_geometry_rebuild) {
            std::optional<PreservedRuntimeState> preserved_state{};
            if (controller_geometry_change_rebuild && !rigidbody_rebuild && !collider_rebuild) {
                PreservedRuntimeState captured_state{};
                if (!CaptureRuntimeStateForRebuild(physics_system_, binding.body, *rigidbody, captured_state)) {
                    teardown_entities.push_back(entity);
                    continue;
                }
                preserved_state = captured_state;
            }
            physics_system_.destroyBody(binding.body);
            bindings_.erase(binding_it);
            (void)create_binding(preserved_state ? &(*preserved_state) : nullptr);
            continue;
        }

        if (collider_action == scene::ColliderReconcileAction::UpdateRuntimeProperties) {
            const bool trigger_update_ok = physics_system_.setBodyTrigger(binding.body, collider->is_trigger);
            physics_backend::CollisionMask collision_mask{};
            collision_mask.layer = collider->mask.layer;
            collision_mask.collides_with = collider->mask.collides_with;
            const bool filter_update_ok = physics_system_.setBodyCollisionMask(binding.body, collision_mask);
            const bool friction_update_ok = physics_system_.setBodyFriction(binding.body, collider->friction);
            const bool restitution_update_ok =
                physics_system_.setBodyRestitution(binding.body, collider->restitution);
            if (!trigger_update_ok || !filter_update_ok || !friction_update_ok || !restitution_update_ok) {
                // Deterministic fallback path for unsupported runtime mutation: rebuild runtime object.
                physics_system_.destroyBody(binding.body);
                bindings_.erase(binding_it);
                (void)create_binding();
                continue;
            }
        }

        if (rigidbody->dynamic && binding.rigidbody_intent.gravity_enabled != rigidbody->gravity_enabled) {
            (void)physics_system_.setBodyGravityEnabled(binding.body, rigidbody->gravity_enabled);
        }
        const scene::RigidBodyKinematicReconcileAction kinematic_action =
            scene::ClassifyRigidBodyKinematicReconcileAction(binding.rigidbody_intent, *rigidbody);
        if (kinematic_action == scene::RigidBodyKinematicReconcileAction::RejectInvalidIntent) {
            teardown_entities.push_back(entity);
            continue;
        }
        if (kinematic_action == scene::RigidBodyKinematicReconcileAction::UpdateRuntimeKinematic) {
            if (!physics_system_.setBodyKinematic(binding.body, rigidbody->kinematic)) {
                // Deterministic fallback path for runtime mutation failure: rebuild runtime object.
                physics_system_.destroyBody(binding.body);
                bindings_.erase(binding_it);
                (void)create_binding();
                continue;
            }
        }
        if (rigidbody->dynamic
            && (!scene::NearlyEqualFloat(binding.rigidbody_intent.linear_damping, rigidbody->linear_damping)
                || !scene::NearlyEqualFloat(binding.rigidbody_intent.angular_damping, rigidbody->angular_damping))) {
            if (!physics_system_.setBodyLinearDamping(binding.body, rigidbody->linear_damping)
                || !physics_system_.setBodyAngularDamping(binding.body, rigidbody->angular_damping)) {
                teardown_entities.push_back(entity);
                continue;
            }
        }
        if (rigidbody->dynamic
            && (binding.rigidbody_intent.rotation_locked != rigidbody->rotation_locked
                || binding.rigidbody_intent.translation_locked != rigidbody->translation_locked)) {
            bool rotation_lock_ok = true;
            bool translation_lock_ok = true;
            if (binding.rigidbody_intent.rotation_locked != rigidbody->rotation_locked) {
                rotation_lock_ok = physics_system_.setBodyRotationLocked(binding.body, rigidbody->rotation_locked);
            }
            if (binding.rigidbody_intent.translation_locked != rigidbody->translation_locked) {
                translation_lock_ok = physics_system_.setBodyTranslationLocked(
                    binding.body, rigidbody->translation_locked);
            }
            if (!rotation_lock_ok || !translation_lock_ok) {
                // Deterministic fallback path for unsupported runtime lock mutation: rebuild runtime object.
                physics_system_.destroyBody(binding.body);
                bindings_.erase(binding_it);
                (void)create_binding();
                continue;
            }
        }

        if (scene::ShouldPushSceneTransformToPhysics(*ownership)) {
            if (physics_system_.setBodyTransform(binding.body, scene_transform)) {
                binding.last_transform = scene_transform;
                auto* mutable_ownership = world.tryGet<scene::PhysicsTransformOwnershipComponent>(entity);
                if (mutable_ownership) {
                    mutable_ownership->scene_transform_dirty = false;
                }
                binding.transform_ownership = *ownership;
                binding.transform_ownership.scene_transform_dirty = false;
            }
        } else {
            binding.transform_ownership = *ownership;
        }

        if (!ApplyRuntimeVelocityPolicy(physics_system_,
                                        binding.body,
                                        *rigidbody,
                                        controller,
                                        controller_compatibility,
                                        binding.velocity_ownership,
                                        binding.runtime_linear_velocity,
                                        binding.runtime_angular_velocity)) {
            // Deterministic fallback path for runtime mutation failure: rebuild runtime object.
            physics_system_.destroyBody(binding.body);
            bindings_.erase(binding_it);
            (void)create_binding();
            continue;
        }
        const scene::RigidBodyAwakeReconcileAction awake_action =
            scene::ClassifyRigidBodyAwakeReconcileAction(binding.rigidbody_intent, *rigidbody);
        if (awake_action == scene::RigidBodyAwakeReconcileAction::RejectInvalidIntent) {
            teardown_entities.push_back(entity);
            continue;
        }
        if (awake_action == scene::RigidBodyAwakeReconcileAction::UpdateRuntimeAwakeState) {
            if (!physics_system_.setBodyAwake(binding.body, rigidbody->awake)) {
                // Deterministic fallback path for runtime mutation failure: rebuild runtime object.
                physics_system_.destroyBody(binding.body);
                bindings_.erase(binding_it);
                (void)create_binding();
                continue;
            }
        }
        const RuntimeCommandApplyResult update_command_result =
            ApplyRuntimeForceImpulseCommands(physics_system_, binding.body, *rigidbody);
        TraceRuntimeCommandOutcome(
            entity,
            binding.body,
            detail::RuntimeCommandTraceStage::Update,
            update_command_result.trace_operation,
            update_command_result.trace_outcome,
            update_command_result.trace_failure_cause);
        if (!update_command_result.success) {
            // Deterministic fallback path for runtime mutation failure: rebuild runtime object.
            physics_system_.destroyBody(binding.body);
            bindings_.erase(binding_it);
            (void)create_binding(nullptr, true);
            continue;
        }

        binding.rigidbody_intent = *rigidbody;
        binding.collider_intent = *collider;
        if (retain_controller_binding && controller) {
            binding.controller_runtime_bound = true;
            binding.controller_intent = *controller;
            binding.controller_compatibility = controller_compatibility;
        } else {
            binding.controller_runtime_bound = false;
            binding.controller_intent = scene::PlayerControllerIntentComponent{};
            binding.controller_compatibility = scene::ControllerColliderCompatibility::Compatible;
        }
    }

    for (const ecs::Entity entity : teardown_entities) {
        const auto it = bindings_.find(entity);
        if (it == bindings_.end()) {
            continue;
        }
        physics_system_.destroyBody(it->second.body);
        bindings_.erase(it);
    }

    for (auto it = bindings_.begin(); it != bindings_.end();) {
        if (!world.isAlive(it->first) || accepted_entities.find(it->first) == accepted_entities.end()) {
            physics_system_.destroyBody(it->second.body);
            it = bindings_.erase(it);
            continue;
        }
        ++it;
    }
}

void EcsSyncSystem::postSimulate(ecs::World& world) {
    std::vector<ecs::Entity> entities{};
    entities.reserve(bindings_.size());
    for (const auto& [entity, binding] : bindings_) {
        (void)binding;
        entities.push_back(entity);
    }
    std::sort(entities.begin(), entities.end(), EntityLess);

    for (const ecs::Entity entity : entities) {
        auto binding_it = bindings_.find(entity);
        if (binding_it == bindings_.end()) {
            continue;
        }

        if (!world.isAlive(entity)) {
            physics_system_.destroyBody(binding_it->second.body);
            bindings_.erase(binding_it);
            continue;
        }

        auto* transform = world.tryGet<scene::TransformComponent>(entity);
        auto* ownership = world.tryGet<scene::PhysicsTransformOwnershipComponent>(entity);
        if (!transform || !ownership || !scene::ValidateTransformOwnership(*ownership)) {
            physics_system_.destroyBody(binding_it->second.body);
            bindings_.erase(binding_it);
            continue;
        }

        binding_it->second.transform_ownership = *ownership;
        if (!scene::ShouldPullPhysicsTransformToScene(*ownership)) {
            continue;
        }

        physics_backend::BodyTransform runtime_transform{};
        if (!physics_system_.getBodyTransform(binding_it->second.body, runtime_transform)) {
            continue;
        }
        WriteSceneTransform(*transform, runtime_transform);
        binding_it->second.last_transform = runtime_transform;
    }
}

} // namespace karma::physics
