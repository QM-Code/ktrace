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

struct RaycastHit {
    BodyId body = kInvalidBodyId;
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    float distance = 0.0f;
    float fraction = 0.0f;
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
    // Closest-hit ray query contract:
    // - Direction is interpreted as a ray direction (it is normalized internally by current backends).
    // - Returns false when no hit is found or when arguments are invalid (e.g. zero direction or max_distance <= 0).
    // - Start-inside-shape semantics are not yet standardized across backends (TODO: define explicitly).
    // - No filtering/layer mask semantics are exposed yet (TODO: add query filter contract before gameplay depends on it).
    // - Current backend implementations map native hit handles to BodyId with a linear scan
    //   (TODO: optimize to indexed mapping if query volume grows).
    virtual bool raycastClosest(const glm::vec3& origin,
                                const glm::vec3& direction,
                                float max_distance,
                                RaycastHit& out_hit) const = 0;
};

std::unique_ptr<Backend> CreateBackend(BackendKind preferred = BackendKind::Auto,
                                       BackendKind* out_selected = nullptr);

} // namespace karma::physics_backend
