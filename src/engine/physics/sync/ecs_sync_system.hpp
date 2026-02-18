#pragma once

#include "karma/ecs/entity.hpp"
#include "karma/physics/backend.hpp"
#include "karma/scene/physics_components.hpp"

#include <cstddef>
#include <unordered_map>

namespace karma::ecs {
class World;
}

namespace karma::physics {

class PhysicsSystem;

class EcsSyncSystem {
 public:
    explicit EcsSyncSystem(PhysicsSystem& physics_system);

    void preSimulate(ecs::World& world);
    void postSimulate(ecs::World& world);
    void clear();

    bool hasRuntimeBinding(ecs::Entity entity) const;
    size_t runtimeBindingCount() const;
    bool tryGetRuntimeBody(ecs::Entity entity, physics::backend::BodyId& out_body) const;
    bool tryGetRuntimeTransformSnapshot(ecs::Entity entity, physics::backend::BodyTransform& out_transform) const;
    bool hasControllerRuntimeBinding(ecs::Entity entity) const;
    size_t controllerRuntimeBindingCount() const;
    bool tryGetControllerCompatibility(ecs::Entity entity,
                                       scene::ControllerColliderCompatibility& out_compatibility) const;

 private:
    struct EntityHash {
        size_t operator()(ecs::Entity entity) const noexcept;
    };

    struct RuntimeBinding {
        physics::backend::BodyId body = physics::backend::kInvalidBodyId;
        scene::RigidBodyIntentComponent rigidbody_intent{};
        scene::ColliderIntentComponent collider_intent{};
        scene::PhysicsTransformOwnershipComponent transform_ownership{};
        bool controller_runtime_bound = false;
        scene::PlayerControllerIntentComponent controller_intent{};
        scene::ControllerColliderCompatibility controller_compatibility =
            scene::ControllerColliderCompatibility::Compatible;
        scene::ControllerVelocityOwnership velocity_ownership = scene::ControllerVelocityOwnership::RigidbodyIntent;
        glm::vec3 runtime_linear_velocity{0.0f, 0.0f, 0.0f};
        glm::vec3 runtime_angular_velocity{0.0f, 0.0f, 0.0f};
        physics::backend::BodyTransform last_transform{};
    };

    PhysicsSystem& physics_system_;
    std::unordered_map<ecs::Entity, RuntimeBinding, EntityHash> bindings_{};
    std::unordered_map<ecs::Entity, bool, EntityHash> static_mesh_recovery_pending_{};
};

} // namespace karma::physics
