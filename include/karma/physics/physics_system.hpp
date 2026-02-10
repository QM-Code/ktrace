#pragma once

#include "karma/physics/backend.hpp"

#include <memory>
#include <string>

namespace karma::physics {

class PhysicsSystem {
 public:
    void setBackend(physics_backend::BackendKind backend) { requested_backend_ = backend; }
    physics_backend::BackendKind requestedBackend() const { return requested_backend_; }
    physics_backend::BackendKind selectedBackend() const { return selected_backend_; }
    const char* selectedBackendName() const;
    bool isInitialized() const { return initialized_; }

    void init();
    void shutdown();
    void beginFrame(float dt);
    void simulateFixedStep(float fixed_dt);
    void endFrame();

    physics_backend::BodyId createBody(const physics_backend::BodyDesc& desc);
    void destroyBody(physics_backend::BodyId body);
    bool setBodyTransform(physics_backend::BodyId body, const physics_backend::BodyTransform& transform);
    bool getBodyTransform(physics_backend::BodyId body, physics_backend::BodyTransform& out_transform) const;
    bool raycastClosest(const glm::vec3& origin,
                        const glm::vec3& direction,
                        float max_distance,
                        physics_backend::RaycastHit& out_hit) const;

 private:
    physics_backend::BackendKind requested_backend_ = physics_backend::BackendKind::Auto;
    physics_backend::BackendKind selected_backend_ = physics_backend::BackendKind::Auto;
    std::unique_ptr<physics_backend::Backend> backend_{};
    bool initialized_ = false;
};

} // namespace karma::physics
