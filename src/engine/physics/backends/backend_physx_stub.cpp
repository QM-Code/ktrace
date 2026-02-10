#include "physics/backends/backend_factory_internal.hpp"

#include "karma/common/logging.hpp"

#include <string>
#include <unordered_map>

#if defined(KARMA_HAS_PHYSICS_PHYSX)
#include <PxPhysicsAPI.h>

#include <algorithm>
#include <cmath>
#include <thread>
#endif

namespace karma::physics_backend {
namespace {

#if defined(KARMA_HAS_PHYSICS_PHYSX)

physx::PxVec3 ToPxVec3(const glm::vec3& value) {
    return physx::PxVec3(value.x, value.y, value.z);
}

physx::PxQuat ToPxQuat(const glm::quat& value) {
    return physx::PxQuat(value.x, value.y, value.z, value.w);
}

glm::vec3 ToGlmVec3(const physx::PxVec3& value) {
    return glm::vec3(value.x, value.y, value.z);
}

glm::quat ToGlmQuat(const physx::PxQuat& value) {
    return glm::quat(value.w, value.x, value.y, value.z);
}

class PhysXBackend final : public Backend {
 public:
    const char* name() const override {
        return "physx";
    }

    bool init() override {
        if (initialized_) {
            return true;
        }

        foundation_ = PxCreateFoundation(PX_PHYSICS_VERSION, allocator_, error_callback_);
        if (!foundation_) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create foundation");
            shutdown();
            return false;
        }

        physics_ = PxCreatePhysics(PX_PHYSICS_VERSION, *foundation_, physx::PxTolerancesScale(), true);
        if (!physics_) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create physics");
            shutdown();
            return false;
        }

