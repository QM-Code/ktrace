#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <string>

#include <glm/glm.hpp>

namespace karma::scene {

enum class ColliderShapeKind {
    Box,
    Sphere,
    Capsule,
    Mesh
};

struct CollisionMaskIntentComponent {
    uint32_t layer = 0x1u;
    uint32_t collides_with = 0xFFFFFFFFu;
};

struct RigidBodyIntentComponent {
    bool dynamic = true;
    bool kinematic = false;
    bool awake = true;
    float mass = 1.0f;
    float linear_damping = 0.0f;
    float angular_damping = 0.0f;
    bool gravity_enabled = true;
    bool rotation_locked = false;
    bool translation_locked = false;
    glm::vec3 linear_velocity{0.0f, 0.0f, 0.0f};
    glm::vec3 angular_velocity{0.0f, 0.0f, 0.0f};
    // Runtime command intents:
    // - linear_force is applied each pre-sim pass while non-zero and linear_force_enabled=true.
    // - linear_impulse is one-shot and consumed only after successful runtime application.
    // - angular_torque is applied each pre-sim pass while non-zero and angular_torque_enabled=true.
    // - angular_impulse is one-shot and consumed only after successful runtime application.
    // - clear_runtime_commands_requested clears force/torque/impulse command intents when reconciliation succeeds.
    glm::vec3 linear_force{0.0f, 0.0f, 0.0f};
    glm::vec3 linear_impulse{0.0f, 0.0f, 0.0f};
    glm::vec3 angular_torque{0.0f, 0.0f, 0.0f};
    glm::vec3 angular_impulse{0.0f, 0.0f, 0.0f};
    bool linear_force_enabled = true;
    bool angular_torque_enabled = true;
    bool clear_runtime_commands_requested = false;
};

struct ColliderIntentComponent {
    ColliderShapeKind shape = ColliderShapeKind::Box;
    bool enabled = true;
    bool is_trigger = false;
    float friction = 0.5f;
    float restitution = 0.0f;
    glm::vec3 half_extents{0.5f, 0.5f, 0.5f};
    float radius = 0.5f;
    float half_height = 0.5f;
    std::string mesh_path{};
    CollisionMaskIntentComponent mask{};
};

struct PlayerControllerIntentComponent {
    bool enabled = true;
    glm::vec3 half_extents{0.5f, 0.9f, 0.5f};
    glm::vec3 center{0.0f, 0.0f, 0.0f};
    glm::vec3 desired_velocity{0.0f, 0.0f, 0.0f};
};

enum class PhysicsTransformAuthority {
    SceneAuthoritative,
    PhysicsAuthoritative
};

struct PhysicsTransformOwnershipComponent {
    PhysicsTransformAuthority authority = PhysicsTransformAuthority::PhysicsAuthoritative;
    bool scene_transform_dirty = false;
    bool push_scene_transform_to_physics = false;
    bool pull_physics_transform_to_scene = true;
};

enum class PhysicsComponentValidationError {
    None,
    NonFiniteValue,
    NonPositiveDimension,
    EmptyMeshPath,
    InvalidMass,
    EmptyCollisionMask,
    ConflictingMotionLocks,
    InvalidKinematicState
};

enum class TransformOwnershipValidationError {
    None,
    SceneAuthorityCannotPullFromPhysics,
    SceneAuthorityDirtyRequiresPush,
    PhysicsAuthorityCannotPushSceneTransform,
    PhysicsAuthorityCannotSetSceneDirty,
    PhysicsAuthorityRequiresPull
};

enum class ColliderReconcileAction {
    NoOp,
    UpdateRuntimeProperties,
    RebuildRuntimeShape,
    RejectInvalidIntent
};

enum class RigidBodyKinematicReconcileAction {
    NoOp,
    UpdateRuntimeKinematic,
    RejectInvalidIntent
};

enum class RigidBodyAwakeReconcileAction {
    NoOp,
    UpdateRuntimeAwakeState,
    RejectInvalidIntent
};

enum class ControllerColliderCompatibility {
    Compatible,
    CompatibleControllerDisabled,
    ControllerInvalid,
    ColliderMissing,
    ColliderInvalid,
    ColliderDisabled,
    ColliderIsTrigger,
    UnsupportedColliderShape,
    EnabledControllerRequiresDynamicRigidBody
};

enum class ControllerVelocityOwnership {
    RigidbodyIntent,
    ControllerIntent
};

enum class ControllerGeometryReconcileAction {
    NoOp,
    RebuildRuntimeShape,
    RejectInvalidIntent
};

inline bool IsFiniteScalar(float value) {
    return std::isfinite(value);
}

inline bool IsFiniteVec3(const glm::vec3& value) {
    return IsFiniteScalar(value.x) && IsFiniteScalar(value.y) && IsFiniteScalar(value.z);
}

inline bool IsNonEmptyPath(const std::string& value) {
    return std::find_if(value.begin(), value.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }) != value.end();
}

