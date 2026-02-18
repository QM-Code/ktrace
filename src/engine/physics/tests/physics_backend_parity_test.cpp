#include "karma/ecs/world.hpp"
#include "karma/physics/backend.hpp"
#include "karma/physics/physics_system.hpp"
#include "karma/physics/world.hpp"
#include "karma/scene/components.hpp"
#include "physics/ecs_sync_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <vector>

namespace {

using karma::physics_backend::BackendKind;
using karma::physics_backend::BackendKindName;
using karma::physics_backend::BodyDesc;
using karma::physics_backend::BodyId;
using karma::physics_backend::BodyTransform;
using karma::physics_backend::CollisionMask;
using BackendColliderShapeKind = karma::physics_backend::ColliderShapeKind;
using karma::physics_backend::RaycastHit;
using karma::physics_backend::CompiledBackends;
using karma::physics_backend::ParseBackendKind;

constexpr float kEpsilon = 1e-3f;
constexpr float kGravityDropMin = 1e-4f;

bool NearlyEqual(float lhs, float rhs, float epsilon = kEpsilon) {
    return std::fabs(lhs - rhs) <= epsilon;
}

bool NearlyEqualVec3(const glm::vec3& lhs, const glm::vec3& rhs, float epsilon = kEpsilon) {
    return NearlyEqual(lhs.x, rhs.x, epsilon) && NearlyEqual(lhs.y, rhs.y, epsilon)
           && NearlyEqual(lhs.z, rhs.z, epsilon);
}

bool NearlyEqualQuat(const glm::quat& lhs, const glm::quat& rhs, float epsilon = kEpsilon) {
    const glm::quat lhs_norm = glm::normalize(lhs);
    const glm::quat rhs_norm = glm::normalize(rhs);
    const float dot = std::fabs(glm::dot(lhs_norm, rhs_norm));
    return NearlyEqual(dot, 1.0f, epsilon);
}

bool EqualCollisionMask(const CollisionMask& lhs, const CollisionMask& rhs) {
    return lhs.layer == rhs.layer && lhs.collides_with == rhs.collides_with;
}

bool ContainsBackend(const std::vector<BackendKind>& values, BackendKind needle) {
    return std::find(values.begin(), values.end(), needle) != values.end();
}

bool ValidateTransform(const BodyTransform& actual,
                       const BodyTransform& expected,
                       std::string_view label,
                       BackendKind backend) {
    if (!NearlyEqualVec3(actual.position, expected.position)) {
        std::cerr << "backend=" << BackendKindName(backend) << " " << label
                  << " position mismatch: expected=(" << expected.position.x << "," << expected.position.y << ","
                  << expected.position.z << ") actual=(" << actual.position.x << "," << actual.position.y << ","
                  << actual.position.z << ")\n";
        return false;
    }
    if (!NearlyEqualQuat(actual.rotation, expected.rotation)) {
        std::cerr << "backend=" << BackendKindName(backend) << " " << label << " rotation mismatch\n";
        return false;
    }
    return true;
}

bool RunLifecycleChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to initialize\n";
        return false;
    }
    if (physics.selectedBackend() != backend) {
        std::cerr << "requested backend=" << BackendKindName(backend) << " selected="
                  << BackendKindName(physics.selectedBackend()) << "\n";
        return false;
    }

    BodyDesc static_desc{};
    static_desc.is_static = true;
    static_desc.transform.position = glm::vec3(10.0f, 20.0f, 30.0f);
    static_desc.transform.rotation = glm::normalize(glm::quat(0.95f, 0.1f, 0.2f, 0.05f));
    const BodyId static_body = physics.createBody(static_desc);
    if (static_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to create static body\n";
        physics.shutdown();
        return false;
    }

    BodyTransform observed{};
    if (!physics.getBodyTransform(static_body, observed)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to read static body transform\n";
        physics.shutdown();
        return false;
    }
    if (!ValidateTransform(observed, static_desc.transform, "static body initial", backend)) {
        physics.shutdown();
        return false;
    }

    BodyDesc dynamic_desc{};
    dynamic_desc.is_static = false;
    dynamic_desc.mass = 2.0f;
    dynamic_desc.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    dynamic_desc.transform.rotation = glm::normalize(glm::quat(0.9f, 0.1f, -0.2f, 0.3f));
    dynamic_desc.linear_velocity = glm::vec3(3.0f, 0.0f, 0.0f);
    dynamic_desc.angular_velocity = glm::vec3(0.0f, 1.0f, 0.0f);
    const BodyId dynamic_body = physics.createBody(dynamic_desc);
    if (dynamic_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to create dynamic body\n";
        physics.shutdown();
        return false;
    }

    BodyTransform target_transform{};
    target_transform.position = glm::vec3(-5.0f, 1.25f, 9.75f);
    target_transform.rotation = glm::normalize(glm::quat(0.82f, -0.21f, 0.44f, 0.29f));
    if (!physics.setBodyTransform(dynamic_body, target_transform)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to set dynamic body transform\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyTransform(dynamic_body, observed)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to get dynamic body transform after set\n";
        physics.shutdown();
        return false;
    }
    if (!ValidateTransform(observed, target_transform, "dynamic body set/get", backend)) {
        physics.shutdown();
        return false;
    }

    BodyTransform pre_sim_dynamic = observed;
    physics.beginFrame(1.0f / 60.0f);
    for (int i = 0; i < 10; ++i) {
        physics.simulateFixedStep(1.0f / 60.0f);
    }
    physics.endFrame();
    if (!physics.getBodyTransform(dynamic_body, observed)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to get dynamic body transform after sim\n";
        physics.shutdown();
        return false;
    }
    if (!std::isfinite(observed.position.x) || !std::isfinite(observed.position.y) || !std::isfinite(observed.position.z)) {
        std::cerr << "backend=" << BackendKindName(backend) << " produced non-finite dynamic body position\n";
        physics.shutdown();
        return false;
    }
    if (!(observed.position.y < (pre_sim_dynamic.position.y - kGravityDropMin))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " gravity sanity failed: expected y to decrease from " << pre_sim_dynamic.position.y << " but got "
                  << observed.position.y << "\n";
        physics.shutdown();
        return false;
    }

    BodyTransform static_after_sim{};
    if (!physics.getBodyTransform(static_body, static_after_sim)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to get static body transform after sim\n";
        physics.shutdown();
        return false;
    }
    if (!ValidateTransform(static_after_sim, static_desc.transform, "static body after sim", backend)) {
        physics.shutdown();
        return false;
    }

    physics.destroyBody(dynamic_body);
    physics.destroyBody(static_body);
    if (physics.getBodyTransform(dynamic_body, observed)) {
        std::cerr << "backend=" << BackendKindName(backend) << " dynamic body still readable after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyTransform(static_body, observed)) {
        std::cerr << "backend=" << BackendKindName(backend) << " static body still readable after destroy\n";
        physics.shutdown();
        return false;
    }

    physics.shutdown();
    return true;
}

bool RunColliderShapeAndRuntimePropertyChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (shape/property check)\n";
        return false;
    }

    BodyDesc box_desc{};
    box_desc.is_static = true;
    box_desc.collider_shape.kind = BackendColliderShapeKind::Box;
    box_desc.collider_shape.box_half_extents = glm::vec3(0.75f, 0.4f, 1.2f);
    box_desc.transform.position = glm::vec3(-2.0f, 1.0f, 0.0f);
    const BodyId box_body = physics.createBody(box_desc);
    if (box_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create box-shape body\n";
        physics.shutdown();
        return false;
    }

    BodyDesc sphere_desc{};
    sphere_desc.is_static = false;
    sphere_desc.mass = 1.0f;
    sphere_desc.collider_shape.kind = BackendColliderShapeKind::Sphere;
    sphere_desc.collider_shape.sphere_radius = 0.6f;
    sphere_desc.transform.position = glm::vec3(0.0f, 4.0f, 0.0f);
    const BodyId sphere_body = physics.createBody(sphere_desc);
    if (sphere_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create sphere-shape body\n";
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }

    BodyDesc capsule_desc{};
    capsule_desc.is_static = true;
    capsule_desc.collider_shape.kind = BackendColliderShapeKind::Capsule;
    capsule_desc.collider_shape.capsule_radius = 0.45f;
    capsule_desc.collider_shape.capsule_half_height = 0.9f;
    capsule_desc.transform.position = glm::vec3(2.0f, 1.5f, 0.0f);
    const BodyId capsule_body = physics.createBody(capsule_desc);
    if (capsule_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create capsule-shape body\n";
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }

    BodyTransform observed{};
    if (!physics.getBodyTransform(box_body, observed)
        || !ValidateTransform(observed, box_desc.transform, "box-shape transform", backend)) {
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyTransform(sphere_body, observed)
        || !ValidateTransform(observed, sphere_desc.transform, "sphere-shape transform", backend)) {
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyTransform(capsule_body, observed)
        || !ValidateTransform(observed, capsule_desc.transform, "capsule-shape transform", backend)) {
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }

    bool trigger_enabled = false;
    if (!physics.getBodyTrigger(sphere_body, trigger_enabled) || trigger_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " trigger default state mismatch (shape/property check)\n";
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }
    if (!physics.setBodyTrigger(sphere_body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTrigger failed for valid body\n";
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyTrigger(sphere_body, trigger_enabled) || !trigger_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " trigger roundtrip mismatch after set\n";
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }

    const CollisionMask default_mask{};
    CollisionMask observed_mask{};
    if (!physics.getBodyCollisionMask(sphere_body, observed_mask) || !EqualCollisionMask(observed_mask, default_mask)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " collision mask default mismatch\n";
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }

    const CollisionMask updated_mask{0x2u, 0x3u};
    const bool set_mask_result = physics.setBodyCollisionMask(sphere_body, updated_mask);
    if (!physics.getBodyCollisionMask(sphere_body, observed_mask)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyCollisionMask failed after runtime set attempt\n";
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }
    if (set_mask_result && !EqualCollisionMask(observed_mask, updated_mask)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " collision mask roundtrip mismatch on successful runtime mutation\n";
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }
    if (!set_mask_result && !EqualCollisionMask(observed_mask, default_mask)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " collision mask state changed despite failed runtime mutation\n";
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }

    const CollisionMask invalid_mask{0u, 0xFFFFFFFFu};
    if (physics.setBodyCollisionMask(sphere_body, invalid_mask)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyCollisionMask unexpectedly succeeded for invalid mask\n";
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }
    const CollisionMask expected_after_invalid = set_mask_result ? updated_mask : default_mask;
    if (!physics.getBodyCollisionMask(sphere_body, observed_mask)
        || !EqualCollisionMask(observed_mask, expected_after_invalid)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " collision mask changed after invalid mask rejection\n";
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(capsule_body);
    physics.destroyBody(sphere_body);
    physics.destroyBody(box_body);
    physics.shutdown();
    return true;
}

bool RunGroundCollisionChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to initialize (ground check)\n";
        return false;
    }

    std::vector<std::pair<BodyId, BodyTransform>> static_tiles{};
    for (int x = -1; x <= 1; ++x) {
        for (int z = -1; z <= 1; ++z) {
            BodyDesc tile_desc{};
            tile_desc.is_static = true;
            tile_desc.transform.position = glm::vec3(static_cast<float>(x), 0.0f, static_cast<float>(z));
            tile_desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
            const BodyId tile = physics.createBody(tile_desc);
            if (tile == karma::physics_backend::kInvalidBodyId) {
                std::cerr << "backend=" << BackendKindName(backend) << " failed to create ground tile\n";
                physics.shutdown();
                return false;
            }
            static_tiles.emplace_back(tile, tile_desc.transform);
        }
    }

    BodyDesc dynamic_desc{};
    dynamic_desc.is_static = false;
    dynamic_desc.mass = 2.0f;
    dynamic_desc.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    dynamic_desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const BodyId dynamic_body = physics.createBody(dynamic_desc);
    if (dynamic_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to create dynamic body (ground check)\n";
        physics.shutdown();
        return false;
    }

    BodyTransform initial_dynamic{};
    if (!physics.getBodyTransform(dynamic_body, initial_dynamic)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to read initial dynamic transform (ground check)\n";
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 60.0f);
    for (int i = 0; i < 240; ++i) {
        physics.simulateFixedStep(1.0f / 60.0f);
    }
    physics.endFrame();

    BodyTransform final_dynamic{};
    if (!physics.getBodyTransform(dynamic_body, final_dynamic)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to read final dynamic transform (ground check)\n";
        physics.shutdown();
        return false;
    }
    if (!std::isfinite(final_dynamic.position.x) || !std::isfinite(final_dynamic.position.y)
        || !std::isfinite(final_dynamic.position.z)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " produced non-finite dynamic position (ground check)\n";
        physics.shutdown();
        return false;
    }
    if (!(final_dynamic.position.y < (initial_dynamic.position.y - 0.1f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " body did not fall enough in ground check; initial_y=" << initial_dynamic.position.y
                  << " final_y=" << final_dynamic.position.y << "\n";
        physics.shutdown();
        return false;
    }
    if (final_dynamic.position.y < 0.6f || final_dynamic.position.y > 1.4f) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ground collision sanity failed; expected resting y around 1.0, got "
                  << final_dynamic.position.y << "\n";
        physics.shutdown();
        return false;
    }
    if (std::fabs(final_dynamic.position.x) > 0.2f || std::fabs(final_dynamic.position.z) > 0.2f) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " unexpected lateral drift in ground check; final=(" << final_dynamic.position.x << ","
                  << final_dynamic.position.y << "," << final_dynamic.position.z << ")\n";
        physics.shutdown();
        return false;
    }

    for (const auto& [tile, expected_transform] : static_tiles) {
        BodyTransform observed{};
        if (!physics.getBodyTransform(tile, observed)) {
            std::cerr << "backend=" << BackendKindName(backend) << " failed reading ground tile transform\n";
            physics.shutdown();
            return false;
        }
        if (!ValidateTransform(observed, expected_transform, "ground tile after sim", backend)) {
            physics.shutdown();
            return false;
        }
    }

    physics.destroyBody(dynamic_body);
    for (const auto& [tile, expected_transform] : static_tiles) {
        (void)expected_transform;
        physics.destroyBody(tile);
    }
    physics.shutdown();
    return true;
}

bool RunVelocityIntegrationChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to initialize (velocity check)\n";
        return false;
    }

    BodyDesc dynamic_desc{};
    dynamic_desc.is_static = false;
    dynamic_desc.mass = 1.0f;
    dynamic_desc.transform.position = glm::vec3(0.0f, 50.0f, 0.0f);
    dynamic_desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    dynamic_desc.linear_velocity = glm::vec3(4.0f, 0.0f, 0.0f);
    dynamic_desc.angular_velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    const BodyId dynamic_body = physics.createBody(dynamic_desc);
    if (dynamic_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to create dynamic body (velocity check)\n";
        physics.shutdown();
        return false;
    }

    BodyTransform start{};
    if (!physics.getBodyTransform(dynamic_body, start)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to read start transform (velocity check)\n";
        physics.shutdown();
        return false;
    }

    constexpr float step_dt = 1.0f / 120.0f;
    constexpr int step_count = 60;
    physics.beginFrame(step_dt);
    for (int i = 0; i < step_count; ++i) {
        physics.simulateFixedStep(step_dt);
    }
    physics.endFrame();

    BodyTransform finish{};
    if (!physics.getBodyTransform(dynamic_body, finish)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to read finish transform (velocity check)\n";
        physics.shutdown();
        return false;
    }

    if (!std::isfinite(finish.position.x) || !std::isfinite(finish.position.y) || !std::isfinite(finish.position.z)) {
        std::cerr << "backend=" << BackendKindName(backend) << " non-finite position (velocity check)\n";
        physics.shutdown();
        return false;
    }

    const float dx = finish.position.x - start.position.x;
    const float dy = finish.position.y - start.position.y;
    if (!(dx > 0.5f && dx < 4.5f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " unexpected horizontal integration: dx=" << dx << " (velocity check)\n";
        physics.shutdown();
        return false;
    }
    if (!(dy < -0.05f && dy > -8.0f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " unexpected gravity integration: dy=" << dy << " (velocity check)\n";
        physics.shutdown();
        return false;
    }
    if (std::fabs(finish.position.z - start.position.z) > 0.2f) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " unexpected z drift in velocity check: start_z=" << start.position.z
                  << " finish_z=" << finish.position.z << "\n";
        physics.shutdown();
        return false;
    }

    physics.destroyBody(dynamic_body);
    physics.shutdown();
    return true;
}