        physx::PxSceneDesc scene_desc(physics_->getTolerancesScale());
        scene_desc.gravity = physx::PxVec3(0.0f, -9.8f, 0.0f);
        const uint32_t worker_count = std::max(1u, std::thread::hardware_concurrency());
        dispatcher_ = physx::PxDefaultCpuDispatcherCreate(static_cast<physx::PxU32>(worker_count));
        if (!dispatcher_) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create dispatcher");
            shutdown();
            return false;
        }
        scene_desc.cpuDispatcher = dispatcher_;
        scene_desc.filterShader = physx::PxDefaultSimulationFilterShader;
        scene_ = physics_->createScene(scene_desc);
        if (!scene_) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create scene");
            shutdown();
            return false;
        }

        default_material_ = physics_->createMaterial(0.5f, 0.5f, 0.0f);
        if (!default_material_) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create default material");
            shutdown();
            return false;
        }

        initialized_ = true;
        KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: initialized");
        return true;
    }

    void shutdown() override {
        for (const auto& [id, actor] : bodies_) {
            (void)id;
            if (scene_ && actor) {
                scene_->removeActor(*actor);
            }
            if (actor) {
                actor->release();
            }
        }
        bodies_.clear();

        if (default_material_) {
            default_material_->release();
            default_material_ = nullptr;
        }
        if (scene_) {
            scene_->release();
            scene_ = nullptr;
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

        initialized_ = false;
        KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: shutdown");
    }

    void beginFrame(float dt) override {
        frame_dt_ = dt;
    }

    void simulateFixedStep(float fixed_dt) override {
        if (!scene_ || fixed_dt <= 0.0f) {
            return;
        }

        scene_->simulate(fixed_dt);
        scene_->fetchResults(true);
    }

    void endFrame() override {
        (void)frame_dt_;
        KARMA_TRACE_CHANGED("physics.physx",
                            std::to_string(bodies_.size()),
                            "PhysicsBackend[physx]: active bodies={}",
                            bodies_.size());
    }

    BodyId createBody(const BodyDesc& desc) override {
        if (!physics_ || !scene_ || !default_material_) {
            return kInvalidBodyId;
        }

        const physx::PxTransform transform(ToPxVec3(desc.transform.position), ToPxQuat(desc.transform.rotation));
        physx::PxShape* shape = physics_->createShape(physx::PxBoxGeometry(0.5f, 0.5f, 0.5f), *default_material_);
        if (!shape) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create shape");
            return kInvalidBodyId;
        }

        physx::PxRigidActor* actor = nullptr;
        if (desc.is_static) {
            auto* static_actor = physics_->createRigidStatic(transform);
            if (static_actor) {
                static_actor->attachShape(*shape);
                actor = static_actor;
            }
        } else {
            auto* dynamic_actor = physics_->createRigidDynamic(transform);
            if (dynamic_actor) {
                dynamic_actor->attachShape(*shape);
                physx::PxRigidBodyExt::updateMassAndInertia(*dynamic_actor, std::max(desc.mass, 0.001f));
                dynamic_actor->setLinearVelocity(ToPxVec3(desc.linear_velocity));
                dynamic_actor->setAngularVelocity(ToPxVec3(desc.angular_velocity));
                actor = dynamic_actor;
            }
        }

        shape->release();
        if (!actor) {
            KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: failed to create actor");
            return kInvalidBodyId;
        }

        scene_->addActor(*actor);
        const BodyId id = next_body_id_++;
        bodies_.emplace(id, actor);
        return id;
    }

    void destroyBody(BodyId body) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return;
        }

        physx::PxRigidActor* actor = it->second;
        if (scene_ && actor) {
            scene_->removeActor(*actor);
        }
        if (actor) {
            actor->release();
        }
        bodies_.erase(it);
    }

    bool setBodyTransform(BodyId body, const BodyTransform& transform) override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second) {
            return false;
        }

        it->second->setGlobalPose(physx::PxTransform(ToPxVec3(transform.position), ToPxQuat(transform.rotation)), true);
        if (auto* dynamic_actor = it->second->is<physx::PxRigidDynamic>()) {
            dynamic_actor->wakeUp();
        }
        return true;
    }

    bool getBodyTransform(BodyId body, BodyTransform& out_transform) const override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end() || !it->second) {
            return false;
        }

        const physx::PxTransform transform = it->second->getGlobalPose();
        out_transform.position = ToGlmVec3(transform.p);
        out_transform.rotation = ToGlmQuat(transform.q);
        return true;
    }

    bool raycastClosest(const glm::vec3& origin,
                        const glm::vec3& direction,
                        float max_distance,
                        RaycastHit& out_hit) const override {
        if (!scene_ || max_distance <= 0.0f) {
            return false;
        }

        const float direction_length = glm::length(direction);
        if (!std::isfinite(direction_length) || direction_length <= 1e-6f) {
            return false;
        }

        const glm::vec3 unit_direction = direction / direction_length;
        physx::PxRaycastBuffer hit_buffer;
        const bool has_hit = scene_->raycast(ToPxVec3(origin), ToPxVec3(unit_direction), max_distance, hit_buffer);
        if (!has_hit || !hit_buffer.hasBlock) {
            return false;
        }

        const BodyId hit_body = findBodyId(hit_buffer.block.actor);
        if (hit_body == kInvalidBodyId) {
            return false;
        }

        out_hit.body = hit_body;
        out_hit.position = ToGlmVec3(hit_buffer.block.position);
        out_hit.distance = hit_buffer.block.distance;
        out_hit.fraction = hit_buffer.block.distance / max_distance;
        return true;
    }

 private:
    BodyId findBodyId(const physx::PxRigidActor* actor) const {
        if (!actor) {
            return kInvalidBodyId;
        }

        for (const auto& [id, candidate] : bodies_) {
            if (candidate == actor) {
                return id;
            }
        }
        return kInvalidBodyId;
    }

    physx::PxDefaultAllocator allocator_{};
    physx::PxDefaultErrorCallback error_callback_{};
    physx::PxFoundation* foundation_ = nullptr;
    physx::PxPhysics* physics_ = nullptr;
    physx::PxDefaultCpuDispatcher* dispatcher_ = nullptr;
    physx::PxScene* scene_ = nullptr;
    physx::PxMaterial* default_material_ = nullptr;
    BodyId next_body_id_ = 1;
    std::unordered_map<BodyId, physx::PxRigidActor*> bodies_{};
    float frame_dt_ = 0.0f;
    bool initialized_ = false;
};

#else

class PhysXBackendStub final : public Backend {
 public:
    const char* name() const override { return "physx"; }

    bool init() override {
        KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: unavailable (not compiled)");
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
    bool raycastClosest(const glm::vec3&, const glm::vec3&, float, RaycastHit&) const override { return false; }
};

#endif

} // namespace

std::unique_ptr<Backend> CreatePhysXBackend() {
#if defined(KARMA_HAS_PHYSICS_PHYSX)
    return std::make_unique<PhysXBackend>();
#else
    return std::make_unique<PhysXBackendStub>();
#endif
}

} // namespace karma::physics_backend