inline bool ValidateCollisionMaskIntent(const CollisionMaskIntentComponent& intent,
                                        PhysicsComponentValidationError* out_error = nullptr) {
    if (intent.layer == 0u || intent.collides_with == 0u) {
        if (out_error) {
            *out_error = PhysicsComponentValidationError::EmptyCollisionMask;
        }
        return false;
    }
    if (out_error) {
        *out_error = PhysicsComponentValidationError::None;
    }
    return true;
}

inline bool ValidateRigidBodyIntent(const RigidBodyIntentComponent& intent,
                                    PhysicsComponentValidationError* out_error = nullptr) {
    if (!IsFiniteScalar(intent.mass)
        || !IsFiniteScalar(intent.linear_damping)
        || !IsFiniteScalar(intent.angular_damping)
        || !IsFiniteVec3(intent.linear_velocity)
        || !IsFiniteVec3(intent.angular_velocity)
        || !IsFiniteVec3(intent.linear_force)
        || !IsFiniteVec3(intent.linear_impulse)
        || !IsFiniteVec3(intent.angular_torque)
        || !IsFiniteVec3(intent.angular_impulse)) {
        if (out_error) {
            *out_error = PhysicsComponentValidationError::NonFiniteValue;
        }
        return false;
    }
    if (intent.linear_damping < 0.0f || intent.angular_damping < 0.0f) {
        if (out_error) {
            *out_error = PhysicsComponentValidationError::NonPositiveDimension;
        }
        return false;
    }
    if (intent.dynamic && intent.mass <= 0.0f) {
        if (out_error) {
            *out_error = PhysicsComponentValidationError::InvalidMass;
        }
        return false;
    }
    if (intent.dynamic && intent.rotation_locked && intent.translation_locked) {
        if (out_error) {
            *out_error = PhysicsComponentValidationError::ConflictingMotionLocks;
        }
        return false;
    }
    if (!intent.dynamic && intent.kinematic) {
        if (out_error) {
            *out_error = PhysicsComponentValidationError::InvalidKinematicState;
        }
        return false;
    }
    if (out_error) {
        *out_error = PhysicsComponentValidationError::None;
    }
    return true;
}

inline bool ValidateColliderIntent(const ColliderIntentComponent& intent,
                                   PhysicsComponentValidationError* out_error = nullptr) {
    PhysicsComponentValidationError mask_error = PhysicsComponentValidationError::None;
    if (!ValidateCollisionMaskIntent(intent.mask, &mask_error)) {
        if (out_error) {
            *out_error = mask_error;
        }
        return false;
    }
    if (!IsFiniteScalar(intent.friction) || !IsFiniteScalar(intent.restitution)) {
        if (out_error) {
            *out_error = PhysicsComponentValidationError::NonFiniteValue;
        }
        return false;
    }
    // Material intent contract: friction >= 0, restitution in [0, 1].
    if (intent.friction < 0.0f || intent.restitution < 0.0f || intent.restitution > 1.0f) {
        if (out_error) {
            *out_error = PhysicsComponentValidationError::NonPositiveDimension;
        }
        return false;
    }

    switch (intent.shape) {
        case ColliderShapeKind::Box:
            if (!IsFiniteVec3(intent.half_extents)) {
                if (out_error) {
                    *out_error = PhysicsComponentValidationError::NonFiniteValue;
                }
                return false;
            }
            if (intent.half_extents.x <= 0.0f || intent.half_extents.y <= 0.0f || intent.half_extents.z <= 0.0f) {
                if (out_error) {
                    *out_error = PhysicsComponentValidationError::NonPositiveDimension;
                }
                return false;
            }
            break;
        case ColliderShapeKind::Sphere:
            if (!IsFiniteScalar(intent.radius)) {
                if (out_error) {
                    *out_error = PhysicsComponentValidationError::NonFiniteValue;
                }
                return false;
            }
            if (intent.radius <= 0.0f) {
                if (out_error) {
                    *out_error = PhysicsComponentValidationError::NonPositiveDimension;
                }
                return false;
            }
            break;
        case ColliderShapeKind::Capsule:
            if (!IsFiniteScalar(intent.radius) || !IsFiniteScalar(intent.half_height)) {
                if (out_error) {
                    *out_error = PhysicsComponentValidationError::NonFiniteValue;
                }
                return false;
            }
            if (intent.radius <= 0.0f || intent.half_height <= 0.0f) {
                if (out_error) {
                    *out_error = PhysicsComponentValidationError::NonPositiveDimension;
                }
                return false;
            }
            break;
        case ColliderShapeKind::Mesh:
            if (!IsNonEmptyPath(intent.mesh_path)) {
                if (out_error) {
                    *out_error = PhysicsComponentValidationError::EmptyMeshPath;
                }
                return false;
            }
            break;
        default:
            break;
    }

    if (out_error) {
        *out_error = PhysicsComponentValidationError::None;
    }
    return true;
}

