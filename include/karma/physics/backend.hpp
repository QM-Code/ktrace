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
    glm::vec3 local_center{0.0f, 0.0f, 0.0f};
};

struct BodyDesc {
    BodyTransform transform{};
    ColliderShapeDesc collider_shape{};
    glm::vec3 linear_velocity{0.0f, 0.0f, 0.0f};
    glm::vec3 angular_velocity{0.0f, 0.0f, 0.0f};
    float friction = 0.5f;
    float restitution = 0.0f;
    float linear_damping = 0.0f;
    float angular_damping = 0.0f;
    float mass = 0.0f;
    bool is_static = true;
    bool is_kinematic = false;
    bool awake = true;
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
    // - Kinematic enablement applies only to dynamic-capable bodies.
    // - Static bodies cannot be configured as kinematic.
    // - For invalid, unknown, or non-dynamic bodies, calls return false.
    // - Dynamic bodies with both rotation_locked and translation_locked enabled are invalid for this contract.
    //   Backends reject these create/mutation requests deterministically.
    // - Rotation/translation lock runtime mutation support is backend-dependent;
    //   unsupported transitions return false so callers can apply deterministic fallback behavior.
    virtual bool setBodyGravityEnabled(BodyId body, bool enabled) = 0;
    virtual bool getBodyGravityEnabled(BodyId body, bool& out_enabled) const = 0;
    virtual bool setBodyKinematic(BodyId body, bool enabled) = 0;
    virtual bool getBodyKinematic(BodyId body, bool& out_enabled) const = 0;
    // Runtime awake/sleep contract:
    // - set/get awake state applies only to dynamic bodies in this slice.
    // - For invalid, unknown, or non-dynamic bodies, calls return false.
    // - set* may return false when runtime awake/sleep mutation is unsupported by the backend;
    //   callers use this to drive deterministic fallback behavior (for example rebuild).
    virtual bool setBodyAwake(BodyId body, bool enabled) = 0;
    virtual bool getBodyAwake(BodyId body, bool& out_enabled) const = 0;
    // Runtime force/impulse command contract:
    // - Applies only to dynamic and runtime-eligible bodies.
    // - Force is interpreted as a per-step command; callers re-issue each pre-sim pass while desired.
    // - Impulse is interpreted as one-shot; callers re-issue only when desired.
    // - Torque is interpreted as a per-step command; callers re-issue each pre-sim pass while desired.
    // - Angular impulse is interpreted as one-shot; callers re-issue only when desired.
    // - Input vectors must be finite.
    // - For invalid, unknown, static, or otherwise ineligible bodies, calls return false.
    virtual bool addBodyForce(BodyId body, const glm::vec3& force) = 0;
    virtual bool addBodyLinearImpulse(BodyId body, const glm::vec3& impulse) = 0;
    virtual bool addBodyTorque(BodyId body, const glm::vec3& torque) = 0;
    virtual bool addBodyAngularImpulse(BodyId body, const glm::vec3& impulse) = 0;
    // Runtime velocity contract:
    // - get/set velocity applies only to dynamic bodies in this slice.
    // - For invalid, unknown, or non-dynamic bodies, calls return false.
    virtual bool setBodyLinearVelocity(BodyId body, const glm::vec3& velocity) = 0;
    virtual bool getBodyLinearVelocity(BodyId body, glm::vec3& out_velocity) const = 0;
    virtual bool setBodyAngularVelocity(BodyId body, const glm::vec3& velocity) = 0;
    virtual bool getBodyAngularVelocity(BodyId body, glm::vec3& out_velocity) const = 0;
    // Runtime damping contract:
    // - get/set damping applies only to dynamic bodies in this slice.
    // - Damping values must be finite and non-negative.
    // - For invalid, unknown, or non-dynamic bodies, calls return false.
    virtual bool setBodyLinearDamping(BodyId body, float damping) = 0;
    virtual bool getBodyLinearDamping(BodyId body, float& out_damping) const = 0;
    virtual bool setBodyAngularDamping(BodyId body, float damping) = 0;
    virtual bool getBodyAngularDamping(BodyId body, float& out_damping) const = 0;
    // Runtime motion-lock contract:
    // - get/set lock state applies only to dynamic bodies in this slice.
    // - For invalid, unknown, or non-dynamic bodies, calls return false.
    // - set* may return false when runtime lock mutation is unsupported by the backend;
    //   callers use this to drive deterministic fallback behavior (for example rebuild).
    virtual bool setBodyRotationLocked(BodyId body, bool locked) = 0;
    virtual bool getBodyRotationLocked(BodyId body, bool& out_locked) const = 0;
    virtual bool setBodyTranslationLocked(BodyId body, bool locked) = 0;
    virtual bool getBodyTranslationLocked(BodyId body, bool& out_locked) const = 0;
    // Collider runtime property contract:
    // - Returns false for invalid/unknown body ids.
    // - Backends may report false when a runtime mutation is unsupported; this is used by callers to apply deterministic
    //   fallback behavior (for example runtime rebuild) instead of silent no-op.
    // - get* methods return the currently active property values that the backend can coherently report.
    virtual bool setBodyTrigger(BodyId body, bool enabled) = 0;
    virtual bool getBodyTrigger(BodyId body, bool& out_enabled) const = 0;
    virtual bool setBodyCollisionMask(BodyId body, const CollisionMask& mask) = 0;
    virtual bool getBodyCollisionMask(BodyId body, CollisionMask& out_mask) const = 0;
    // Runtime collider material contract:
    // - set/get applies to valid known body ids (static + dynamic).
    // - Friction must be finite and >= 0.
    // - Restitution must be finite and in [0, 1].
    // - Unsupported runtime mutations return false so callers can apply deterministic fallback behavior.
    virtual bool setBodyFriction(BodyId body, float friction) = 0;
    virtual bool getBodyFriction(BodyId body, float& out_friction) const = 0;
    virtual bool setBodyRestitution(BodyId body, float restitution) = 0;
    virtual bool getBodyRestitution(BodyId body, float& out_restitution) const = 0;
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