bool RunInvalidBodyApiChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to initialize (invalid-id check)\n";
        return false;
    }

    BodyTransform probe{};
    probe.position = glm::vec3(7.0f, 8.0f, 9.0f);
    probe.rotation = glm::normalize(glm::quat(0.9f, 0.1f, 0.2f, 0.3f));
    BodyTransform out{};

    if (physics.getBodyTransform(karma::physics_backend::kInvalidBodyId, out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTransform unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTransform(karma::physics_backend::kInvalidBodyId, probe)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTransform unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    bool gravity_enabled = false;
    if (physics.getBodyGravityEnabled(karma::physics_backend::kInvalidBodyId, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyGravityEnabled unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyGravityEnabled(karma::physics_backend::kInvalidBodyId, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyGravityEnabled unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTrigger(karma::physics_backend::kInvalidBodyId, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTrigger unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyTrigger(karma::physics_backend::kInvalidBodyId, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTrigger unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    CollisionMask mask_out{};
    if (physics.setBodyCollisionMask(karma::physics_backend::kInvalidBodyId, CollisionMask{})) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyCollisionMask unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyCollisionMask(karma::physics_backend::kInvalidBodyId, mask_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyCollisionMask unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    physics.destroyBody(karma::physics_backend::kInvalidBodyId);

    BodyDesc desc{};
    desc.is_static = true;
    desc.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const BodyId body = physics.createBody(desc);
    if (body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create body (invalid-id check)\n";
        physics.shutdown();
        return false;
    }

    const BodyId bogus = body + 1000000;
    if (physics.getBodyTransform(bogus, out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTransform unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTransform(bogus, probe)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTransform unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyGravityEnabled(bogus, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyGravityEnabled unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyGravityEnabled(bogus, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyGravityEnabled unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTrigger(bogus, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTrigger unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyTrigger(bogus, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTrigger unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyCollisionMask(bogus, CollisionMask{})) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyCollisionMask unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyCollisionMask(bogus, mask_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyCollisionMask unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    physics.destroyBody(bogus);

    physics.destroyBody(body);
    if (physics.getBodyTransform(body, out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTransform unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTransform(body, probe)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTransform unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyGravityEnabled(body, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyGravityEnabled unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyGravityEnabled(body, false)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyGravityEnabled unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTrigger(body, false)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTrigger unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyTrigger(body, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTrigger unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyCollisionMask(body, CollisionMask{})) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyCollisionMask unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyCollisionMask(body, mask_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyCollisionMask unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }

    physics.shutdown();
    return true;
}

bool RunBodyGravityFlagChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to initialize (gravity-flag check)\n";
        return false;
    }

    BodyDesc static_desc{};
    static_desc.is_static = true;
    static_desc.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    const BodyId static_body = physics.createBody(static_desc);
    if (static_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to create static body (gravity-flag check)\n";
        physics.shutdown();
        return false;
    }
    bool gravity_enabled = false;
    if (physics.getBodyGravityEnabled(static_body, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyGravityEnabled unexpectedly succeeded for static body\n";
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }
    if (physics.setBodyGravityEnabled(static_body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyGravityEnabled unexpectedly succeeded for static body\n";
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    BodyDesc no_gravity_desc{};
    no_gravity_desc.is_static = false;
    no_gravity_desc.mass = 1.0f;
    no_gravity_desc.gravity_enabled = false;
    no_gravity_desc.transform.position = glm::vec3(0.0f, 10.0f, 0.0f);
    const BodyId no_gravity_body = physics.createBody(no_gravity_desc);
    if (no_gravity_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create no-gravity dynamic body (gravity-flag check)\n";
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    if (!physics.getBodyGravityEnabled(no_gravity_body, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyGravityEnabled failed for no-gravity dynamic body\n";
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }
    if (gravity_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " gravity flag mismatch for no-gravity dynamic body; expected=false actual=true\n";
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    BodyTransform no_gravity_start{};
    if (!physics.getBodyTransform(no_gravity_body, no_gravity_start)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read no-gravity body start transform\n";
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 120.0f);
    for (int i = 0; i < 120; ++i) {
        physics.simulateFixedStep(1.0f / 120.0f);
    }
    physics.endFrame();

    BodyTransform no_gravity_finish{};
    if (!physics.getBodyTransform(no_gravity_body, no_gravity_finish)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read no-gravity body finish transform\n";
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    const float no_gravity_drop = no_gravity_start.position.y - no_gravity_finish.position.y;
    if (!std::isfinite(no_gravity_drop) || no_gravity_drop > 0.2f) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " gravity-disabled body dropped unexpectedly: drop=" << no_gravity_drop << "\n";
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    BodyDesc runtime_toggle_desc{};
    runtime_toggle_desc.is_static = false;
    runtime_toggle_desc.mass = 1.0f;
    runtime_toggle_desc.gravity_enabled = false;
    runtime_toggle_desc.transform.position = glm::vec3(2.0f, 10.0f, 0.0f);
    const BodyId runtime_toggle_body = physics.createBody(runtime_toggle_desc);
    if (runtime_toggle_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create runtime-toggle body (gravity-flag check)\n";
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    if (!physics.setBodyGravityEnabled(runtime_toggle_body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyGravityEnabled failed for runtime-toggle body\n";
        physics.destroyBody(runtime_toggle_body);
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyGravityEnabled(runtime_toggle_body, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyGravityEnabled failed after runtime toggle\n";
        physics.destroyBody(runtime_toggle_body);
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }
    if (!gravity_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " gravity flag did not update after runtime toggle\n";
        physics.destroyBody(runtime_toggle_body);
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    BodyTransform runtime_toggle_start{};
    if (!physics.getBodyTransform(runtime_toggle_body, runtime_toggle_start)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read runtime-toggle body start transform\n";
        physics.destroyBody(runtime_toggle_body);
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 120.0f);
    for (int i = 0; i < 120; ++i) {
        physics.simulateFixedStep(1.0f / 120.0f);
    }
    physics.endFrame();

    BodyTransform runtime_toggle_finish{};
    if (!physics.getBodyTransform(runtime_toggle_body, runtime_toggle_finish)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read runtime-toggle body finish transform\n";
        physics.destroyBody(runtime_toggle_body);
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    const float runtime_toggle_drop = runtime_toggle_start.position.y - runtime_toggle_finish.position.y;
    if (!(runtime_toggle_drop > 0.5f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " gravity-enabled runtime-toggle body did not fall enough: drop=" << runtime_toggle_drop << "\n";
        physics.destroyBody(runtime_toggle_body);
        physics.destroyBody(no_gravity_body);
        physics.destroyBody(static_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(runtime_toggle_body);
    physics.destroyBody(no_gravity_body);
    physics.destroyBody(static_body);
    physics.shutdown();
    return true;
}

bool RunBodyRotationLockChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to initialize (rotation-lock check)\n";
        return false;
    }

    BodyDesc unlocked_desc{};
    unlocked_desc.is_static = false;
    unlocked_desc.mass = 1.0f;
    unlocked_desc.gravity_enabled = false;
    unlocked_desc.transform.position = glm::vec3(0.0f, 5.0f, 0.0f);
    unlocked_desc.angular_velocity = glm::vec3(0.0f, 8.0f, 0.0f);
    const BodyId unlocked_body = physics.createBody(unlocked_desc);
    if (unlocked_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create unlocked body (rotation-lock check)\n";
        physics.shutdown();
        return false;
    }

    BodyDesc locked_desc = unlocked_desc;
    locked_desc.rotation_locked = true;
    locked_desc.transform.position = glm::vec3(2.0f, 5.0f, 0.0f);
    const BodyId locked_body = physics.createBody(locked_desc);
    if (locked_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create locked body (rotation-lock check)\n";
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }

    BodyTransform unlocked_start{};
    BodyTransform locked_start{};
    if (!physics.getBodyTransform(unlocked_body, unlocked_start)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read unlocked start transform (rotation-lock check)\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyTransform(locked_body, locked_start)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read locked start transform (rotation-lock check)\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 120.0f);
    for (int i = 0; i < 240; ++i) {
        physics.simulateFixedStep(1.0f / 120.0f);
    }
    physics.endFrame();

    BodyTransform unlocked_finish{};
    BodyTransform locked_finish{};
    if (!physics.getBodyTransform(unlocked_body, unlocked_finish)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read unlocked finish transform (rotation-lock check)\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyTransform(locked_body, locked_finish)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read locked finish transform (rotation-lock check)\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }

    const float unlocked_dot = std::fabs(glm::dot(glm::normalize(unlocked_start.rotation),
                                                   glm::normalize(unlocked_finish.rotation)));
    if (!(unlocked_dot < 0.95f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " unlocked body did not rotate enough in rotation-lock check: dot=" << unlocked_dot << "\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }

    const float locked_dot = std::fabs(glm::dot(glm::normalize(locked_start.rotation),
                                                 glm::normalize(locked_finish.rotation)));
    if (!(locked_dot > 0.999f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " rotation-locked body rotated unexpectedly: dot=" << locked_dot << "\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(locked_body);
    physics.destroyBody(unlocked_body);
    physics.shutdown();
    return true;
}

bool RunBodyTranslationLockChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to initialize (translation-lock check)\n";
        return false;
    }

    BodyDesc unlocked_desc{};
    unlocked_desc.is_static = false;
    unlocked_desc.mass = 1.0f;
    unlocked_desc.gravity_enabled = false;
    unlocked_desc.transform.position = glm::vec3(-4.0f, 5.0f, 0.0f);
    unlocked_desc.linear_velocity = glm::vec3(3.0f, 0.0f, 0.0f);
    const BodyId unlocked_body = physics.createBody(unlocked_desc);
    if (unlocked_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create unlocked body (translation-lock check)\n";
        physics.shutdown();
        return false;
    }

    BodyDesc locked_desc = unlocked_desc;
    locked_desc.translation_locked = true;
    locked_desc.transform.position = glm::vec3(8.0f, 5.0f, 0.0f);
    const BodyId locked_body = physics.createBody(locked_desc);
    if (locked_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create translation-locked body (translation-lock check)\n";
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }

    BodyTransform unlocked_start{};
    BodyTransform locked_start{};
    if (!physics.getBodyTransform(unlocked_body, unlocked_start)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read unlocked start transform (translation-lock check)\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyTransform(locked_body, locked_start)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read locked start transform (translation-lock check)\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 120.0f);
    for (int i = 0; i < 240; ++i) {
        physics.simulateFixedStep(1.0f / 120.0f);
    }
    physics.endFrame();

    BodyTransform unlocked_finish{};
    BodyTransform locked_finish{};
    if (!physics.getBodyTransform(unlocked_body, unlocked_finish)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read unlocked finish transform (translation-lock check)\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyTransform(locked_body, locked_finish)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read locked finish transform (translation-lock check)\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }

    const float unlocked_dx = unlocked_finish.position.x - unlocked_start.position.x;
    if (!(unlocked_dx > 1.5f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " unlocked body did not translate enough in translation-lock check: dx=" << unlocked_dx << "\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }

    const float locked_translation = glm::length(locked_finish.position - locked_start.position);
    if (!(locked_translation < 0.15f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " translation-locked body translated unexpectedly: delta=" << locked_translation << "\n";
        physics.destroyBody(locked_body);
        physics.destroyBody(unlocked_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(locked_body);
    physics.destroyBody(unlocked_body);
    physics.shutdown();
    return true;
}

bool RunReinitCycleChecks(BackendKind backend) {
    constexpr int cycle_count = 4;
    for (int cycle = 0; cycle < cycle_count; ++cycle) {
        karma::physics::PhysicsSystem physics;
        physics.setBackend(backend);
        physics.init();

        if (!physics.isInitialized()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " failed to initialize on cycle " << cycle << "\n";
            return false;
        }
        if (physics.selectedBackend() != backend) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " selected backend mismatch on cycle " << cycle << ": selected="
                      << BackendKindName(physics.selectedBackend()) << "\n";
            physics.shutdown();
            return false;
        }

        BodyDesc desc{};
        desc.is_static = false;
        desc.mass = 1.0f;
        desc.transform.position = glm::vec3(0.0f, 10.0f + static_cast<float>(cycle), 0.0f);
        desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        desc.linear_velocity = glm::vec3(1.0f + static_cast<float>(cycle), 0.0f, 0.0f);
        const BodyId body = physics.createBody(desc);
        if (body == karma::physics_backend::kInvalidBodyId) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " failed to create body on cycle " << cycle << "\n";
            physics.shutdown();
            return false;
        }

        physics.beginFrame(1.0f / 60.0f);
        for (int i = 0; i < 8; ++i) {
            physics.simulateFixedStep(1.0f / 60.0f);
        }
        physics.endFrame();

        BodyTransform observed{};
        if (!physics.getBodyTransform(body, observed)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " failed to read body on cycle " << cycle << "\n";
            physics.shutdown();
            return false;
        }
        if (!std::isfinite(observed.position.x) || !std::isfinite(observed.position.y) || !std::isfinite(observed.position.z)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " non-finite body position on cycle " << cycle << "\n";
            physics.shutdown();
            return false;
        }

        physics.destroyBody(body);
        physics.shutdown();

        BodyTransform probe{};
        if (physics.getBodyTransform(body, probe)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " body remained readable after shutdown on cycle " << cycle << "\n";
            return false;
        }
        if (physics.setBodyTransform(body, desc.transform)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " setBodyTransform succeeded after shutdown on cycle " << cycle << "\n";
            return false;
        }
    }
    return true;
}

bool RunRestingStabilityChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to initialize (resting check)\n";
        return false;
    }

    BodyDesc ground_desc{};
    ground_desc.is_static = true;
    ground_desc.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    ground_desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const BodyId ground = physics.createBody(ground_desc);
    if (ground == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to create ground (resting check)\n";
        physics.shutdown();
        return false;
    }

    BodyDesc drop_desc{};
    drop_desc.is_static = false;
    drop_desc.mass = 1.0f;
    drop_desc.transform.position = glm::vec3(0.0f, 4.0f, 0.0f);
    drop_desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const BodyId body = physics.createBody(drop_desc);
    if (body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to create body (resting check)\n";
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 60.0f);
    for (int i = 0; i < 240; ++i) {
        physics.simulateFixedStep(1.0f / 60.0f);
    }
    physics.endFrame();

    BodyTransform settled{};
    if (!physics.getBodyTransform(body, settled)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to read settled transform\n";
        physics.shutdown();
        return false;
    }
    if (!std::isfinite(settled.position.x) || !std::isfinite(settled.position.y) || !std::isfinite(settled.position.z)) {
        std::cerr << "backend=" << BackendKindName(backend) << " non-finite settled transform\n";
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 60.0f);
    for (int i = 0; i < 240; ++i) {
        physics.simulateFixedStep(1.0f / 60.0f);
    }
    physics.endFrame();

    BodyTransform stable{};
    if (!physics.getBodyTransform(body, stable)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to read stable transform\n";
        physics.shutdown();
        return false;
    }

    const float dy = std::fabs(stable.position.y - settled.position.y);
    const float dx = std::fabs(stable.position.x - settled.position.x);
    const float dz = std::fabs(stable.position.z - settled.position.z);
    if (dy > 0.1f || dx > 0.15f || dz > 0.15f) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " resting stability drift too large: dx=" << dx << " dy=" << dy << " dz=" << dz << "\n";
        physics.shutdown();
        return false;
    }
    if (stable.position.y < 0.5f || stable.position.y > 1.5f) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " resting stability height out of range: y=" << stable.position.y << "\n";
        physics.shutdown();
        return false;
    }

    BodyTransform ground_after{};
    if (!physics.getBodyTransform(ground, ground_after)) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to read ground transform after sim\n";
        physics.shutdown();
        return false;
    }
    if (!ValidateTransform(ground_after, ground_desc.transform, "resting check ground after sim", backend)) {
        physics.shutdown();
        return false;
    }

    physics.destroyBody(body);
    physics.destroyBody(ground);
    physics.shutdown();
    return true;
}

bool RunRaycastQueryChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to initialize (raycast check)\n";
        return false;
    }

    BodyDesc lower_desc{};
    lower_desc.is_static = true;
    lower_desc.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
    lower_desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const BodyId lower_body = physics.createBody(lower_desc);
    if (lower_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to create lower static body (raycast check)\n";
        physics.shutdown();
        return false;
    }

    BodyDesc upper_desc{};
    upper_desc.is_static = true;
    upper_desc.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    upper_desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const BodyId upper_body = physics.createBody(upper_desc);
    if (upper_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to create upper static body (raycast check)\n";
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }

    constexpr float ray_max_distance = 10.0f;
    const glm::vec3 ray_origin(0.0f, 6.0f, 0.0f);
    const glm::vec3 ray_direction(0.0f, -1.0f, 0.0f);

    RaycastHit hit{};
    if (!physics.raycastClosest(ray_origin, ray_direction, ray_max_distance, hit)) {
        std::cerr << "backend=" << BackendKindName(backend) << " raycastClosest failed to hit closest body\n";
        physics.destroyBody(upper_body);
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }
    if (hit.body != upper_body) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest hit unexpected body: expected=" << upper_body
                  << " actual=" << hit.body << "\n";
        physics.destroyBody(upper_body);
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }
    if (!std::isfinite(hit.position.x) || !std::isfinite(hit.position.y) || !std::isfinite(hit.position.z)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest returned non-finite hit position\n";
        physics.destroyBody(upper_body);
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }
    if (!(hit.distance > 2.3f && hit.distance < 2.7f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest distance out of range for closest hit: distance=" << hit.distance << "\n";
        physics.destroyBody(upper_body);
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }
    if (!(hit.fraction > 0.23f && hit.fraction < 0.27f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest fraction out of range for closest hit: fraction=" << hit.fraction << "\n";
        physics.destroyBody(upper_body);
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }
    if (std::fabs(hit.position.x) > 0.05f || std::fabs(hit.position.z) > 0.05f
        || !(hit.position.y > 3.3f && hit.position.y < 3.7f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest hit position unexpected: pos=("
                  << hit.position.x << "," << hit.position.y << "," << hit.position.z << ")\n";
        physics.destroyBody(upper_body);
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }

    if (physics.raycastClosest(glm::vec3(2.0f, 6.0f, 0.0f), ray_direction, ray_max_distance, hit)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest unexpectedly hit for miss ray\n";
        physics.destroyBody(upper_body);
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }
    if (physics.raycastClosest(ray_origin, glm::vec3(0.0f, 0.0f, 0.0f), ray_max_distance, hit)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest unexpectedly succeeded with zero direction\n";
        physics.destroyBody(upper_body);
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }
    if (physics.raycastClosest(ray_origin, ray_direction, 0.0f, hit)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest unexpectedly succeeded with non-positive distance\n";
        physics.destroyBody(upper_body);
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(upper_body);
    if (!physics.raycastClosest(ray_origin, ray_direction, ray_max_distance, hit)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest failed after removing closest body\n";
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }
    if (hit.body != lower_body) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest after destroy hit unexpected body: expected=" << lower_body
                  << " actual=" << hit.body << "\n";
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }
    if (!(hit.distance > 5.3f && hit.distance < 5.7f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest distance out of range after destroy: distance=" << hit.distance << "\n";
        physics.destroyBody(lower_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(lower_body);
    physics.shutdown();
    return true;
}

bool RunUninitializedApiChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);

    BodyDesc desc{};
    desc.is_static = true;
    desc.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    BodyTransform transform{};
    transform.position = glm::vec3(4.0f, 5.0f, 6.0f);
    transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    BodyTransform out{};
    RaycastHit hit{};
    bool gravity_enabled = false;

    const BodyId before_init = physics.createBody(desc);
    if (before_init != karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " createBody succeeded before init\n";
        return false;
    }
    if (physics.getBodyTransform(1, out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTransform succeeded before init\n";
        return false;
    }
    if (physics.setBodyTransform(1, transform)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTransform succeeded before init\n";
        return false;
    }
    if (physics.getBodyGravityEnabled(1, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyGravityEnabled succeeded before init\n";
        return false;
    }
    if (physics.setBodyGravityEnabled(1, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyGravityEnabled succeeded before init\n";
        return false;
    }
    if (physics.raycastClosest(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f, hit)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest succeeded before init\n";
        return false;
    }
    physics.destroyBody(1);

    physics.init();
    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize in uninitialized-api check\n";
        return false;
    }
    const BodyId body = physics.createBody(desc);
    if (body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create body after init in uninitialized-api check\n";
        physics.shutdown();
        return false;
    }
    physics.destroyBody(body);
    physics.shutdown();

    const BodyId after_shutdown = physics.createBody(desc);
    if (after_shutdown != karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " createBody succeeded after shutdown\n";
        return false;
    }
    if (physics.getBodyTransform(body, out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTransform succeeded after shutdown\n";
        return false;
    }
    if (physics.setBodyTransform(body, transform)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTransform succeeded after shutdown\n";
        return false;
    }
    if (physics.getBodyGravityEnabled(body, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyGravityEnabled succeeded after shutdown\n";
        return false;
    }
    if (physics.setBodyGravityEnabled(body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyGravityEnabled succeeded after shutdown\n";
        return false;
    }
    if (physics.raycastClosest(glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, -1.0f, 0.0f), 5.0f, hit)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " raycastClosest succeeded after shutdown\n";
        return false;
    }
    physics.destroyBody(body);

    return true;
}

bool RunRepeatabilityChecks(BackendKind backend) {
    auto run_trial = [backend](BodyTransform& out_transform, int trial_index) -> bool {
        karma::physics::PhysicsSystem physics;
        physics.setBackend(backend);
        physics.init();

        if (!physics.isInitialized()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " failed to initialize (repeatability trial " << trial_index << ")\n";
            return false;
        }

        BodyDesc ground_desc{};
        ground_desc.is_static = true;
        ground_desc.transform.position = glm::vec3(0.0f, 0.0f, 0.0f);
        ground_desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        const BodyId ground = physics.createBody(ground_desc);
        if (ground == karma::physics_backend::kInvalidBodyId) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " failed to create ground (repeatability trial " << trial_index << ")\n";
            physics.shutdown();
            return false;
        }

        BodyDesc body_desc{};
        body_desc.is_static = false;
        body_desc.mass = 1.0f;
        body_desc.transform.position = glm::vec3(0.25f, 6.0f, -0.25f);
        body_desc.transform.rotation = glm::normalize(glm::quat(0.95f, 0.1f, -0.05f, 0.2f));
        body_desc.linear_velocity = glm::vec3(2.0f, 0.5f, -1.0f);
        body_desc.angular_velocity = glm::vec3(0.3f, 0.2f, -0.1f);
        const BodyId body = physics.createBody(body_desc);
        if (body == karma::physics_backend::kInvalidBodyId) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " failed to create body (repeatability trial " << trial_index << ")\n";
            physics.destroyBody(ground);
            physics.shutdown();
            return false;
        }

        physics.beginFrame(1.0f / 120.0f);
        for (int i = 0; i < 180; ++i) {
            physics.simulateFixedStep(1.0f / 120.0f);
        }
        physics.endFrame();

        if (!physics.getBodyTransform(body, out_transform)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " failed to read body transform (repeatability trial " << trial_index << ")\n";
            physics.destroyBody(body);
            physics.destroyBody(ground);
            physics.shutdown();
            return false;
        }

        physics.destroyBody(body);
        physics.destroyBody(ground);
        physics.shutdown();
        return true;
    };

    BodyTransform first{};
    BodyTransform second{};
    if (!run_trial(first, 1) || !run_trial(second, 2)) {
        return false;
    }

    if (!std::isfinite(first.position.x) || !std::isfinite(first.position.y) || !std::isfinite(first.position.z)
        || !std::isfinite(second.position.x) || !std::isfinite(second.position.y) || !std::isfinite(second.position.z)) {
        std::cerr << "backend=" << BackendKindName(backend) << " non-finite transform in repeatability check\n";
        return false;
    }

    const float dx = std::fabs(first.position.x - second.position.x);
    const float dy = std::fabs(first.position.y - second.position.y);
    const float dz = std::fabs(first.position.z - second.position.z);
    if (dx > 0.1f || dy > 0.1f || dz > 0.1f) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " repeatability position drift too large: dx=" << dx << " dy=" << dy << " dz=" << dz << "\n";
        return false;
    }

    const float quat_dot = std::fabs(glm::dot(glm::normalize(first.rotation), glm::normalize(second.rotation)));
    if (quat_dot < 0.99f) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " repeatability rotation drift too large: dot=" << quat_dot << "\n";
        return false;
    }

    return true;
}

bool RunBackendSelectionChecks() {
    const std::vector<BackendKind> compiled_backends = CompiledBackends();
    if (compiled_backends.empty()) {
        std::cerr << "no physics backends are compiled (selection check)\n";
        return false;
    }

    {
        karma::physics::PhysicsSystem physics;
        physics.setBackend(BackendKind::Auto);
        physics.init();
        if (!physics.isInitialized()) {
            std::cerr << "auto backend selection failed to initialize\n";
            return false;
        }
        if (physics.selectedBackend() == BackendKind::Auto) {
            std::cerr << "auto backend selection kept selected backend as auto\n";
            physics.shutdown();
            return false;
        }
        if (!ContainsBackend(compiled_backends, physics.selectedBackend())) {
            std::cerr << "auto backend selected an uncompiled backend: "
                      << BackendKindName(physics.selectedBackend()) << "\n";
            physics.shutdown();
            return false;
        }
        if (physics.selectedBackend() != compiled_backends.front()) {
            std::cerr << "auto backend selection order mismatch: expected "
                      << BackendKindName(compiled_backends.front()) << " got "
                      << BackendKindName(physics.selectedBackend()) << "\n";
            physics.shutdown();
            return false;
        }
        physics.shutdown();
    }

    for (const BackendKind backend : compiled_backends) {
        karma::physics::PhysicsSystem physics;
        physics.setBackend(backend);
        physics.init();
        if (!physics.isInitialized()) {
            std::cerr << "explicit backend failed to initialize: " << BackendKindName(backend) << "\n";
            return false;
        }
        if (physics.selectedBackend() != backend) {
            std::cerr << "explicit backend selection mismatch: requested=" << BackendKindName(backend)
                      << " selected=" << BackendKindName(physics.selectedBackend()) << "\n";
            physics.shutdown();
            return false;
        }
        physics.shutdown();
    }

    if (!ContainsBackend(compiled_backends, BackendKind::Jolt)) {
        karma::physics::PhysicsSystem physics;
        physics.setBackend(BackendKind::Jolt);
        physics.init();
        if (physics.isInitialized()) {
            std::cerr << "jolt backend initialized despite not being compiled\n";
            physics.shutdown();
            return false;
        }
    }

    if (!ContainsBackend(compiled_backends, BackendKind::PhysX)) {
        karma::physics::PhysicsSystem physics;
        physics.setBackend(BackendKind::PhysX);
        physics.init();
        if (physics.isInitialized()) {
            std::cerr << "physx backend initialized despite not being compiled\n";
            physics.shutdown();
            return false;
        }
    }

    return true;
}

bool RunScenePhysicsComponentContractChecks() {
    using karma::scene::ColliderReconcileAction;
    using karma::scene::ColliderIntentComponent;
    using karma::scene::ColliderShapeKind;
    using karma::scene::CollisionMaskIntentComponent;
    using karma::scene::ControllerColliderCompatibility;
    using karma::scene::PhysicsTransformAuthority;
    using karma::scene::PhysicsTransformOwnershipComponent;
    using karma::scene::PhysicsComponentValidationError;
    using karma::scene::PlayerControllerIntentComponent;
    using karma::scene::RigidBodyIntentComponent;
    using karma::scene::TransformOwnershipValidationError;

    PhysicsComponentValidationError error = PhysicsComponentValidationError::None;

    const RigidBodyIntentComponent default_rigidbody{};
    if (!karma::scene::ValidateRigidBodyIntent(default_rigidbody, &error)
        || error != PhysicsComponentValidationError::None) {
        std::cerr << "scene physics component contract: default rigidbody intent invalid\n";
        return false;
    }

    const ColliderIntentComponent default_collider{};
    if (!karma::scene::ValidateColliderIntent(default_collider, &error)
        || error != PhysicsComponentValidationError::None) {
        std::cerr << "scene physics component contract: default collider intent invalid\n";
        return false;
    }

    const PlayerControllerIntentComponent default_controller{};
    if (!karma::scene::ValidatePlayerControllerIntent(default_controller, &error)
        || error != PhysicsComponentValidationError::None) {
        std::cerr << "scene physics component contract: default player-controller intent invalid\n";
        return false;
    }

    const CollisionMaskIntentComponent default_mask{};
    if (!karma::scene::ValidateCollisionMaskIntent(default_mask, &error)
        || error != PhysicsComponentValidationError::None) {
        std::cerr << "scene physics component contract: default collision mask invalid\n";
        return false;
    }

    RigidBodyIntentComponent invalid_mass_rigidbody{};
    invalid_mass_rigidbody.dynamic = true;
    invalid_mass_rigidbody.mass = 0.0f;
    if (karma::scene::ValidateRigidBodyIntent(invalid_mass_rigidbody, &error)
        || error != PhysicsComponentValidationError::InvalidMass) {
        std::cerr << "scene physics component contract: invalid rigidbody mass was not rejected\n";
        return false;
    }

    RigidBodyIntentComponent nan_velocity_rigidbody{};
    nan_velocity_rigidbody.linear_velocity.x = std::numeric_limits<float>::quiet_NaN();
    if (karma::scene::ValidateRigidBodyIntent(nan_velocity_rigidbody, &error)
        || error != PhysicsComponentValidationError::NonFiniteValue) {
        std::cerr << "scene physics component contract: rigidbody NaN velocity was not rejected\n";
        return false;
    }

    ColliderIntentComponent invalid_box{};
    invalid_box.shape = ColliderShapeKind::Box;
    invalid_box.half_extents = glm::vec3(-1.0f, 0.5f, 0.5f);
    if (karma::scene::ValidateColliderIntent(invalid_box, &error)
        || error != PhysicsComponentValidationError::NonPositiveDimension) {
        std::cerr << "scene physics component contract: invalid box dimensions were not rejected\n";
        return false;
    }

    ColliderIntentComponent invalid_sphere{};
    invalid_sphere.shape = ColliderShapeKind::Sphere;
    invalid_sphere.radius = std::numeric_limits<float>::quiet_NaN();
    if (karma::scene::ValidateColliderIntent(invalid_sphere, &error)
        || error != PhysicsComponentValidationError::NonFiniteValue) {
        std::cerr << "scene physics component contract: invalid sphere radius was not rejected\n";
        return false;
    }

    ColliderIntentComponent invalid_mesh{};
    invalid_mesh.shape = ColliderShapeKind::Mesh;
    invalid_mesh.mesh_path = "   ";
    if (karma::scene::ValidateColliderIntent(invalid_mesh, &error)
        || error != PhysicsComponentValidationError::EmptyMeshPath) {
        std::cerr << "scene physics component contract: empty mesh path was not rejected\n";
        return false;
    }

    ColliderIntentComponent invalid_mask{};
    invalid_mask.shape = ColliderShapeKind::Box;
    invalid_mask.mask.layer = 0u;
    if (karma::scene::ValidateColliderIntent(invalid_mask, &error)
        || error != PhysicsComponentValidationError::EmptyCollisionMask) {
        std::cerr << "scene physics component contract: empty collision layer mask was not rejected\n";
        return false;
    }

    PlayerControllerIntentComponent invalid_controller{};
    invalid_controller.half_extents = glm::vec3(0.0f, 0.9f, 0.5f);
    if (karma::scene::ValidatePlayerControllerIntent(invalid_controller, &error)
        || error != PhysicsComponentValidationError::NonPositiveDimension) {
        std::cerr << "scene physics component contract: invalid controller extents were not rejected\n";
        return false;
    }

    TransformOwnershipValidationError ownership_error = TransformOwnershipValidationError::None;
    const PhysicsTransformOwnershipComponent default_ownership{};
    if (!karma::scene::ValidateTransformOwnership(default_ownership, &ownership_error)
        || ownership_error != TransformOwnershipValidationError::None) {
        std::cerr << "scene physics component contract: default transform ownership invalid\n";
        return false;
    }
    if (!karma::scene::ShouldPullPhysicsTransformToScene(default_ownership)
        || karma::scene::ShouldPushSceneTransformToPhysics(default_ownership)) {
        std::cerr << "scene physics component contract: default transform ownership helper mismatch\n";
        return false;
    }

    PhysicsTransformOwnershipComponent scene_authoritative{};
    scene_authoritative.authority = PhysicsTransformAuthority::SceneAuthoritative;
    scene_authoritative.scene_transform_dirty = true;
    scene_authoritative.push_scene_transform_to_physics = true;
    scene_authoritative.pull_physics_transform_to_scene = false;
    if (!karma::scene::ValidateTransformOwnership(scene_authoritative, &ownership_error)
        || ownership_error != TransformOwnershipValidationError::None) {
        std::cerr << "scene physics component contract: valid scene-authoritative ownership rejected\n";
        return false;
    }
    if (!karma::scene::ShouldPushSceneTransformToPhysics(scene_authoritative)
        || karma::scene::ShouldPullPhysicsTransformToScene(scene_authoritative)) {
        std::cerr << "scene physics component contract: scene-authoritative helper mismatch\n";
        return false;
    }

    PhysicsTransformOwnershipComponent invalid_scene_pull = scene_authoritative;
    invalid_scene_pull.scene_transform_dirty = false;
    invalid_scene_pull.push_scene_transform_to_physics = false;
    invalid_scene_pull.pull_physics_transform_to_scene = true;
    if (karma::scene::ValidateTransformOwnership(invalid_scene_pull, &ownership_error)
        || ownership_error != TransformOwnershipValidationError::SceneAuthorityCannotPullFromPhysics) {
        std::cerr << "scene physics component contract: invalid scene pull policy was not rejected\n";
        return false;
    }

    PhysicsTransformOwnershipComponent invalid_scene_missing_push = scene_authoritative;
    invalid_scene_missing_push.push_scene_transform_to_physics = false;
    if (karma::scene::ValidateTransformOwnership(invalid_scene_missing_push, &ownership_error)
        || ownership_error != TransformOwnershipValidationError::SceneAuthorityDirtyRequiresPush) {
        std::cerr << "scene physics component contract: missing scene push policy was not rejected\n";
        return false;
    }

    PhysicsTransformOwnershipComponent invalid_physics_dirty = default_ownership;
    invalid_physics_dirty.scene_transform_dirty = true;
    if (karma::scene::ValidateTransformOwnership(invalid_physics_dirty, &ownership_error)
        || ownership_error != TransformOwnershipValidationError::PhysicsAuthorityCannotSetSceneDirty) {
        std::cerr << "scene physics component contract: physics-authoritative dirty scene was not rejected\n";
        return false;
    }

    PhysicsTransformOwnershipComponent invalid_physics_push = default_ownership;
    invalid_physics_push.push_scene_transform_to_physics = true;
    if (karma::scene::ValidateTransformOwnership(invalid_physics_push, &ownership_error)
        || ownership_error != TransformOwnershipValidationError::PhysicsAuthorityCannotPushSceneTransform) {
        std::cerr << "scene physics component contract: physics-authoritative push policy was not rejected\n";
        return false;
    }

    PhysicsTransformOwnershipComponent invalid_physics_no_pull = default_ownership;
    invalid_physics_no_pull.pull_physics_transform_to_scene = false;
    if (karma::scene::ValidateTransformOwnership(invalid_physics_no_pull, &ownership_error)
        || ownership_error != TransformOwnershipValidationError::PhysicsAuthorityRequiresPull) {
        std::cerr << "scene physics component contract: missing physics pull policy was not rejected\n";
        return false;
    }

    if (karma::scene::ClassifyColliderReconcileAction(default_collider, default_collider) != ColliderReconcileAction::NoOp) {
        std::cerr << "scene physics component contract: collider no-op reconcile classification mismatch\n";
        return false;
    }

    ColliderIntentComponent collider_flag_change = default_collider;
    collider_flag_change.enabled = false;
    if (karma::scene::ClassifyColliderReconcileAction(default_collider, collider_flag_change)
        != ColliderReconcileAction::UpdateRuntimeProperties) {
        std::cerr << "scene physics component contract: collider flag reconcile classification mismatch\n";
        return false;
    }

    ColliderIntentComponent collider_mask_change = default_collider;
    collider_mask_change.mask.collides_with = 0x3u;
    if (karma::scene::ClassifyColliderReconcileAction(default_collider, collider_mask_change)
        != ColliderReconcileAction::UpdateRuntimeProperties) {
        std::cerr << "scene physics component contract: collider mask reconcile classification mismatch\n";
        return false;
    }

    ColliderIntentComponent collider_shape_change = default_collider;
    collider_shape_change.shape = ColliderShapeKind::Capsule;
    collider_shape_change.radius = 0.45f;
    collider_shape_change.half_height = 0.8f;
    if (karma::scene::ClassifyColliderReconcileAction(default_collider, collider_shape_change)
        != ColliderReconcileAction::RebuildRuntimeShape) {
        std::cerr << "scene physics component contract: collider shape-change reconcile classification mismatch\n";
        return false;
    }

    ColliderIntentComponent collider_dimension_change = default_collider;
    collider_dimension_change.half_extents.x += 0.25f;
    if (karma::scene::ClassifyColliderReconcileAction(default_collider, collider_dimension_change)
        != ColliderReconcileAction::RebuildRuntimeShape) {
        std::cerr << "scene physics component contract: collider dimension-change reconcile classification mismatch\n";
        return false;
    }

    ColliderIntentComponent collider_inactive_field_change = default_collider;
    collider_inactive_field_change.radius = 99.0f;
    if (karma::scene::ClassifyColliderReconcileAction(default_collider, collider_inactive_field_change)
        != ColliderReconcileAction::NoOp) {
        std::cerr << "scene physics component contract: inactive shape field should not trigger reconcile action\n";
        return false;
    }

    ColliderIntentComponent collider_invalid_desired = default_collider;
    collider_invalid_desired.shape = ColliderShapeKind::Mesh;
    collider_invalid_desired.mesh_path = "";
    if (karma::scene::ClassifyColliderReconcileAction(default_collider, collider_invalid_desired)
        != ColliderReconcileAction::RejectInvalidIntent) {
        std::cerr << "scene physics component contract: invalid desired collider reconcile classification mismatch\n";
        return false;
    }

    ControllerColliderCompatibility compatibility =
        karma::scene::ClassifyControllerColliderCompatibility(default_controller, &default_collider);
    if (compatibility != ControllerColliderCompatibility::Compatible
        || !karma::scene::IsControllerColliderCompatible(compatibility)) {
        std::cerr << "scene physics component contract: default controller/collider compatibility mismatch\n";
        return false;
    }

    PlayerControllerIntentComponent controller_disabled = default_controller;
    controller_disabled.enabled = false;
    compatibility = karma::scene::ClassifyControllerColliderCompatibility(controller_disabled, nullptr);
    if (compatibility != ControllerColliderCompatibility::CompatibleControllerDisabled
        || !karma::scene::IsControllerColliderCompatible(compatibility)) {
        std::cerr << "scene physics component contract: disabled-controller compatibility mismatch\n";
        return false;
    }

    compatibility = karma::scene::ClassifyControllerColliderCompatibility(default_controller, nullptr);
    if (compatibility != ControllerColliderCompatibility::ColliderMissing
        || karma::scene::IsControllerColliderCompatible(compatibility)) {
        std::cerr << "scene physics component contract: missing-collider compatibility mismatch\n";
        return false;
    }

    ColliderIntentComponent trigger_collider = default_collider;
    trigger_collider.is_trigger = true;
    compatibility = karma::scene::ClassifyControllerColliderCompatibility(default_controller, &trigger_collider);
    if (compatibility != ControllerColliderCompatibility::ColliderIsTrigger
        || karma::scene::IsControllerColliderCompatible(compatibility)) {
        std::cerr << "scene physics component contract: trigger-collider compatibility mismatch\n";
        return false;
    }

    ColliderIntentComponent disabled_collider = default_collider;
    disabled_collider.enabled = false;
    compatibility = karma::scene::ClassifyControllerColliderCompatibility(default_controller, &disabled_collider);
    if (compatibility != ControllerColliderCompatibility::ColliderDisabled
        || karma::scene::IsControllerColliderCompatible(compatibility)) {
        std::cerr << "scene physics component contract: disabled-collider compatibility mismatch\n";
        return false;
    }

    ColliderIntentComponent mesh_collider{};
    mesh_collider.shape = ColliderShapeKind::Mesh;
    mesh_collider.mesh_path = "demo/worlds/controller-shape.glb";
    compatibility = karma::scene::ClassifyControllerColliderCompatibility(default_controller, &mesh_collider);
    if (compatibility != ControllerColliderCompatibility::UnsupportedColliderShape
        || karma::scene::IsControllerColliderCompatible(compatibility)) {
        std::cerr << "scene physics component contract: unsupported-shape compatibility mismatch\n";
        return false;
    }

    PlayerControllerIntentComponent invalid_compat_controller = default_controller;
    invalid_compat_controller.half_extents = glm::vec3(0.0f, 0.5f, 0.5f);
    compatibility =
        karma::scene::ClassifyControllerColliderCompatibility(invalid_compat_controller, &default_collider);
    if (compatibility != ControllerColliderCompatibility::ControllerInvalid
        || karma::scene::IsControllerColliderCompatible(compatibility)) {
        std::cerr << "scene physics component contract: invalid-controller compatibility mismatch\n";
        return false;
    }

    ColliderIntentComponent invalid_compat_collider = default_collider;
    invalid_compat_collider.half_extents.y = 0.0f;
    compatibility =
        karma::scene::ClassifyControllerColliderCompatibility(default_controller, &invalid_compat_collider);
    if (compatibility != ControllerColliderCompatibility::ColliderInvalid
        || karma::scene::IsControllerColliderCompatible(compatibility)) {
        std::cerr << "scene physics component contract: invalid-collider compatibility mismatch\n";
        return false;
    }

    karma::ecs::World world{};
    const auto entity_a = world.createEntity();
    const auto entity_b = world.createEntity();

    world.add<RigidBodyIntentComponent>(entity_a, RigidBodyIntentComponent{});
    world.add<ColliderIntentComponent>(entity_a, ColliderIntentComponent{});
    world.add<PlayerControllerIntentComponent>(entity_a, PlayerControllerIntentComponent{});

    world.add<ColliderIntentComponent>(entity_b, ColliderIntentComponent{});

    if (!world.has<RigidBodyIntentComponent>(entity_a)
        || !world.has<ColliderIntentComponent>(entity_a)
        || !world.has<PlayerControllerIntentComponent>(entity_a)) {
        std::cerr << "scene physics component contract: expected components missing from entity A\n";
        return false;
    }
    if (world.has<RigidBodyIntentComponent>(entity_b)) {
        std::cerr << "scene physics component contract: unexpected rigidbody component on entity B\n";
        return false;
    }

    const std::vector<karma::ecs::Entity> rigid_collider_view =
        world.view<RigidBodyIntentComponent, ColliderIntentComponent>();
    if (rigid_collider_view.size() != 1 || rigid_collider_view.front() != entity_a) {
        std::cerr << "scene physics component contract: rigidbody+collider view membership mismatch\n";
        return false;
    }

    const std::vector<karma::ecs::Entity> controller_view =
        world.view<RigidBodyIntentComponent, ColliderIntentComponent, PlayerControllerIntentComponent>();
    if (controller_view.size() != 1 || controller_view.front() != entity_a) {
        std::cerr << "scene physics component contract: controller view membership mismatch\n";
        return false;
    }

    world.remove<PlayerControllerIntentComponent>(entity_a);
    const std::vector<karma::ecs::Entity> controller_view_after_remove =
        world.view<RigidBodyIntentComponent, ColliderIntentComponent, PlayerControllerIntentComponent>();
    if (!controller_view_after_remove.empty()) {
        std::cerr << "scene physics component contract: controller view did not update after removal\n";
        return false;
    }

    world.destroyEntity(entity_a);
    if (world.has<RigidBodyIntentComponent>(entity_a)
        || world.has<ColliderIntentComponent>(entity_a)
        || world.has<PlayerControllerIntentComponent>(entity_a)) {
        std::cerr << "scene physics component contract: components persisted after entity destroy\n";
        return false;
    }

    return true;
}

bool RunEcsSyncSystemPolicyChecks(BackendKind backend) {
    using karma::scene::ColliderIntentComponent;
    using karma::scene::ColliderReconcileAction;
    using karma::scene::ColliderShapeKind;
    using karma::scene::ControllerColliderCompatibility;
    using karma::scene::PhysicsTransformAuthority;
    using karma::scene::PhysicsTransformOwnershipComponent;
    using karma::scene::PlayerControllerIntentComponent;
    using karma::scene::RigidBodyIntentComponent;
    using karma::scene::TransformComponent;

    auto make_transform_component = [](const glm::vec3& position) {
        TransformComponent transform{};
        transform.local = glm::mat4(1.0f);
        transform.world = glm::mat4(1.0f);
        transform.local[3] = glm::vec4(position, 1.0f);
        transform.world[3] = glm::vec4(position, 1.0f);
        return transform;
    };
    auto world_position = [](const TransformComponent& transform) {
        return glm::vec3(transform.world[3]);
    };

    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();
    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (ecs-sync check)\n";
        return false;
    }

    karma::physics::EcsSyncSystem sync(physics);
    karma::ecs::World world{};

    const auto entity_create = world.createEntity();
    world.add<TransformComponent>(entity_create, make_transform_component(glm::vec3(0.0f, 3.0f, 0.0f)));
    world.add<RigidBodyIntentComponent>(entity_create, RigidBodyIntentComponent{});
    world.add<ColliderIntentComponent>(entity_create, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_create, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId created_body = karma::physics_backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_create, created_body)
        || created_body == karma::physics_backend::kInvalidBodyId
        || sync.runtimeBindingCount() != 1u) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create runtime body for valid entity\n";
        physics.shutdown();
        return false;
    }

    BodyTransform created_snapshot{};
    if (!sync.tryGetRuntimeTransformSnapshot(entity_create, created_snapshot)
        || !NearlyEqualVec3(created_snapshot.position, glm::vec3(0.0f, 3.0f, 0.0f), 1e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync snapshot mismatch after create\n";
        physics.shutdown();
        return false;
    }

    const ColliderIntentComponent before_rebuild = world.get<ColliderIntentComponent>(entity_create);
    auto* rebuild_collider = world.tryGet<ColliderIntentComponent>(entity_create);
    rebuild_collider->half_extents.x += 0.4f;
    if (karma::scene::ClassifyColliderReconcileAction(before_rebuild, *rebuild_collider)
        != ColliderReconcileAction::RebuildRuntimeShape) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync expected rebuild classification for shape parameter change\n";
        physics.shutdown();
        return false;
    }

    sync.preSimulate(world);
    BodyId rebuilt_body = karma::physics_backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_create, rebuilt_body)
        || rebuilt_body == karma::physics_backend::kInvalidBodyId
        || rebuilt_body == created_body) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not rebuild runtime body after shape change\n";
        physics.shutdown();
        return false;
    }

    auto* invalid_collider = world.tryGet<ColliderIntentComponent>(entity_create);
    invalid_collider->half_extents.x = 0.0f;
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_create) || sync.runtimeBindingCount() != 0u) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not teardown runtime body for invalid intent\n";
        physics.shutdown();
        return false;
    }

    const auto entity_mesh = world.createEntity();
    world.add<TransformComponent>(entity_mesh, make_transform_component(glm::vec3(1.0f, 1.0f, 1.0f)));
    RigidBodyIntentComponent mesh_static_rigidbody{};
    mesh_static_rigidbody.dynamic = false;
    world.add<RigidBodyIntentComponent>(entity_mesh, mesh_static_rigidbody);
    ColliderIntentComponent mesh_collider{};
    mesh_collider.shape = ColliderShapeKind::Mesh;
    mesh_collider.mesh_path = "demo/worlds/phase3-placeholder-static.glb";
    world.add<ColliderIntentComponent>(entity_mesh, mesh_collider);
    world.add<PhysicsTransformOwnershipComponent>(entity_mesh, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_mesh)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create mesh-placeholder runtime body\n";
        physics.shutdown();
        return false;
    }

    auto* mesh_dynamic_flip = world.tryGet<RigidBodyIntentComponent>(entity_mesh);
    mesh_dynamic_flip->dynamic = true;
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_mesh)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not teardown mesh-placeholder runtime body after unsupported dynamic transition\n";
        physics.shutdown();
        return false;
    }

    mesh_dynamic_flip->dynamic = false;
    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_mesh)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not recreate mesh-placeholder runtime body after returning to supported static intent\n";
        physics.shutdown();
        return false;
    }

    const auto entity_enabled_toggle = world.createEntity();
    world.add<TransformComponent>(entity_enabled_toggle, make_transform_component(glm::vec3(-1.0f, 4.0f, 0.0f)));
    world.add<RigidBodyIntentComponent>(entity_enabled_toggle, RigidBodyIntentComponent{});
    world.add<ColliderIntentComponent>(entity_enabled_toggle, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_enabled_toggle, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_enabled_toggle)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create enabled-toggle fixture runtime body\n";
        physics.shutdown();
        return false;
    }

    auto* enabled_toggle_collider = world.tryGet<ColliderIntentComponent>(entity_enabled_toggle);
    enabled_toggle_collider->enabled = false;
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_enabled_toggle)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not teardown runtime body when collider was disabled\n";
        physics.shutdown();
        return false;
    }

    enabled_toggle_collider->enabled = true;
    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_enabled_toggle)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not recreate runtime body when collider was re-enabled\n";
        physics.shutdown();
        return false;
    }

    const auto entity_runtime_properties = world.createEntity();
    world.add<TransformComponent>(entity_runtime_properties, make_transform_component(glm::vec3(-3.0f, 5.0f, 0.0f)));
    world.add<RigidBodyIntentComponent>(entity_runtime_properties, RigidBodyIntentComponent{});
    world.add<ColliderIntentComponent>(entity_runtime_properties, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_runtime_properties, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId runtime_properties_body = karma::physics_backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_runtime_properties, runtime_properties_body)
        || runtime_properties_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create runtime-properties fixture body\n";
        physics.shutdown();
        return false;
    }

    auto* runtime_properties_collider = world.tryGet<ColliderIntentComponent>(entity_runtime_properties);
    runtime_properties_collider->is_trigger = true;
    sync.preSimulate(world);

    BodyId trigger_updated_body = karma::physics_backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_runtime_properties, trigger_updated_body)
        || trigger_updated_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lost runtime-properties fixture after trigger transition\n";
        physics.shutdown();
        return false;
    }

    bool runtime_trigger_enabled = false;
    if (!physics.getBodyTrigger(trigger_updated_body, runtime_trigger_enabled) || !runtime_trigger_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync trigger transition did not update runtime trigger state\n";
        physics.shutdown();
        return false;
    }

    const BodyId pre_filter_update_body = trigger_updated_body;
    const CollisionMask updated_runtime_mask{0x2u, 0x3u};
    runtime_properties_collider->mask.layer = updated_runtime_mask.layer;
    runtime_properties_collider->mask.collides_with = updated_runtime_mask.collides_with;
    sync.preSimulate(world);

    BodyId post_filter_update_body = karma::physics_backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_runtime_properties, post_filter_update_body)
        || post_filter_update_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lost runtime-properties fixture after filter transition\n";
        physics.shutdown();
        return false;
    }

    if (backend == BackendKind::Jolt && post_filter_update_body == pre_filter_update_body) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync expected deterministic rebuild fallback for unsupported runtime filter mutation\n";
        physics.shutdown();
        return false;
    }
    if (backend == BackendKind::PhysX && post_filter_update_body != pre_filter_update_body) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly rebuilt body for supported runtime filter mutation\n";
        physics.shutdown();
        return false;
    }

    CollisionMask observed_runtime_mask{};
    if (!physics.getBodyCollisionMask(post_filter_update_body, observed_runtime_mask)
        || !EqualCollisionMask(observed_runtime_mask, updated_runtime_mask)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync filter transition did not converge to expected runtime mask\n";
        physics.shutdown();
        return false;
    }

    const auto entity_controller = world.createEntity();
    world.add<TransformComponent>(entity_controller, make_transform_component(glm::vec3(3.0f, 6.0f, 0.0f)));
    world.add<RigidBodyIntentComponent>(entity_controller, RigidBodyIntentComponent{});
    world.add<ColliderIntentComponent>(entity_controller, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_controller, PhysicsTransformOwnershipComponent{});
    world.add<PlayerControllerIntentComponent>(entity_controller, PlayerControllerIntentComponent{});

    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_controller) || !sync.hasControllerRuntimeBinding(entity_controller)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not create controller runtime metadata for compatible controller intent\n";
        physics.shutdown();
        return false;
    }
    ControllerColliderCompatibility controller_compatibility = ControllerColliderCompatibility::ColliderMissing;
    if (!sync.tryGetControllerCompatibility(entity_controller, controller_compatibility)
        || controller_compatibility != ControllerColliderCompatibility::Compatible) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync controller compatibility metadata mismatch for compatible fixture\n";
        physics.shutdown();
        return false;
    }

    auto* controller_collider = world.tryGet<ColliderIntentComponent>(entity_controller);
    controller_collider->is_trigger = true;
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_controller) || sync.hasControllerRuntimeBinding(entity_controller)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not teardown runtime/controller metadata on incompatible controller transition\n";
        physics.shutdown();
        return false;
    }

    controller_collider->is_trigger = false;
    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_controller) || !sync.hasControllerRuntimeBinding(entity_controller)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not recreate runtime/controller metadata after returning to compatible controller intent\n";
        physics.shutdown();
        return false;
    }

    auto* controller_intent = world.tryGet<PlayerControllerIntentComponent>(entity_controller);
    controller_intent->enabled = false;
    sync.preSimulate(world);
    if (!sync.hasControllerRuntimeBinding(entity_controller)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly removed controller metadata for controller-disabled compatible state\n";
        physics.shutdown();
        return false;
    }
    if (!sync.tryGetControllerCompatibility(entity_controller, controller_compatibility)
        || controller_compatibility != ControllerColliderCompatibility::CompatibleControllerDisabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync controller-disabled compatibility metadata mismatch\n";
        physics.shutdown();
        return false;
    }

    world.remove<PlayerControllerIntentComponent>(entity_controller);
    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_controller) || sync.hasControllerRuntimeBinding(entity_controller)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync controller metadata lifecycle did not update after controller component removal\n";
        physics.shutdown();
        return false;
    }

    const auto entity_push = world.createEntity();
    world.add<TransformComponent>(entity_push, make_transform_component(glm::vec3(2.0f, 5.0f, 0.0f)));
    world.add<RigidBodyIntentComponent>(entity_push, RigidBodyIntentComponent{});
    world.add<ColliderIntentComponent>(entity_push, ColliderIntentComponent{});
    PhysicsTransformOwnershipComponent scene_authoritative{};
    scene_authoritative.authority = PhysicsTransformAuthority::SceneAuthoritative;
    scene_authoritative.scene_transform_dirty = true;
    scene_authoritative.push_scene_transform_to_physics = true;
    scene_authoritative.pull_physics_transform_to_scene = false;
    world.add<PhysicsTransformOwnershipComponent>(entity_push, scene_authoritative);

    sync.preSimulate(world);
    BodyId push_body = karma::physics_backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_push, push_body)
        || push_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create scene-authoritative runtime body\n";
        physics.shutdown();
        return false;
    }

    auto* push_transform = world.tryGet<TransformComponent>(entity_push);
    push_transform->local[3] = glm::vec4(2.0f, 7.0f, 0.0f, 1.0f);
    push_transform->world[3] = glm::vec4(2.0f, 7.0f, 0.0f, 1.0f);
    auto* push_ownership = world.tryGet<PhysicsTransformOwnershipComponent>(entity_push);
    push_ownership->scene_transform_dirty = true;
    sync.preSimulate(world);

    BodyTransform pushed_runtime{};
    if (!physics.getBodyTransform(push_body, pushed_runtime)
        || !NearlyEqualVec3(pushed_runtime.position, glm::vec3(2.0f, 7.0f, 0.0f), 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync scene-authoritative push did not update runtime transform\n";
        physics.shutdown();
        return false;
    }
    if (push_ownership->scene_transform_dirty) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync scene-authoritative push did not clear dirty flag\n";
        physics.shutdown();
        return false;
    }

    const auto entity_pull = world.createEntity();
    world.add<TransformComponent>(entity_pull, make_transform_component(glm::vec3(4.0f, 10.0f, 0.0f)));
    RigidBodyIntentComponent pull_rigidbody{};
    pull_rigidbody.gravity_enabled = false;
    world.add<RigidBodyIntentComponent>(entity_pull, pull_rigidbody);
    world.add<ColliderIntentComponent>(entity_pull, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_pull, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId pull_body = karma::physics_backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_pull, pull_body)
        || pull_body == karma::physics_backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create physics-authoritative runtime body\n";
        physics.shutdown();
        return false;
    }

    BodyTransform forced_runtime{};
    forced_runtime.position = glm::vec3(4.0f, 3.0f, 0.0f);
    forced_runtime.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (!physics.setBodyTransform(pull_body, forced_runtime)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync could not set runtime transform for pull check\n";
        physics.shutdown();
        return false;
    }
    sync.postSimulate(world);

    const auto* pulled_transform = world.tryGet<TransformComponent>(entity_pull);
    if (!pulled_transform
        || !NearlyEqualVec3(world_position(*pulled_transform), forced_runtime.position, 5e-2f)
        || !NearlyEqualVec3(glm::vec3(pulled_transform->local[3]), forced_runtime.position, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync physics-authoritative pull did not write scene transform\n";
        physics.shutdown();
        return false;
    }

    const auto entity_cleanup_mutate = world.createEntity();
    world.add<TransformComponent>(entity_cleanup_mutate, make_transform_component(glm::vec3(6.0f, 2.0f, 0.0f)));
    world.add<RigidBodyIntentComponent>(entity_cleanup_mutate, RigidBodyIntentComponent{});
    world.add<ColliderIntentComponent>(entity_cleanup_mutate, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_cleanup_mutate, PhysicsTransformOwnershipComponent{});
    world.add<PlayerControllerIntentComponent>(entity_cleanup_mutate, PlayerControllerIntentComponent{});
    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_cleanup_mutate)
        || !sync.hasControllerRuntimeBinding(entity_cleanup_mutate)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync cleanup-mutation fixture did not create runtime/controller metadata\n";
        physics.shutdown();
        return false;
    }

    world.remove<ColliderIntentComponent>(entity_cleanup_mutate);
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_cleanup_mutate)
        || sync.hasControllerRuntimeBinding(entity_cleanup_mutate)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not cleanup stale runtime/controller metadata after required component mutation\n";
        physics.shutdown();
        return false;
    }

    const auto entity_cleanup_destroy = world.createEntity();
    world.add<TransformComponent>(entity_cleanup_destroy, make_transform_component(glm::vec3(7.0f, 2.0f, 0.0f)));
    world.add<RigidBodyIntentComponent>(entity_cleanup_destroy, RigidBodyIntentComponent{});
    world.add<ColliderIntentComponent>(entity_cleanup_destroy, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_cleanup_destroy, PhysicsTransformOwnershipComponent{});
    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_cleanup_destroy)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync cleanup-destroy fixture did not create runtime body\n";
        physics.shutdown();
        return false;
    }

    world.destroyEntity(entity_cleanup_destroy);
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_cleanup_destroy)
        || sync.hasControllerRuntimeBinding(entity_cleanup_destroy)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not cleanup runtime/controller metadata after entity destroy\n";
        physics.shutdown();
        return false;
    }

    sync.clear();
    physics.shutdown();
    return true;
}

