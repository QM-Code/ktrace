#include "physics/backends/physx/physics_world_physx.hpp"
#include "physics/backends/physx/player_controller_physx.hpp"
#include "physics/backends/physx/rigid_body_physx.hpp"
#include "physics/backends/physx/static_body_physx.hpp"
#include <PxPhysicsAPI.h>
#include <spdlog/spdlog.h>

namespace {
physx::PxVec3 toPx(const glm::vec3& v) { return physx::PxVec3(v.x, v.y, v.z); }

class IgnorePlayerQueryFilter final : public physx::PxQueryFilterCallback {
public:
    physx::PxQueryHitType::Enum preFilter(const physx::PxFilterData&,
                                          const physx::PxShape* shape,
                                          const physx::PxRigidActor*,
                                          physx::PxHitFlags&) override {
        if (!shape) {
            return physx::PxQueryHitType::eNONE;
        }
        const physx::PxFilterData data = shape->getQueryFilterData();
        if ((data.word0 & physics_backend::kPhysXQueryIgnorePlayer) != 0) {
            return physx::PxQueryHitType::eNONE;
        }
        return physx::PxQueryHitType::eBLOCK;
    }

    physx::PxQueryHitType::Enum postFilter(const physx::PxFilterData&,
                                           const physx::PxQueryHit&,
                                           const physx::PxShape*,
                                           const physx::PxRigidActor*) override {
        return physx::PxQueryHitType::eBLOCK;
    }
};
}

namespace physics_backend {

PhysicsWorldPhysX::PhysicsWorldPhysX() {
    foundation_ = PxCreateFoundation(PX_PHYSICS_VERSION, allocator_, errorCallback_);
    if (!foundation_) {
        spdlog::error("PhysX: failed to create foundation");
        return;
    }

    physics_ = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation_, physx::PxTolerancesScale(), true);
    if (!physics_) {
        spdlog::error("PhysX: failed to create physics");
        return;
    }

    physx::PxSceneDesc sceneDesc(physics_->getTolerancesScale());
    sceneDesc.gravity = physx::PxVec3(0.0f, -9.8f, 0.0f);
    dispatcher_ = physx::PxDefaultCpuDispatcherCreate(2);
    sceneDesc.cpuDispatcher = dispatcher_;
    sceneDesc.filterShader = physx::PxDefaultSimulationFilterShader;
    scene_ = physics_->createScene(sceneDesc);

    defaultMaterial_ = physics_->createMaterial(0.5f, 0.5f, 0.0f);
    controllerManager_ = PxCreateControllerManager(*scene_);
}

PhysicsWorldPhysX::~PhysicsWorldPhysX() {
    if (defaultMaterial_) {
        defaultMaterial_->release();
        defaultMaterial_ = nullptr;
    }
    if (scene_) {
        scene_->release();
        scene_ = nullptr;
    }
    if (controllerManager_) {
        controllerManager_->release();
        controllerManager_ = nullptr;
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
}

void PhysicsWorldPhysX::update(float deltaTime) {
    if (!scene_) return;
    scene_->simulate(deltaTime);
    scene_->fetchResults(true);
}

void PhysicsWorldPhysX::setGravity(float gravity) {
    if (!scene_) return;
    scene_->setGravity(physx::PxVec3(0.0f, gravity, 0.0f));
}

std::unique_ptr<PhysicsRigidBodyBackend> PhysicsWorldPhysX::createBoxBody(const glm::vec3& halfExtents,
                                                                          float mass,
                                                                          const glm::vec3& position,
                                                                          const karma::physics::PhysicsMaterial& material) {
    if (!physics_ || !scene_) {
        return std::make_unique<PhysicsRigidBodyPhysX>();
    }

    physx::PxMaterial* mat = defaultMaterial_;
    if (mat) {
        mat->setStaticFriction(material.friction);
        mat->setDynamicFriction(material.friction);
        mat->setRestitution(material.restitution);
    }
    physx::PxShape* shape = physics_->createShape(physx::PxBoxGeometry(halfExtents.x, halfExtents.y, halfExtents.z), *mat);
    if (!shape) {
        return std::make_unique<PhysicsRigidBodyPhysX>();
    }

    physx::PxTransform transform(toPx(position));
    physx::PxRigidActor* actor = nullptr;
    if (mass > 0.0f) {
        auto* body = physics_->createRigidDynamic(transform);
        if (body) {
            body->attachShape(*shape);
            physx::PxRigidBodyExt::updateMassAndInertia(*body, mass);
            actor = body;
        }
    } else {
        auto* body = physics_->createRigidStatic(transform);
        if (body) {
            body->attachShape(*shape);
            actor = body;
        }
    }
    shape->release();
    if (!actor) {
        return std::make_unique<PhysicsRigidBodyPhysX>();
    }
    scene_->addActor(*actor);

    return std::make_unique<PhysicsRigidBodyPhysX>(this, actor);
}

std::unique_ptr<PhysicsPlayerControllerBackend> PhysicsWorldPhysX::createPlayer(const glm::vec3& size) {
    return std::make_unique<PhysicsPlayerControllerPhysX>(this, size, glm::vec3(0.0f, 2.0f, 0.0f));
}

std::unique_ptr<PhysicsStaticBodyBackend> PhysicsWorldPhysX::createStaticMesh(const std::string& meshPath) {
    return PhysicsStaticBodyPhysX::fromMesh(this, meshPath);
}

bool PhysicsWorldPhysX::raycast(const glm::vec3& from,
                                const glm::vec3& to,
                                glm::vec3& hitPoint,
                                glm::vec3& hitNormal) const {
    if (!scene_) return false;

    const physx::PxVec3 origin = toPx(from);
    const physx::PxVec3 direction = toPx(to - from);
    const float distance = direction.magnitude();
    if (distance <= 1e-6f) return false;

    physx::PxRaycastBuffer hit;
    physx::PxQueryFilterData filterData;
    filterData.flags = physx::PxQueryFlag::eSTATIC | physx::PxQueryFlag::eDYNAMIC | physx::PxQueryFlag::ePREFILTER;
    IgnorePlayerQueryFilter filterCallback;
    if (!scene_->raycast(origin, direction.getNormalized(), distance, hit, physx::PxHitFlag::eDEFAULT, filterData, &filterCallback)) {
        return false;
    }
    hitPoint = glm::vec3(hit.block.position.x, hit.block.position.y, hit.block.position.z);
    hitNormal = glm::vec3(hit.block.normal.x, hit.block.normal.y, hit.block.normal.z);
    return true;
}

} // namespace physics_backend