inline bool ValidatePlayerControllerIntent(const PlayerControllerIntentComponent& intent,
                                           PhysicsComponentValidationError* out_error = nullptr) {
    if (!IsFiniteVec3(intent.half_extents)
        || !IsFiniteVec3(intent.center)
        || !IsFiniteVec3(intent.desired_velocity)) {
        if (out_error) {
            *out_error = PhysicsComponentValidationError::NonFiniteValue;
        }
        return false;
    }
    if (intent.half_extents.x <= 0.0f || intent.half_extents.y <= 0.0f || intent.half_extents.z <= 0.0f) {
        if (out_error) {
            *out_error = PhysicsComponentValidationError::NonPositiveDimension;
        }
        return false;
    }
    if (out_error) {
        *out_error = PhysicsComponentValidationError::None;
    }
    return true;
}

inline bool NearlyEqualFloat(float lhs, float rhs, float epsilon = 1e-5f) {
    return std::fabs(lhs - rhs) <= epsilon;
}

inline bool NearlyEqualVec3(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = 1e-5f) {
    return NearlyEqualFloat(lhs.x, rhs.x, epsilon)
           && NearlyEqualFloat(lhs.y, rhs.y, epsilon)
           && NearlyEqualFloat(lhs.z, rhs.z, epsilon);
}

inline bool HasRuntimeLinearForceCommand(const RigidBodyIntentComponent& intent, float epsilon = 1e-5f) {
    return intent.linear_force_enabled
           && !NearlyEqualVec3(intent.linear_force, glm::vec3(0.0f, 0.0f, 0.0f), epsilon);
}

inline bool HasRuntimeLinearImpulseCommand(const RigidBodyIntentComponent& intent, float epsilon = 1e-5f) {
    return !NearlyEqualVec3(intent.linear_impulse, glm::vec3(0.0f, 0.0f, 0.0f), epsilon);
}

inline bool HasRuntimeAngularTorqueCommand(const RigidBodyIntentComponent& intent, float epsilon = 1e-5f) {
    return intent.angular_torque_enabled
           && !NearlyEqualVec3(intent.angular_torque, glm::vec3(0.0f, 0.0f, 0.0f), epsilon);
}

inline bool HasRuntimeAngularImpulseCommand(const RigidBodyIntentComponent& intent, float epsilon = 1e-5f) {
    return !NearlyEqualVec3(intent.angular_impulse, glm::vec3(0.0f, 0.0f, 0.0f), epsilon);
}

inline bool HasRuntimeCommandClearRequest(const RigidBodyIntentComponent& intent) {
    return intent.clear_runtime_commands_requested;
}

inline void ClearRuntimeCommandIntents(RigidBodyIntentComponent& intent) {
    intent.linear_force = glm::vec3(0.0f, 0.0f, 0.0f);
    intent.linear_impulse = glm::vec3(0.0f, 0.0f, 0.0f);
    intent.angular_torque = glm::vec3(0.0f, 0.0f, 0.0f);
    intent.angular_impulse = glm::vec3(0.0f, 0.0f, 0.0f);
    intent.clear_runtime_commands_requested = false;
}