bool RunFacadeScaffoldChecks(BackendKind backend) {
    karma::physics::World world;
    world.setBackend(backend);
    world.init();

    if (!world.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend) << " world facade failed to initialize\n";
        return false;
    }
    if (world.selectedBackend() != backend) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade selected unexpected backend: "
                  << BackendKindName(world.selectedBackend()) << "\n";
        world.shutdown();
        return false;
    }

    karma::physics::RigidBody dynamic_body =
        world.createBoxBody(glm::vec3(0.5f, 0.5f, 0.5f), 1.0f, glm::vec3(0.0f, 3.0f, 0.0f));
    if (!dynamic_body.isValid()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade failed to create dynamic rigid body\n";
        world.shutdown();
        return false;
    }

    dynamic_body.setPosition(glm::vec3(0.25f, 3.0f, 0.25f));
    const glm::vec3 moved_position = dynamic_body.getPosition();
    if (!std::isfinite(moved_position.x) || !std::isfinite(moved_position.y) || !std::isfinite(moved_position.z)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade produced non-finite rigid body position\n";
        world.shutdown();
        return false;
    }

    bool gravity_enabled = false;
    if (!dynamic_body.getGravityEnabled(gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade could not query gravity flag for rigid body\n";
        world.shutdown();
        return false;
    }

    karma::physics::StaticBody static_mesh = world.createStaticMesh("demo/worlds/placeholder-static.glb");
    if (!static_mesh.isValid()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade failed to create placeholder static mesh body\n";
        world.shutdown();
        return false;
    }

    karma::physics::PlayerController& controller = world.createPlayer(glm::vec3(0.5f, 0.9f, 0.5f));
    controller.setVelocity(glm::vec3(0.1f, 0.0f, 0.0f));
    controller.update(1.0f / 60.0f);

    if (world.playerController() == nullptr) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade did not retain player controller instance\n";
        world.shutdown();
        return false;
    }

    glm::vec3 hit_point{};
    glm::vec3 hit_normal{};
    const bool has_raycast_hit =
        world.raycast(glm::vec3(0.0f, 8.0f, 0.0f), glm::vec3(0.0f, -8.0f, 0.0f), hit_point, hit_normal);
    if (!has_raycast_hit) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade raycast failed in scaffold check\n";
        world.shutdown();
        return false;
    }
    if (!NearlyEqualVec3(hit_normal, glm::vec3(0.0f, 0.0f, 0.0f), 1e-6f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade returned unexpected non-zero raycast normal during phase-2a contract\n";
        world.shutdown();
        return false;
    }

    controller.destroy();
    dynamic_body.destroy();
    static_mesh.destroy();
    world.shutdown();

    karma::physics::PhysicsSystem borrowed_system{};
    borrowed_system.setBackend(backend);
    borrowed_system.init();
    if (!borrowed_system.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " borrowed system failed to initialize for world shutdown contract check\n";
        return false;
    }

    {
        karma::physics::World borrowed_world(borrowed_system);
        borrowed_world.shutdown();
    }

    if (!borrowed_system.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " borrowed system was shut down by world facade unexpectedly\n";
        return false;
    }
    borrowed_system.shutdown();

    return true;
}

