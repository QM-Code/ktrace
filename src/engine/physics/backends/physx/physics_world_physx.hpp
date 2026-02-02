#pragma once

#include "physics/backend.hpp"
#include <PxPhysicsAPI.h>
#include <memory>

namespace physics_backend {

inline constexpr physx::PxU32 kPhysXQueryIgnorePlayer = 1u << 0;

class PhysicsWorldPhysX final : public PhysicsWorldBackend {
public:
    PhysicsWorldPhysX();
    ~PhysicsWorldPhysX() override;

    void update(float deltaTime) override;
    void setGravity(float gravity) override;
    std::unique_ptr<PhysicsRigidBodyBackend> createBoxBody(const glm::vec3& halfExtents,
                                                           float mass,
                                                           const glm::vec3& position,
                                                           const karma::physics::PhysicsMaterial& material) override;
    std::unique_ptr<PhysicsPlayerControllerBackend> createPlayer(const glm::vec3& size) override;
    std::unique_ptr<PhysicsStaticBodyBackend> createStaticMesh(const std::string& meshPath) override;
    bool raycast(const glm::vec3& from, const glm::vec3& to, glm::vec3& hitPoint, glm::vec3& hitNormal) const override;

    physx::PxPhysics* physics() const { return physics_; }
    physx::PxScene* scene() const { return scene_; }
    physx::PxMaterial* defaultMaterial() const { return defaultMaterial_; }
    physx::PxControllerManager* controllerManager() const { return controllerManager_; }

private:
    physx::PxDefaultAllocator allocator_;
    physx::PxDefaultErrorCallback errorCallback_;
    physx::PxFoundation* foundation_ = nullptr;
    physx::PxPhysics* physics_ = nullptr;
    physx::PxDefaultCpuDispatcher* dispatcher_ = nullptr;
    physx::PxScene* scene_ = nullptr;
    physx::PxMaterial* defaultMaterial_ = nullptr;
    physx::PxControllerManager* controllerManager_ = nullptr;
};

} // namespace physics_backend
