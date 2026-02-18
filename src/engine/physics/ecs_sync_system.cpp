#include "physics/ecs_sync_system.hpp"

#include "karma/ecs/world.hpp"
#include "karma/physics/physics_system.hpp"
#include "karma/scene/components.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <unordered_set>
#include <vector>

#include <glm/gtc/quaternion.hpp>

namespace karma::physics {
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
    if (previous.rotation_locked != desired.rotation_locked) {
        return true;
    }
    if (previous.translation_locked != desired.translation_locked) {
        return true;
    }
    return false;
}

void BuildBodyDesc(const scene::RigidBodyIntentComponent& rigidbody,
                   const scene::ColliderIntentComponent& collider,
                   const physics_backend::BodyTransform& transform,
                   physics_backend::BodyDesc& out_desc) {
    out_desc = physics_backend::BodyDesc{};
    out_desc.transform = transform;
    out_desc.linear_velocity = rigidbody.linear_velocity;
    out_desc.angular_velocity = rigidbody.angular_velocity;
    out_desc.mass = rigidbody.dynamic ? rigidbody.mass : 0.0f;
    out_desc.is_static = !rigidbody.dynamic;
    out_desc.gravity_enabled = rigidbody.dynamic && rigidbody.gravity_enabled;
    out_desc.rotation_locked = rigidbody.rotation_locked;
    out_desc.translation_locked = rigidbody.translation_locked;
    out_desc.is_trigger = collider.is_trigger;
    out_desc.collision_mask.layer = collider.mask.layer;
    out_desc.collision_mask.collides_with = collider.mask.collides_with;

    switch (collider.shape) {
        case scene::ColliderShapeKind::Box:
            out_desc.collider_shape.kind = physics_backend::ColliderShapeKind::Box;
            out_desc.collider_shape.box_half_extents = collider.half_extents;
            break;
        case scene::ColliderShapeKind::Sphere:
            out_desc.collider_shape.kind = physics_backend::ColliderShapeKind::Sphere;
            out_desc.collider_shape.sphere_radius = collider.radius;
            break;
        case scene::ColliderShapeKind::Capsule:
            out_desc.collider_shape.kind = physics_backend::ColliderShapeKind::Capsule;
            out_desc.collider_shape.capsule_radius = collider.radius;
            out_desc.collider_shape.capsule_half_height = collider.half_height;
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
        if (controller) {
            controller_compatibility = scene::ClassifyControllerColliderCompatibility(*controller, collider);
            if (!scene::IsControllerColliderCompatible(controller_compatibility)) {
                teardown_entities.push_back(entity);
                continue;
            }
        }

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
                                     &scene_transform,
                                     &world]() -> bool {
            physics_backend::BodyDesc desc{};
            BuildBodyDesc(*rigidbody, *collider, scene_transform, desc);

            const physics_backend::BodyId body = physics_system_.createBody(desc);
            if (body == physics_backend::kInvalidBodyId) {
                return false;
            }

            RuntimeBinding binding{};
            binding.body = body;
            binding.rigidbody_intent = *rigidbody;
            binding.collider_intent = *collider;
            binding.transform_ownership = *ownership;
            binding.controller_runtime_bound = controller != nullptr;
            if (controller) {
                binding.controller_intent = *controller;
                binding.controller_compatibility = controller_compatibility;
            }
            binding.last_transform = scene_transform;
            bindings_[entity] = binding;

            if (scene::ShouldPushSceneTransformToPhysics(*ownership)) {
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

        if (collider_action == scene::ColliderReconcileAction::RejectInvalidIntent) {
            teardown_entities.push_back(entity);
            continue;
        }

        if (collider_action == scene::ColliderReconcileAction::UpdateRuntimeProperties && !collider->enabled) {
            // Deterministic UpdateRuntimeProperties handling: disable means teardown/no-runtime.
            teardown_entities.push_back(entity);
            continue;
        }

        if (rigidbody_rebuild || collider_rebuild) {
            physics_system_.destroyBody(binding.body);
            bindings_.erase(binding_it);
            (void)create_binding();
            continue;
        }

        if (collider_action == scene::ColliderReconcileAction::UpdateRuntimeProperties) {
            const bool trigger_update_ok = physics_system_.setBodyTrigger(binding.body, collider->is_trigger);
            physics_backend::CollisionMask collision_mask{};
            collision_mask.layer = collider->mask.layer;
            collision_mask.collides_with = collider->mask.collides_with;
            const bool filter_update_ok = physics_system_.setBodyCollisionMask(binding.body, collision_mask);
            if (!trigger_update_ok || !filter_update_ok) {
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

        binding.rigidbody_intent = *rigidbody;
        binding.collider_intent = *collider;
        if (controller) {
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
