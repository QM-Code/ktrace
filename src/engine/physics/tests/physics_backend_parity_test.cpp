#include "karma/physics/backend.hpp"
#include "karma/physics/physics_system.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

namespace {

using karma::physics_backend::BackendKind;
using karma::physics_backend::BackendKindName;
using karma::physics_backend::BodyDesc;
using karma::physics_backend::BodyId;
using karma::physics_backend::BodyTransform;
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
        if (!RunUninitializedApiChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunInvalidBodyApiChecks(backend)) {
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
        if (!RunRepeatabilityChecks(backend)) {
            return EXIT_FAILURE;
        }
    }

    std::cout << "physics backend parity checks passed\n";
    return EXIT_SUCCESS;
}
