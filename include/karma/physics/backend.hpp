#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace karma::physics_backend {

enum class BackendKind {
    Auto,
    Jolt,
    PhysX
};

const char* BackendKindName(BackendKind kind);
std::optional<BackendKind> ParseBackendKind(std::string_view name);
std::vector<BackendKind> CompiledBackends();

using BodyId = uint64_t;
inline constexpr BodyId kInvalidBodyId = 0;

struct BodyTransform {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::quat rotation{1.0f, 0.0f, 0.0f, 0.0f};
};

struct BodyDesc {
    BodyTransform transform{};
    glm::vec3 linear_velocity{0.0f, 0.0f, 0.0f};
    glm::vec3 angular_velocity{0.0f, 0.0f, 0.0f};
    float mass = 0.0f;
    bool is_static = true;
};

class Backend {
 public:
    virtual ~Backend() = default;

    virtual const char* name() const = 0;
    virtual bool init() = 0;
    virtual void shutdown() = 0;
    virtual void beginFrame(float dt) = 0;
    virtual void simulateFixedStep(float fixed_dt) = 0;
    virtual void endFrame() = 0;

    virtual BodyId createBody(const BodyDesc& desc) = 0;
    virtual void destroyBody(BodyId body) = 0;
    virtual bool setBodyTransform(BodyId body, const BodyTransform& transform) = 0;
    virtual bool getBodyTransform(BodyId body, BodyTransform& out_transform) const = 0;
};

std::unique_ptr<Backend> CreateBackend(BackendKind preferred = BackendKind::Auto,
                                       BackendKind* out_selected = nullptr);

} // namespace karma::physics_backend