bool ParseBackends(int argc, char** argv, std::vector<BackendKind>& out_backends) {
    for (int i = 1; i < argc; ++i) {
        const std::string_view arg(argv[i]);
        if (arg == "--backend") {
            if (i + 1 >= argc) {
                std::cerr << "missing value for --backend\n";
                return false;
            }
            const std::string_view value(argv[++i]);
            const auto parsed = ParseBackendKind(value);
            if (!parsed || *parsed == BackendKind::Auto) {
                std::cerr << "invalid backend for --backend: " << value << "\n";
                return false;
            }
            out_backends.push_back(*parsed);
            continue;
        }
        if (arg == "--help" || arg == "-h") {
            std::cout << "usage: physics_backend_parity_test [--backend jolt|physx]...\n";
            return false;
        }
        std::cerr << "unknown argument: " << arg << "\n";
        return false;
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::vector<BackendKind> requested_backends{};
    if (!ParseBackends(argc, argv, requested_backends)) {
        return EXIT_FAILURE;
    }

    const std::vector<BackendKind> compiled_backends = CompiledBackends();
    if (compiled_backends.empty()) {
        std::cerr << "no physics backends are compiled\n";
        return EXIT_FAILURE;
    }

    if (!RunBackendSelectionChecks()) {
        return EXIT_FAILURE;
    }
    if (!RunScenePhysicsComponentContractChecks()) {
        return EXIT_FAILURE;
    }

    if (requested_backends.empty()) {
        requested_backends = compiled_backends;
    }

    for (const BackendKind backend : requested_backends) {
        if (!ContainsBackend(compiled_backends, backend)) {
            std::cerr << "requested backend not compiled: " << BackendKindName(backend) << "\n";
            return EXIT_FAILURE;
        }
        std::cout << "running physics backend parity checks for '" << BackendKindName(backend) << "'\n";
        if (!RunLifecycleChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunColliderShapeAndRuntimePropertyChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunUninitializedApiChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunInvalidBodyApiChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBodyGravityFlagChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBodyRotationLockChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBodyTranslationLockChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunReinitCycleChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunVelocityIntegrationChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunRaycastQueryChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunGroundCollisionChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunRestingStabilityChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunFacadeScaffoldChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunEcsSyncSystemPolicyChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunRepeatabilityChecks(backend)) {
            return EXIT_FAILURE;
        }
    }

    std::cout << "physics backend parity checks passed\n";
    return EXIT_SUCCESS;
}
