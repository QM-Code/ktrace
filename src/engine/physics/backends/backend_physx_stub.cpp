#include "physics/backends/backend_factory_internal.hpp"

#include "karma/common/logging.hpp"

#include <string>
#include <unordered_map>

namespace karma::physics_backend {
namespace {

class PhysXBackendStub final : public Backend {
 public:
    const char* name() const override { return "physx"; }

    bool init() override {
        KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: initialized stub backend");
        return true;
    }

    void shutdown() override {
        bodies_.clear();
        KARMA_TRACE("physics.physx", "PhysicsBackend[physx]: shutdown");
    }

    void beginFrame(float dt) override {
        (void)dt;
    }

    void simulateFixedStep(float fixed_dt) override {
        for (auto& [id, body] : bodies_) {
            (void)id;
            if (body.is_static) {
                continue;
            }
            body.transform.position += body.linear_velocity * fixed_dt;
        }
    }

    void endFrame() override {
        KARMA_TRACE_CHANGED("physics.physx",
                            std::to_string(bodies_.size()),
                            "PhysicsBackend[physx]: active bodies={}",
                            bodies_.size());
    }

    BodyId createBody(const BodyDesc& desc) override {
        const BodyId id = next_body_id_++;
        bodies_[id] = BodyState{desc.transform, desc.linear_velocity, desc.angular_velocity, desc.mass, desc.is_static};
        return id;
    }

    void destroyBody(BodyId body) override {
        bodies_.erase(body);
    }

    bool setBodyTransform(BodyId body, const BodyTransform& transform) override {
        auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }
        it->second.transform = transform;
        return true;
    }

    bool getBodyTransform(BodyId body, BodyTransform& out_transform) const override {
        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }
        out_transform = it->second.transform;
        return true;
    }

 private:
    struct BodyState {
        BodyTransform transform{};
        glm::vec3 linear_velocity{0.0f, 0.0f, 0.0f};
        glm::vec3 angular_velocity{0.0f, 0.0f, 0.0f};
        float mass = 0.0f;
        bool is_static = true;
    };

    BodyId next_body_id_ = 1;
    std::unordered_map<BodyId, BodyState> bodies_{};
};

} // namespace

std::unique_ptr<Backend> CreatePhysXBackend() {
    return std::make_unique<PhysXBackendStub>();
}

} // namespace karma::physics_backend
