#include "physics/physics_world.hpp"
#include "physics/backend.hpp"
#include "physics/player_controller.hpp"

PhysicsWorld::PhysicsWorld()
    : backend_(physics_backend::CreatePhysicsWorldBackend()) {}

PhysicsWorld::~PhysicsWorld() = default;

void PhysicsWorld::update(float deltaTime) {
    if (playerController_) {
        playerController_->update(deltaTime);
    }
    if (backend_) {
        backend_->update(deltaTime);
    }
}

void PhysicsWorld::setGravity(float gravity) {
    if (backend_) {
        backend_->setGravity(gravity);
    }
}

PhysicsRigidBody PhysicsWorld::createBoxBody(const glm::vec3& halfExtents,
                                             float mass,
                                             const glm::vec3& position,
                                             const karma::physics::PhysicsMaterial& material) {
    if (!backend_) {
        return PhysicsRigidBody();
    }
    return PhysicsRigidBody(backend_->createBoxBody(halfExtents, mass, position, material));
}

PhysicsPlayerController& PhysicsWorld::createPlayer(const glm::vec3& size) {
    if (!backend_) {
        playerController_ = std::make_unique<PhysicsPlayerController>();
        return *playerController_;
    }
    playerController_ = std::make_unique<PhysicsPlayerController>(backend_->createPlayer(size));
    return *playerController_;
}

PhysicsPlayerController& PhysicsWorld::createPlayer() {
    return createPlayer(glm::vec3(1.0f, 2.0f, 1.0f));
}

PhysicsStaticBody PhysicsWorld::createStaticMesh(const std::string& meshPath) {
    if (!backend_) {
        return PhysicsStaticBody();
    }
    return PhysicsStaticBody(backend_->createStaticMesh(meshPath));
}

bool PhysicsWorld::raycast(const glm::vec3& from,
                           const glm::vec3& to,
                           glm::vec3& hitPoint,
                           glm::vec3& hitNormal) const {
    if (!backend_) {
        return false;
    }
    return backend_->raycast(from, to, hitPoint, hitNormal);
}
