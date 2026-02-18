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

enum class ColliderShapeKind {
    Box,
    Sphere,
    Capsule
};

struct CollisionMask {
    uint32_t layer = 0x1u;
    uint32_t collides_with = 0xFFFFFFFFu;
};

struct ColliderShapeDesc {
    ColliderShapeKind kind = ColliderShapeKind::Box;
    glm::vec3 box_half_extents{0.5f, 0.5f, 0.5f};
    float sphere_radius = 0.5f;
    float capsule_radius = 0.5f;
    float capsule_half_height = 0.5f;
};

struct BodyDesc {
    BodyTransform transform{};
    ColliderShapeDesc collider_shape{};
    glm::vec3 linear_velocity{0.0f, 0.0f, 0.0f};
    glm::vec3 angular_velocity{0.0f, 0.0f, 0.0f};
    float mass = 0.0f;
    bool is_static = true;
    bool is_trigger = false;
    bool gravity_enabled = true;
    bool rotation_locked = false;
    bool translation_locked = false;
    CollisionMask collision_mask{};
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
    // Body flag/constraint contract:
    // - Gravity enablement currently applies only to dynamic bodies.
    // - For invalid, unknown, or non-dynamic bodies, calls return false.
    // - Rotation/translation locks are currently creation-time only via BodyDesc.
    virtual bool setBodyGravityEnabled(BodyId body, bool enabled) = 0;
    virtual bool getBodyGravityEnabled(BodyId body, bool& out_enabled) const = 0;
    // Collider runtime property contract:
    // - Returns false for invalid/unknown body ids.
    // - Backends may report false when a runtime mutation is unsupported; this is used by callers to apply deterministic
    //   fallback behavior (for example runtime rebuild) instead of silent no-op.
    // - get* methods return the currently active property values that the backend can coherently report.
    virtual bool setBodyTrigger(BodyId body, bool enabled) = 0;
    virtual bool getBodyTrigger(BodyId body, bool& out_enabled) const = 0;
    virtual bool setBodyCollisionMask(BodyId body, const CollisionMask& mask) = 0;
    virtual bool getBodyCollisionMask(BodyId body, CollisionMask& out_mask) const = 0;
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