inline bool ValidateTransformOwnership(const PhysicsTransformOwnershipComponent& ownership,
                                       TransformOwnershipValidationError* out_error = nullptr) {
    switch (ownership.authority) {
        case PhysicsTransformAuthority::SceneAuthoritative:
            if (ownership.pull_physics_transform_to_scene) {
                if (out_error) {
                    *out_error = TransformOwnershipValidationError::SceneAuthorityCannotPullFromPhysics;
                }
                return false;
            }
            if (ownership.scene_transform_dirty && !ownership.push_scene_transform_to_physics) {
                if (out_error) {
                    *out_error = TransformOwnershipValidationError::SceneAuthorityDirtyRequiresPush;
                }
                return false;
            }
            break;
        case PhysicsTransformAuthority::PhysicsAuthoritative:
            if (ownership.push_scene_transform_to_physics) {
                if (out_error) {
                    *out_error = TransformOwnershipValidationError::PhysicsAuthorityCannotPushSceneTransform;
                }
                return false;
            }
            if (ownership.scene_transform_dirty) {
                if (out_error) {
                    *out_error = TransformOwnershipValidationError::PhysicsAuthorityCannotSetSceneDirty;
                }
                return false;
            }
            if (!ownership.pull_physics_transform_to_scene) {
                if (out_error) {
                    *out_error = TransformOwnershipValidationError::PhysicsAuthorityRequiresPull;
                }
                return false;
            }
            break;
        default:
            break;
    }

    if (out_error) {
        *out_error = TransformOwnershipValidationError::None;
    }
    return true;
}

inline bool ShouldPushSceneTransformToPhysics(const PhysicsTransformOwnershipComponent& ownership) {
    return ownership.authority == PhysicsTransformAuthority::SceneAuthoritative
           && ownership.scene_transform_dirty
           && ownership.push_scene_transform_to_physics;
}

inline bool ShouldPullPhysicsTransformToScene(const PhysicsTransformOwnershipComponent& ownership) {
    return ownership.authority == PhysicsTransformAuthority::PhysicsAuthoritative
           && ownership.pull_physics_transform_to_scene;
}

inline bool ColliderShapeParametersChanged(ColliderShapeKind shape,
                                           const ColliderIntentComponent& previous,
                                           const ColliderIntentComponent& desired) {
    switch (shape) {
        case ColliderShapeKind::Box:
            return !NearlyEqualVec3(previous.half_extents, desired.half_extents);
        case ColliderShapeKind::Sphere:
            return !NearlyEqualFloat(previous.radius, desired.radius);
        case ColliderShapeKind::Capsule:
            return !NearlyEqualFloat(previous.radius, desired.radius)
                   || !NearlyEqualFloat(previous.half_height, desired.half_height);
        case ColliderShapeKind::Mesh:
            return previous.mesh_path != desired.mesh_path;
        default:
            return false;
    }
}

inline ColliderReconcileAction ClassifyColliderReconcileAction(const ColliderIntentComponent& previous,
                                                               const ColliderIntentComponent& desired) {
    if (!ValidateColliderIntent(previous) || !ValidateColliderIntent(desired)) {
        return ColliderReconcileAction::RejectInvalidIntent;
    }
    if (previous.shape != desired.shape) {
        return ColliderReconcileAction::RebuildRuntimeShape;
    }
    if (ColliderShapeParametersChanged(previous.shape, previous, desired)) {
        return ColliderReconcileAction::RebuildRuntimeShape;
    }
    if (previous.enabled != desired.enabled
        || previous.is_trigger != desired.is_trigger
        || !NearlyEqualFloat(previous.friction, desired.friction)
        || !NearlyEqualFloat(previous.restitution, desired.restitution)
        || previous.mask.layer != desired.mask.layer
        || previous.mask.collides_with != desired.mask.collides_with) {
        return ColliderReconcileAction::UpdateRuntimeProperties;
    }
    return ColliderReconcileAction::NoOp;
}

inline RigidBodyKinematicReconcileAction ClassifyRigidBodyKinematicReconcileAction(
    const RigidBodyIntentComponent& previous,
    const RigidBodyIntentComponent& desired) {
    if (!ValidateRigidBodyIntent(previous) || !ValidateRigidBodyIntent(desired)) {
        return RigidBodyKinematicReconcileAction::RejectInvalidIntent;
    }
    if (previous.dynamic != desired.dynamic || !desired.dynamic) {
        return RigidBodyKinematicReconcileAction::NoOp;
    }
    if (previous.kinematic != desired.kinematic) {
        return RigidBodyKinematicReconcileAction::UpdateRuntimeKinematic;
    }
    return RigidBodyKinematicReconcileAction::NoOp;
}

