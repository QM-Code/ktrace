#pragma once

#include "karma/physics/physics_world.hpp"
#include "karma/physics/rigid_body.hpp"
#include "karma/physics/static_body.hpp"
#include "karma/components/collider.h"
#include "karma/components/rigidbody.h"
#include "karma/components/transform.h"
#include "karma/components/mesh.h"
#include "karma/ecs/world.h"
#include <unordered_map>

namespace karma::ecs {
namespace components = karma::components;

class PhysicsSyncSystem {
public:
    void update(karma::ecs::World &world, PhysicsWorld *physics) {
        if (!physics) {
            return;
        }

        // Cleanup stale entries.
        for (auto it = rigidBodies_.begin(); it != rigidBodies_.end(); ) {
            const karma::ecs::Entity entity = it->first;
            if (!world.has<components::ColliderComponent>(entity) ||
                !world.has<components::RigidbodyComponent>(entity) ||
                !world.has<components::TransformComponent>(entity)) {
                it->second.destroy();
                it = rigidBodies_.erase(it);
            } else {
                ++it;
            }
        }
        for (auto it = staticBodies_.begin(); it != staticBodies_.end(); ) {
            const karma::ecs::Entity entity = it->first;
            if (!world.has<components::ColliderComponent>(entity) ||
                !world.has<components::TransformComponent>(entity)) {
                it->second.destroy();
                it = staticBodies_.erase(it);
            } else {
                ++it;
            }
        }

        auto &transforms = world.storage<components::TransformComponent>();
        auto &colliders = world.storage<components::ColliderComponent>();
        auto &rigidbodies = world.storage<components::RigidbodyComponent>();

        for (const karma::ecs::Entity entity : colliders.denseEntities()) {
            const components::ColliderComponent &collider = colliders.get(entity);
            if (!transforms.has(entity)) {
                continue;
            }
            const components::TransformComponent &transform = transforms.get(entity);

            const bool hasRigidBody = rigidbodies.has(entity);
            if (hasRigidBody) {
                if (rigidBodies_.find(entity) == rigidBodies_.end()) {
                    if (collider.shape == components::ColliderComponent::Shape::Box) {
                        const components::RigidbodyComponent &rb = rigidbodies.get(entity);
                        PhysicsRigidBody body = physics->createBoxBody(
                            collider.half_extents,
                            rb.mass,
                            transform.position,
                            collider.material);
                        rigidBodies_.insert({entity, std::move(body)});
                    }
                }
            } else if (collider.shape == components::ColliderComponent::Shape::Mesh) {
                if (staticBodies_.find(entity) == staticBodies_.end() && !collider.mesh_key.empty()) {
                    PhysicsStaticBody body = physics->createStaticMesh(collider.mesh_key);
                    staticBodies_.insert({entity, std::move(body)});
                }
            }
        }

        for (auto &pair : rigidBodies_) {
            const karma::ecs::Entity entity = pair.first;
            if (transforms.has(entity) && rigidbodies.has(entity)) {
                auto &transform = transforms.get(entity);
                const components::RigidbodyComponent &rb = rigidbodies.get(entity);
                    if (rb.is_kinematic) {
                        pair.second.setPosition(transform.position);
                        pair.second.setRotation(transform.rotation);
                    } else {
                        transform.position = pair.second.getPosition();
                        transform.rotation = pair.second.getRotation();
                    }
            }
        }
    }

private:
    std::unordered_map<karma::ecs::Entity, PhysicsRigidBody> rigidBodies_;
    std::unordered_map<karma::ecs::Entity, PhysicsStaticBody> staticBodies_;
};

} // namespace karma::ecs
