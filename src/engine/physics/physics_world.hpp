#pragma once

#include "karma/physics/types.h"
#include "physics/rigid_body.hpp"
#include "physics/static_body.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <memory>
#include <string>

class PhysicsPlayerController;

namespace physics_backend {
class PhysicsWorldBackend;
}

class PhysicsWorld {
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld&) = delete;
    PhysicsWorld& operator=(const PhysicsWorld&) = delete;

    void update(float deltaTime);

    void setGravity(float gravity);

    PhysicsRigidBody createBoxBody(const glm::vec3& halfExtents,
                                   float mass,
                                   const glm::vec3& position,
                                   const karma::physics::PhysicsMaterial& material);

    PhysicsPlayerController& createPlayer();
    PhysicsPlayerController& createPlayer(const glm::vec3& size);

    PhysicsPlayerController* playerController() { return playerController_.get(); }

    PhysicsStaticBody createStaticMesh(const std::string& meshPath);

    bool raycast(const glm::vec3& from, const glm::vec3& to, glm::vec3& hitPoint, glm::vec3& hitNormal) const;

private:
    std::unique_ptr<physics_backend::PhysicsWorldBackend> backend_;
    std::unique_ptr<PhysicsPlayerController> playerController_;
};