inline RigidBodyAwakeReconcileAction ClassifyRigidBodyAwakeReconcileAction(
    const RigidBodyIntentComponent& previous,
    const RigidBodyIntentComponent& desired) {
    if (!ValidateRigidBodyIntent(previous) || !ValidateRigidBodyIntent(desired)) {
        return RigidBodyAwakeReconcileAction::RejectInvalidIntent;
    }
    if (previous.dynamic != desired.dynamic || !desired.dynamic) {
        return RigidBodyAwakeReconcileAction::NoOp;
    }
    if (previous.awake != desired.awake) {
        return RigidBodyAwakeReconcileAction::UpdateRuntimeAwakeState;
    }
    return RigidBodyAwakeReconcileAction::NoOp;
}

inline ControllerColliderCompatibility ClassifyControllerColliderCompatibility(
    const PlayerControllerIntentComponent& controller,
    const ColliderIntentComponent* collider,
    const RigidBodyIntentComponent* rigidbody = nullptr) {
    if (!ValidatePlayerControllerIntent(controller)) {
        return ControllerColliderCompatibility::ControllerInvalid;
    }
    if (!controller.enabled) {
        return ControllerColliderCompatibility::CompatibleControllerDisabled;
    }
    if (!collider) {
        return ControllerColliderCompatibility::ColliderMissing;
    }
    if (!ValidateColliderIntent(*collider)) {
        return ControllerColliderCompatibility::ColliderInvalid;
    }
    if (!collider->enabled) {
        return ControllerColliderCompatibility::ColliderDisabled;
    }
    if (collider->is_trigger) {
        return ControllerColliderCompatibility::ColliderIsTrigger;
    }
    if (rigidbody && !rigidbody->dynamic) {
        return ControllerColliderCompatibility::EnabledControllerRequiresDynamicRigidBody;
    }
    switch (collider->shape) {
        case ColliderShapeKind::Box:
        case ColliderShapeKind::Capsule:
            return ControllerColliderCompatibility::Compatible;
        case ColliderShapeKind::Sphere:
        case ColliderShapeKind::Mesh:
            return ControllerColliderCompatibility::UnsupportedColliderShape;
        default:
            return ControllerColliderCompatibility::UnsupportedColliderShape;
    }
}

inline bool IsControllerColliderCompatible(ControllerColliderCompatibility compatibility) {
    return compatibility == ControllerColliderCompatibility::Compatible
           || compatibility == ControllerColliderCompatibility::CompatibleControllerDisabled;
}

inline ControllerVelocityOwnership ClassifyControllerVelocityOwnership(
    const PlayerControllerIntentComponent* controller,
    ControllerColliderCompatibility compatibility) {
    if (!controller || !controller->enabled) {
        return ControllerVelocityOwnership::RigidbodyIntent;
    }
    if (compatibility != ControllerColliderCompatibility::Compatible) {
        return ControllerVelocityOwnership::RigidbodyIntent;
    }
    return ControllerVelocityOwnership::ControllerIntent;
}

inline bool IsControllerVelocityOwner(ControllerVelocityOwnership ownership) {
    return ownership == ControllerVelocityOwnership::ControllerIntent;
}

inline ControllerGeometryReconcileAction ClassifyControllerGeometryReconcileAction(
    const PlayerControllerIntentComponent& previous,
    const PlayerControllerIntentComponent& desired,
    ControllerColliderCompatibility compatibility) {
    if (!ValidatePlayerControllerIntent(previous) || !ValidatePlayerControllerIntent(desired)) {
        return ControllerGeometryReconcileAction::RejectInvalidIntent;
    }
    if (compatibility != ControllerColliderCompatibility::Compatible) {
        return ControllerGeometryReconcileAction::NoOp;
    }
    if (!NearlyEqualVec3(previous.half_extents, desired.half_extents)
        || !NearlyEqualVec3(previous.center, desired.center)) {
        return ControllerGeometryReconcileAction::RebuildRuntimeShape;
    }
    return ControllerGeometryReconcileAction::NoOp;
}

} // namespace karma::scene
