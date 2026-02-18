#include "karma/app/client/engine.hpp"
#include "karma/app/server/engine.hpp"
#include "karma/common/config/store.hpp"
#include "karma/common/data/path_resolver.hpp"
#include "karma/ecs/world.hpp"
#include "karma/common/logging/logging.hpp"
#include "karma/physics/backend.hpp"
#include "karma/physics/physics_system.hpp"
#include "karma/physics/world.hpp"
#include "karma/scene/components.hpp"
#include "physics/sync/ecs_sync_system.hpp"
#include "physics/sync/engine_fixed_step_sync.hpp"

#include <spdlog/sinks/base_sink.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace karma::physics::detail {
const char* ClassifyRuntimeCommandTraceStageTag(bool has_existing_binding, bool recovering_from_command_failure);
const char* ClassifyRuntimeCommandTraceOperationTag(bool has_linear_force,
                                                    bool has_linear_impulse,
                                                    bool has_angular_torque,
                                                    bool has_angular_impulse);
const char* ClassifyRuntimeCommandTraceOperationPrecedenceTag(bool has_linear_force,
                                                              bool has_linear_impulse,
                                                              bool has_angular_torque,
                                                              bool has_angular_impulse);
const char* ClassifyRuntimeCommandTraceOutcomeTag(bool has_pending_commands,
                                                  bool is_dynamic,
                                                  bool is_kinematic,
                                                  bool stale_runtime_binding_body,
                                                  bool runtime_apply_failed,
                                                  bool recovery_applied);
const char* ClassifyRuntimeCommandTraceFailureCauseTag(bool stale_runtime_binding_body, bool runtime_apply_failed);
const char* ClassifyRuntimeCommandTraceFailureCausePrecedenceTag(bool stale_runtime_binding_body,
                                                                 bool runtime_apply_failed);
const char* ClassifyRuntimeCommandTraceDecisionPathTag(bool has_pending_commands,
                                                       bool is_dynamic,
                                                       bool is_kinematic,
                                                       bool stale_runtime_binding_body,
                                                       bool runtime_apply_failed,
                                                       bool recovery_applied);
}

namespace {

using karma::physics::backend::BackendKind;
using karma::physics::backend::BackendKindName;
using karma::physics::backend::BodyDesc;
using karma::physics::backend::BodyId;
using karma::physics::backend::BodyTransform;
using karma::physics::backend::CollisionMask;
using BackendColliderShapeKind = karma::physics::backend::ColliderShapeKind;
using karma::physics::backend::RaycastHit;
using karma::physics::backend::CompiledBackends;
using karma::physics::backend::ParseBackendKind;

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

std::string ResolveTestAssetPath(std::string_view relative_path) {
    const std::filesystem::path direct_path(relative_path);
    if (std::filesystem::exists(direct_path)) {
        return direct_path.lexically_normal().string();
    }

    // Resolve from source tree when tests run from a build directory.
    std::filesystem::path source_root = std::filesystem::path(__FILE__);
    for (int i = 0; i < 5; ++i) {
        source_root = source_root.parent_path();
    }
    return (source_root / direct_path).lexically_normal().string();
}

class RuntimeTraceCaptureSink final : public spdlog::sinks::base_sink<std::mutex> {
 private:
    struct CapturedTraceEvent {
        std::string logger_name{};
        std::string payload{};
    };

 public:
    void Clear() {
        std::lock_guard<std::mutex> lock(this->mutex_);
        events_.clear();
    }

    bool ContainsEvent(std::string_view logger_name, std::initializer_list<std::string_view> tokens) {
        std::lock_guard<std::mutex> lock(this->mutex_);
        for (const auto& event : events_) {
            if (event.logger_name != logger_name) {
                continue;
            }
            bool all_tokens_present = true;
            for (const auto token : tokens) {
                if (event.payload.find(token) == std::string::npos) {
                    all_tokens_present = false;
                    break;
                }
            }
            if (all_tokens_present) {
                return true;
            }
        }
        return false;
    }

 protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        CapturedTraceEvent event{};
        event.logger_name.assign(msg.logger_name.data(), msg.logger_name.size());
        event.payload.assign(msg.payload.data(), msg.payload.size());
        events_.push_back(std::move(event));
    }

    void flush_() override {}

 private:
    std::vector<CapturedTraceEvent> events_{};
};

class ServerEngineSmokeGame final : public karma::app::server::GameInterface {
 public:
    void onStart() override { ++start_calls; }
    void onTick(float /*dt*/) override { ++tick_calls; }
    void onShutdown() override { ++shutdown_calls; }

    int start_calls = 0;
    int tick_calls = 0;
    int shutdown_calls = 0;
};

class ClientEngineSmokeGame final : public karma::app::client::GameInterface {
 public:
    void onStart() override { ++start_calls; }
    void onUpdate(float /*dt*/) override { ++update_calls; }
    void onShutdown() override { ++shutdown_calls; }

    int start_calls = 0;
    int update_calls = 0;
    int shutdown_calls = 0;
};

class EngineSyncLifecycleCapture final : public karma::physics::detail::EngineSyncLifecycleObserver {
 public:
    void onEngineSyncLifecycleEvent(const karma::physics::detail::EngineSyncLifecycleEvent& event) override {
        events.push_back(event);
    }

    int countPhase(karma::physics::detail::EngineSyncLifecyclePhase phase) const {
        return static_cast<int>(std::count_if(events.begin(), events.end(), [phase](const auto& event) {
            return event.phase == phase;
        }));
    }

    bool hasResetWhilePhysicsInitialized() const {
        return std::any_of(events.begin(), events.end(), [](const auto& event) {
            return event.phase == karma::physics::detail::EngineSyncLifecyclePhase::ResetApplied
                && event.physics_initialized;
        });
    }

    bool hasCreateBeforeReset() const {
        std::optional<size_t> create_index{};
        std::optional<size_t> reset_index{};
        for (size_t i = 0; i < events.size(); ++i) {
            if (!create_index
                && events[i].phase == karma::physics::detail::EngineSyncLifecyclePhase::CreateSucceeded) {
                create_index = i;
            }
            if (!reset_index
                && events[i].phase == karma::physics::detail::EngineSyncLifecyclePhase::ResetApplied) {
                reset_index = i;
            }
        }
        return create_index && reset_index && (*create_index < *reset_index);
    }

    bool hasDeterministicCreateResetPairs(int expected_pairs) const {
        if (expected_pairs < 0) {
            return false;
        }

        int unmatched_creates = 0;
        int paired_resets = 0;
        for (const auto& event : events) {
            if (event.phase == karma::physics::detail::EngineSyncLifecyclePhase::CreateSucceeded) {
                ++unmatched_creates;
                continue;
            }
            if (event.phase == karma::physics::detail::EngineSyncLifecyclePhase::ResetApplied) {
                if (unmatched_creates <= 0) {
                    return false;
                }
                --unmatched_creates;
                ++paired_resets;
            }
        }

        return unmatched_creates == 0 && paired_resets == expected_pairs;
    }

 private:
    std::vector<karma::physics::detail::EngineSyncLifecycleEvent> events{};
};

class ScopedClientStartupPrerequisites final {
 public:
    ScopedClientStartupPrerequisites() {
        using karma::common::config::ConfigFileSpec;
        using karma::common::config::ConfigStore;
        using karma::common::data::DataRoot;
        using karma::common::data::SetDataRootOverride;

        const std::filesystem::path expected_data_root = ResolveTestAssetPath("data");
        std::error_code root_ec;
        if (!std::filesystem::exists(expected_data_root, root_ec) || !std::filesystem::is_directory(expected_data_root, root_ec)) {
            failure_reason_ = "startup data root is missing";
            return;
        }
        const std::filesystem::path expected_shader_path = expected_data_root / "bgfx" / "shaders" / "bin" / "vk" / "mesh" / "vs_mesh.bin";
        if (!std::filesystem::exists(expected_shader_path, root_ec)) {
            failure_reason_ = "bgfx shader prerequisite is missing under startup data root";
            return;
        }

        try {
            SetDataRootOverride(expected_data_root);
        } catch (const std::exception&) {
            try {
                const std::filesystem::path existing_root = DataRoot();
                const std::filesystem::path existing_shader_path =
                    existing_root / "bgfx" / "shaders" / "bin" / "vk" / "mesh" / "vs_mesh.bin";
                const std::filesystem::path existing_client_config_path = existing_root / "client" / "config.json";
                if (!std::filesystem::exists(existing_shader_path, root_ec)
                    || !std::filesystem::exists(existing_client_config_path, root_ec)) {
                    failure_reason_ = "existing data root is initialized without required client startup assets";
                    return;
                }
            } catch (const std::exception& ex) {
                failure_reason_ = std::string("failed to prepare data root override: ") + ex.what();
                return;
            }
        }

        ConfigFileSpec client_config_spec{};
        client_config_spec.path = "client/config.json";
        client_config_spec.label = "data/client/config.json";
        client_config_spec.missingLevel = spdlog::level::err;
        client_config_spec.required = true;
        client_config_spec.resolveRelativeToDataRoot = true;
        ConfigStore::Initialize({client_config_spec}, std::nullopt);

        auto* bindings = ConfigStore::Get("bindings");
        const bool bindings_valid = bindings && bindings->is_object()
            && bindings->contains("global") && (*bindings)["global"].is_object()
            && bindings->contains("game") && (*bindings)["game"].is_object()
            && bindings->contains("roaming") && (*bindings)["roaming"].is_object();
        if (!bindings_valid) {
            auto fallback_bindings = karma::common::serialization::Object();
            fallback_bindings["global"] = karma::common::serialization::Object();
            fallback_bindings["game"] = karma::common::serialization::Object();
            fallback_bindings["roaming"] = karma::common::serialization::Object();
            if (!ConfigStore::Set("bindings", std::move(fallback_bindings))) {
                failure_reason_ = "failed to configure deterministic client bindings";
                return;
            }
        }

        const std::filesystem::path resolved_world_asset = ConfigStore::ResolveAssetPath("assets.models.world", {});
        if (resolved_world_asset.empty() || !std::filesystem::exists(resolved_world_asset, root_ec)) {
            failure_reason_ = "startup world asset key did not resolve to an existing path";
            return;
        }

        configured_ = true;
    }

    bool configured() const { return configured_; }
    const std::string& failureReason() const { return failure_reason_; }

 private:
    bool configured_ = false;
    std::string failure_reason_{};
};

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
    if (static_body == karma::physics::backend::kInvalidBodyId) {
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
    if (dynamic_body == karma::physics::backend::kInvalidBodyId) {
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

    const std::string static_mesh_asset = ResolveTestAssetPath("demo/worlds/r55man-2/world.glb");
    const std::string missing_mesh_asset = ResolveTestAssetPath("demo/worlds/phase4x-missing-static-mesh.glb");

    BodyDesc box_desc{};
    box_desc.is_static = true;
    box_desc.collider_shape.kind = BackendColliderShapeKind::Box;
    box_desc.collider_shape.box_half_extents = glm::vec3(0.75f, 0.4f, 1.2f);
    box_desc.transform.position = glm::vec3(-2.0f, 1.0f, 0.0f);
    const BodyId box_body = physics.createBody(box_desc);
    if (box_body == karma::physics::backend::kInvalidBodyId) {
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
    if (sphere_body == karma::physics::backend::kInvalidBodyId) {
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
    if (capsule_body == karma::physics::backend::kInvalidBodyId) {
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

    BodyDesc mesh_desc{};
    mesh_desc.is_static = true;
    mesh_desc.collider_shape.kind = BackendColliderShapeKind::Mesh;
    mesh_desc.collider_shape.mesh_asset_path = static_mesh_asset;
    mesh_desc.transform.position = glm::vec3(4.0f, 0.0f, 0.0f);
    const BodyId mesh_body = physics.createBody(mesh_desc);
    if (mesh_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create static mesh-shape body from mesh path\n";
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyTransform(mesh_body, observed)
        || !ValidateTransform(observed, mesh_desc.transform, "mesh-shape transform", backend)) {
        physics.destroyBody(mesh_body);
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }

    BodyDesc missing_mesh_desc = mesh_desc;
    missing_mesh_desc.transform.position = glm::vec3(6.0f, 0.0f, 0.0f);
    missing_mesh_desc.collider_shape.mesh_asset_path = missing_mesh_asset;
    if (physics.createBody(missing_mesh_desc) != karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " createBody unexpectedly succeeded for missing static mesh path\n";
        physics.destroyBody(mesh_body);
        physics.destroyBody(capsule_body);
        physics.destroyBody(sphere_body);
        physics.destroyBody(box_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(mesh_body);
    physics.destroyBody(capsule_body);
    physics.destroyBody(sphere_body);
    physics.destroyBody(box_body);
    physics.shutdown();
    return true;
}

bool RunColliderShapeOffsetQueryChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (shape-offset check)\n";
        return false;
    }

    BodyDesc baseline_desc{};
    baseline_desc.is_static = true;
    baseline_desc.collider_shape.kind = BackendColliderShapeKind::Box;
    baseline_desc.collider_shape.box_half_extents = glm::vec3(0.5f, 0.5f, 0.5f);
    baseline_desc.collider_shape.local_center = glm::vec3(0.0f, 0.0f, 0.0f);
    baseline_desc.transform.position = glm::vec3(-2.0f, 0.0f, 0.0f);
    const BodyId baseline_body = physics.createBody(baseline_desc);
    if (baseline_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create baseline box body (shape-offset check)\n";
        physics.shutdown();
        return false;
    }

    BodyDesc offset_desc = baseline_desc;
    offset_desc.transform.position = glm::vec3(2.0f, 0.0f, 0.0f);
    offset_desc.collider_shape.local_center = glm::vec3(0.0f, 2.0f, 0.0f);
    const BodyId offset_body = physics.createBody(offset_desc);
    if (offset_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create offset box body (shape-offset check)\n";
        physics.destroyBody(baseline_body);
        physics.shutdown();
        return false;
    }

    BodyDesc invalid_offset_desc = baseline_desc;
    invalid_offset_desc.transform.position = glm::vec3(6.0f, 0.0f, 0.0f);
    invalid_offset_desc.collider_shape.local_center.x = std::numeric_limits<float>::quiet_NaN();
    if (physics.createBody(invalid_offset_desc) != karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " createBody unexpectedly succeeded with non-finite local offset\n";
        physics.destroyBody(offset_body);
        physics.destroyBody(baseline_body);
        physics.shutdown();
        return false;
    }

    constexpr float ray_max_distance = 10.0f;
    const glm::vec3 ray_direction(0.0f, -1.0f, 0.0f);
    RaycastHit baseline_hit{};
    if (!physics.raycastClosest(glm::vec3(-2.0f, 5.0f, 0.0f), ray_direction, ray_max_distance, baseline_hit)
        || baseline_hit.body != baseline_body) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " shape-offset baseline raycast mismatch\n";
        physics.destroyBody(offset_body);
        physics.destroyBody(baseline_body);
        physics.shutdown();
        return false;
    }

    RaycastHit offset_hit{};
    if (!physics.raycastClosest(glm::vec3(2.0f, 5.0f, 0.0f), ray_direction, ray_max_distance, offset_hit)
        || offset_hit.body != offset_body) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " shape-offset centered raycast mismatch\n";
        physics.destroyBody(offset_body);
        physics.destroyBody(baseline_body);
        physics.shutdown();
        return false;
    }

    if (!(baseline_hit.distance > 4.2f && baseline_hit.distance < 4.8f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " baseline shape-offset distance out of range: distance=" << baseline_hit.distance << "\n";
        physics.destroyBody(offset_body);
        physics.destroyBody(baseline_body);
        physics.shutdown();
        return false;
    }
    if (!(offset_hit.distance > 2.2f && offset_hit.distance < 2.8f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " offset shape distance out of range: distance=" << offset_hit.distance << "\n";
        physics.destroyBody(offset_body);
        physics.destroyBody(baseline_body);
        physics.shutdown();
        return false;
    }
    if (!(offset_hit.distance + 1.0f < baseline_hit.distance)
        || !(offset_hit.position.y > baseline_hit.position.y + 1.5f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " local-center offset did not shift collision query behavior as expected\n";
        physics.destroyBody(offset_body);
        physics.destroyBody(baseline_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(offset_body);
    physics.destroyBody(baseline_body);
    physics.shutdown();
    return true;
}

bool RunBodyVelocityApiChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (velocity-api check)\n";
        return false;
    }

    BodyDesc dynamic_desc{};
    dynamic_desc.is_static = false;
    dynamic_desc.mass = 1.0f;
    dynamic_desc.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    dynamic_desc.linear_velocity = glm::vec3(0.4f, 0.0f, -0.3f);
    dynamic_desc.angular_velocity = glm::vec3(0.0f, 1.1f, 0.0f);
    const BodyId dynamic_body = physics.createBody(dynamic_desc);
    if (dynamic_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create dynamic body (velocity-api check)\n";
        physics.shutdown();
        return false;
    }

    glm::vec3 observed_linear{};
    glm::vec3 observed_angular{};
    if (!physics.getBodyLinearVelocity(dynamic_body, observed_linear)
        || !NearlyEqualVec3(observed_linear, dynamic_desc.linear_velocity, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " linear velocity read mismatch after dynamic create\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyAngularVelocity(dynamic_body, observed_angular)
        || !NearlyEqualVec3(observed_angular, dynamic_desc.angular_velocity, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " angular velocity read mismatch after dynamic create\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    const glm::vec3 updated_linear(1.25f, 0.0f, 0.5f);
    const glm::vec3 updated_angular(0.0f, 0.0f, 2.0f);
    if (!physics.setBodyLinearVelocity(dynamic_body, updated_linear)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyLinearVelocity failed for valid dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!physics.setBodyAngularVelocity(dynamic_body, updated_angular)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAngularVelocity failed for valid dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyLinearVelocity(dynamic_body, observed_linear)
        || !NearlyEqualVec3(observed_linear, updated_linear, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " linear velocity roundtrip mismatch\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyAngularVelocity(dynamic_body, observed_angular)
        || !NearlyEqualVec3(observed_angular, updated_angular, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " angular velocity roundtrip mismatch\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    BodyDesc static_desc{};
    static_desc.is_static = true;
    const BodyId static_body = physics.createBody(static_desc);
    if (static_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create static body (velocity-api check)\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (physics.setBodyLinearVelocity(static_body, glm::vec3(0.1f, 0.0f, 0.0f))
        || physics.getBodyLinearVelocity(static_body, observed_linear)
        || physics.setBodyAngularVelocity(static_body, glm::vec3(0.0f, 0.2f, 0.0f))
        || physics.getBodyAngularVelocity(static_body, observed_angular)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " velocity APIs unexpectedly succeeded on static body\n";
        physics.destroyBody(static_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(static_body);
    physics.destroyBody(dynamic_body);
    if (physics.setBodyLinearVelocity(dynamic_body, glm::vec3(0.0f, 0.0f, 0.0f))
        || physics.getBodyLinearVelocity(dynamic_body, observed_linear)
        || physics.setBodyAngularVelocity(dynamic_body, glm::vec3(0.0f, 0.0f, 0.0f))
        || physics.getBodyAngularVelocity(dynamic_body, observed_angular)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " velocity APIs unexpectedly succeeded after body destroy\n";
        physics.shutdown();
        return false;
    }

    physics.shutdown();
    return true;
}

bool RunBodyForceImpulseApiChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (force-impulse-api check)\n";
        return false;
    }

    BodyDesc dynamic_desc{};
    dynamic_desc.is_static = false;
    dynamic_desc.mass = 2.0f;
    dynamic_desc.gravity_enabled = false;
    dynamic_desc.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    dynamic_desc.linear_velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    dynamic_desc.angular_velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    const BodyId dynamic_body = physics.createBody(dynamic_desc);
    if (dynamic_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create dynamic body (force-impulse-api check)\n";
        physics.shutdown();
        return false;
    }

    glm::vec3 velocity_before_force{};
    if (!physics.getBodyLinearVelocity(dynamic_body, velocity_before_force)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read linear velocity before force application\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!physics.addBodyForce(dynamic_body, glm::vec3(30.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyForce failed for valid dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 60.0f);
    physics.simulateFixedStep(1.0f / 60.0f);
    physics.endFrame();

    glm::vec3 velocity_after_force{};
    if (!physics.getBodyLinearVelocity(dynamic_body, velocity_after_force)
        || !(velocity_after_force.x > velocity_before_force.x + 1e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " force command did not produce expected runtime velocity change\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!physics.addBodyLinearImpulse(dynamic_body, glm::vec3(1.5f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyLinearImpulse failed for valid dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    glm::vec3 velocity_after_impulse_immediate{};
    if (!physics.getBodyLinearVelocity(dynamic_body, velocity_after_impulse_immediate)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read linear velocity after impulse application\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 60.0f);
    physics.simulateFixedStep(1.0f / 60.0f);
    physics.endFrame();

    glm::vec3 velocity_after_impulse_step{};
    if (!physics.getBodyLinearVelocity(dynamic_body, velocity_after_impulse_step)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read linear velocity after impulse simulation step\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!(velocity_after_impulse_immediate.x > velocity_after_force.x + 1e-3f)
        && !(velocity_after_impulse_step.x > velocity_after_force.x + 1e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " impulse command did not produce expected runtime velocity change\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    glm::vec3 angular_before_torque{};
    if (!physics.getBodyAngularVelocity(dynamic_body, angular_before_torque)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read angular velocity before torque application\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!physics.addBodyTorque(dynamic_body, glm::vec3(0.0f, 18.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyTorque failed for valid dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 60.0f);
    physics.simulateFixedStep(1.0f / 60.0f);
    physics.endFrame();

    glm::vec3 angular_after_torque{};
    if (!physics.getBodyAngularVelocity(dynamic_body, angular_after_torque)
        || !(angular_after_torque.y > angular_before_torque.y + 1e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " torque command did not produce expected runtime angular-velocity change\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!physics.addBodyAngularImpulse(dynamic_body, glm::vec3(0.0f, 0.9f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyAngularImpulse failed for valid dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    glm::vec3 angular_after_impulse_immediate{};
    if (!physics.getBodyAngularVelocity(dynamic_body, angular_after_impulse_immediate)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read angular velocity after angular-impulse application\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.beginFrame(1.0f / 60.0f);
    physics.simulateFixedStep(1.0f / 60.0f);
    physics.endFrame();

    glm::vec3 angular_after_impulse_step{};
    if (!physics.getBodyAngularVelocity(dynamic_body, angular_after_impulse_step)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to read angular velocity after angular-impulse simulation step\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!(angular_after_impulse_immediate.y > angular_after_torque.y + 1e-3f)
        && !(angular_after_impulse_step.y > angular_after_torque.y + 1e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " angular impulse command did not produce expected runtime angular-velocity change\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    BodyDesc static_desc{};
    static_desc.is_static = true;
    const BodyId static_body = physics.createBody(static_desc);
    if (static_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create static body (force-impulse-api check)\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (physics.addBodyForce(static_body, glm::vec3(1.0f, 0.0f, 0.0f))
        || physics.addBodyLinearImpulse(static_body, glm::vec3(1.0f, 0.0f, 0.0f))
        || physics.addBodyTorque(static_body, glm::vec3(0.0f, 1.0f, 0.0f))
        || physics.addBodyAngularImpulse(static_body, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " force/impulse/torque APIs unexpectedly succeeded on static body\n";
        physics.destroyBody(static_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    const BodyId unknown_body = dynamic_body + 1000000;
    if (physics.addBodyForce(unknown_body, glm::vec3(1.0f, 0.0f, 0.0f))
        || physics.addBodyLinearImpulse(unknown_body, glm::vec3(1.0f, 0.0f, 0.0f))
        || physics.addBodyTorque(unknown_body, glm::vec3(0.0f, 1.0f, 0.0f))
        || physics.addBodyAngularImpulse(unknown_body, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " force/impulse/torque APIs unexpectedly succeeded for unknown body id\n";
        physics.destroyBody(static_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(static_body);
    physics.destroyBody(dynamic_body);
    if (physics.addBodyForce(dynamic_body, glm::vec3(1.0f, 0.0f, 0.0f))
        || physics.addBodyLinearImpulse(dynamic_body, glm::vec3(1.0f, 0.0f, 0.0f))
        || physics.addBodyTorque(dynamic_body, glm::vec3(0.0f, 1.0f, 0.0f))
        || physics.addBodyAngularImpulse(dynamic_body, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " force/impulse/torque APIs unexpectedly succeeded after body destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyForce(karma::physics::backend::kInvalidBodyId, glm::vec3(1.0f, 0.0f, 0.0f))
        || physics.addBodyLinearImpulse(karma::physics::backend::kInvalidBodyId, glm::vec3(1.0f, 0.0f, 0.0f))
        || physics.addBodyTorque(karma::physics::backend::kInvalidBodyId, glm::vec3(0.0f, 1.0f, 0.0f))
        || physics.addBodyAngularImpulse(karma::physics::backend::kInvalidBodyId, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " force/impulse/torque APIs unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }

    physics.shutdown();
    return true;
}

bool RunBodyDampingApiChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (damping-api check)\n";
        return false;
    }

    BodyDesc dynamic_desc{};
    dynamic_desc.is_static = false;
    dynamic_desc.mass = 1.0f;
    dynamic_desc.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    dynamic_desc.linear_damping = 0.35f;
    dynamic_desc.angular_damping = 0.65f;
    const BodyId dynamic_body = physics.createBody(dynamic_desc);
    if (dynamic_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create dynamic body (damping-api check)\n";
        physics.shutdown();
        return false;
    }

    float observed_linear_damping = 0.0f;
    float observed_angular_damping = 0.0f;
    if (!physics.getBodyLinearDamping(dynamic_body, observed_linear_damping)
        || !NearlyEqual(observed_linear_damping, dynamic_desc.linear_damping, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " linear damping read mismatch after dynamic create\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyAngularDamping(dynamic_body, observed_angular_damping)
        || !NearlyEqual(observed_angular_damping, dynamic_desc.angular_damping, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " angular damping read mismatch after dynamic create\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    const float updated_linear_damping = 0.2f;
    const float updated_angular_damping = 1.1f;
    if (!physics.setBodyLinearDamping(dynamic_body, updated_linear_damping)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyLinearDamping failed for valid dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!physics.setBodyAngularDamping(dynamic_body, updated_angular_damping)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAngularDamping failed for valid dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyLinearDamping(dynamic_body, observed_linear_damping)
        || !NearlyEqual(observed_linear_damping, updated_linear_damping, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " linear damping roundtrip mismatch\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyAngularDamping(dynamic_body, observed_angular_damping)
        || !NearlyEqual(observed_angular_damping, updated_angular_damping, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " angular damping roundtrip mismatch\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (physics.setBodyLinearDamping(dynamic_body, -0.01f)
        || physics.setBodyAngularDamping(dynamic_body, std::numeric_limits<float>::quiet_NaN())) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " damping APIs unexpectedly accepted invalid damping values\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    BodyDesc static_desc{};
    static_desc.is_static = true;
    const BodyId static_body = physics.createBody(static_desc);
    if (static_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create static body (damping-api check)\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (physics.setBodyLinearDamping(static_body, 0.1f)
        || physics.getBodyLinearDamping(static_body, observed_linear_damping)
        || physics.setBodyAngularDamping(static_body, 0.1f)
        || physics.getBodyAngularDamping(static_body, observed_angular_damping)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " damping APIs unexpectedly succeeded on static body\n";
        physics.destroyBody(static_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(static_body);
    physics.destroyBody(dynamic_body);
    if (physics.setBodyLinearDamping(dynamic_body, 0.0f)
        || physics.getBodyLinearDamping(dynamic_body, observed_linear_damping)
        || physics.setBodyAngularDamping(dynamic_body, 0.0f)
        || physics.getBodyAngularDamping(dynamic_body, observed_angular_damping)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " damping APIs unexpectedly succeeded after body destroy\n";
        physics.shutdown();
        return false;
    }

    if (physics.setBodyLinearDamping(karma::physics::backend::kInvalidBodyId, 0.0f)
        || physics.getBodyLinearDamping(karma::physics::backend::kInvalidBodyId, observed_linear_damping)
        || physics.setBodyAngularDamping(karma::physics::backend::kInvalidBodyId, 0.0f)
        || physics.getBodyAngularDamping(karma::physics::backend::kInvalidBodyId, observed_angular_damping)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " damping APIs unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }

    BodyDesc invalid_linear_desc{};
    invalid_linear_desc.is_static = false;
    invalid_linear_desc.mass = 1.0f;
    invalid_linear_desc.linear_damping = -0.2f;
    if (physics.createBody(invalid_linear_desc) != karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " createBody unexpectedly accepted negative linear damping\n";
        physics.shutdown();
        return false;
    }
    BodyDesc invalid_angular_desc = invalid_linear_desc;
    invalid_angular_desc.linear_damping = 0.1f;
    invalid_angular_desc.angular_damping = std::numeric_limits<float>::quiet_NaN();
    if (physics.createBody(invalid_angular_desc) != karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " createBody unexpectedly accepted non-finite angular damping\n";
        physics.shutdown();
        return false;
    }

    physics.shutdown();
    return true;
}

bool RunBodyMaterialApiChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (material-api check)\n";
        return false;
    }

    BodyDesc dynamic_desc{};
    dynamic_desc.is_static = false;
    dynamic_desc.mass = 1.0f;
    dynamic_desc.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    dynamic_desc.friction = 0.25f;
    dynamic_desc.restitution = 0.4f;
    const BodyId dynamic_body = physics.createBody(dynamic_desc);
    if (dynamic_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create dynamic body (material-api check)\n";
        physics.shutdown();
        return false;
    }

    float observed_friction = 0.0f;
    float observed_restitution = 0.0f;
    if (!physics.getBodyFriction(dynamic_body, observed_friction)
        || !physics.getBodyRestitution(dynamic_body, observed_restitution)
        || !NearlyEqual(observed_friction, dynamic_desc.friction, 5e-3f)
        || !NearlyEqual(observed_restitution, dynamic_desc.restitution, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " material read mismatch after dynamic create\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    const float updated_friction = 0.85f;
    if (!physics.setBodyFriction(dynamic_body, updated_friction)
        || !physics.getBodyFriction(dynamic_body, observed_friction)
        || !NearlyEqual(observed_friction, updated_friction, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " friction runtime mutation roundtrip mismatch\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    const float updated_restitution = 0.75f;
    const bool set_restitution_result = physics.setBodyRestitution(dynamic_body, updated_restitution);
    if (!physics.getBodyRestitution(dynamic_body, observed_restitution)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRestitution failed after runtime mutation attempt\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (set_restitution_result && !NearlyEqual(observed_restitution, updated_restitution, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " restitution runtime mutation roundtrip mismatch on successful update\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!set_restitution_result && !NearlyEqual(observed_restitution, dynamic_desc.restitution, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " restitution changed despite runtime mutation rejection\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (physics.setBodyFriction(dynamic_body, -0.01f)
        || physics.setBodyFriction(dynamic_body, std::numeric_limits<float>::quiet_NaN())
        || physics.setBodyRestitution(dynamic_body, -0.01f)
        || physics.setBodyRestitution(dynamic_body, 1.2f)
        || physics.setBodyRestitution(dynamic_body, std::numeric_limits<float>::quiet_NaN())) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " material APIs unexpectedly accepted invalid inputs\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    BodyDesc static_desc{};
    static_desc.is_static = true;
    static_desc.friction = 0.3f;
    static_desc.restitution = 0.2f;
    const BodyId static_body = physics.createBody(static_desc);
    if (static_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create static body (material-api check)\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!physics.getBodyFriction(static_body, observed_friction)
        || !physics.getBodyRestitution(static_body, observed_restitution)
        || !NearlyEqual(observed_friction, static_desc.friction, 5e-3f)
        || !NearlyEqual(observed_restitution, static_desc.restitution, 5e-3f)
        || !physics.setBodyFriction(static_body, 0.45f)
        || !physics.getBodyFriction(static_body, observed_friction)
        || !NearlyEqual(observed_friction, 0.45f, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " static-body friction material API mismatch\n";
        physics.destroyBody(static_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(static_body);
    physics.destroyBody(dynamic_body);
    if (physics.setBodyFriction(dynamic_body, 0.2f)
        || physics.getBodyFriction(dynamic_body, observed_friction)
        || physics.setBodyRestitution(dynamic_body, 0.1f)
        || physics.getBodyRestitution(dynamic_body, observed_restitution)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " material APIs unexpectedly succeeded after body destroy\n";
        physics.shutdown();
        return false;
    }

    if (physics.setBodyFriction(karma::physics::backend::kInvalidBodyId, 0.2f)
        || physics.getBodyFriction(karma::physics::backend::kInvalidBodyId, observed_friction)
        || physics.setBodyRestitution(karma::physics::backend::kInvalidBodyId, 0.1f)
        || physics.getBodyRestitution(karma::physics::backend::kInvalidBodyId, observed_restitution)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " material APIs unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }

    BodyDesc invalid_friction_desc{};
    invalid_friction_desc.is_static = true;
    invalid_friction_desc.friction = -0.1f;
    if (physics.createBody(invalid_friction_desc) != karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " createBody unexpectedly accepted invalid friction\n";
        physics.shutdown();
        return false;
    }
    BodyDesc invalid_restitution_desc{};
    invalid_restitution_desc.is_static = true;
    invalid_restitution_desc.restitution = 1.1f;
    if (physics.createBody(invalid_restitution_desc) != karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " createBody unexpectedly accepted out-of-range restitution\n";
        physics.shutdown();
        return false;
    }

    physics.shutdown();
    return true;
}

bool RunBodyKinematicApiChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (kinematic-api check)\n";
        return false;
    }

    BodyDesc dynamic_desc{};
    dynamic_desc.is_static = false;
    dynamic_desc.mass = 1.0f;
    dynamic_desc.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    dynamic_desc.is_kinematic = false;
    const BodyId dynamic_body = physics.createBody(dynamic_desc);
    if (dynamic_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create dynamic body (kinematic-api check)\n";
        physics.shutdown();
        return false;
    }

    bool observed_kinematic = true;
    if (!physics.getBodyKinematic(dynamic_body, observed_kinematic) || observed_kinematic) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " unexpected default kinematic state for dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!physics.setBodyKinematic(dynamic_body, true)
        || !physics.getBodyKinematic(dynamic_body, observed_kinematic)
        || !observed_kinematic) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to enable/query runtime kinematic state on dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!physics.setBodyKinematic(dynamic_body, false)
        || !physics.getBodyKinematic(dynamic_body, observed_kinematic)
        || observed_kinematic) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to disable/query runtime kinematic state on dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    BodyDesc create_kinematic_desc = dynamic_desc;
    create_kinematic_desc.is_kinematic = true;
    const BodyId create_kinematic_body = physics.createBody(create_kinematic_desc);
    if (create_kinematic_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create kinematic dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyKinematic(create_kinematic_body, observed_kinematic) || !observed_kinematic) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " create-time kinematic state mismatch for dynamic body\n";
        physics.destroyBody(create_kinematic_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    BodyDesc static_desc{};
    static_desc.is_static = true;
    const BodyId static_body = physics.createBody(static_desc);
    if (static_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create static body (kinematic-api check)\n";
        physics.destroyBody(create_kinematic_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (physics.setBodyKinematic(static_body, true) || physics.getBodyKinematic(static_body, observed_kinematic)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " kinematic APIs unexpectedly succeeded on static body\n";
        physics.destroyBody(static_body);
        physics.destroyBody(create_kinematic_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    BodyDesc invalid_static_kinematic_desc = static_desc;
    invalid_static_kinematic_desc.is_kinematic = true;
    if (physics.createBody(invalid_static_kinematic_desc) != karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " createBody unexpectedly accepted static body with kinematic enabled\n";
        physics.destroyBody(static_body);
        physics.destroyBody(create_kinematic_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(static_body);
    physics.destroyBody(create_kinematic_body);
    physics.destroyBody(dynamic_body);
    if (physics.setBodyKinematic(dynamic_body, true) || physics.getBodyKinematic(dynamic_body, observed_kinematic)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " kinematic APIs unexpectedly succeeded after body destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyKinematic(karma::physics::backend::kInvalidBodyId, true)
        || physics.getBodyKinematic(karma::physics::backend::kInvalidBodyId, observed_kinematic)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " kinematic APIs unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }

    physics.shutdown();
    return true;
}

bool RunBodyAwakeApiChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (awake-api check)\n";
        return false;
    }

    BodyDesc dynamic_desc{};
    dynamic_desc.is_static = false;
    dynamic_desc.mass = 1.0f;
    dynamic_desc.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    dynamic_desc.awake = false;
    const BodyId dynamic_body = physics.createBody(dynamic_desc);
    if (dynamic_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create dynamic body (awake-api check)\n";
        physics.shutdown();
        return false;
    }

    bool observed_awake = true;
    if (!physics.getBodyAwake(dynamic_body, observed_awake) || observed_awake) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " create-time awake state mismatch for dynamic body\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!physics.setBodyAwake(dynamic_body, true)
        || !physics.getBodyAwake(dynamic_body, observed_awake)
        || !observed_awake) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to wake/query dynamic body runtime awake state\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (!physics.setBodyAwake(dynamic_body, false)
        || !physics.getBodyAwake(dynamic_body, observed_awake)
        || observed_awake) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to sleep/query dynamic body runtime awake state\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    BodyDesc static_desc{};
    static_desc.is_static = true;
    const BodyId static_body = physics.createBody(static_desc);
    if (static_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create static body (awake-api check)\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (physics.setBodyAwake(static_body, true) || physics.getBodyAwake(static_body, observed_awake)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " awake APIs unexpectedly succeeded on static body\n";
        physics.destroyBody(static_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(static_body);
    physics.destroyBody(dynamic_body);
    if (physics.setBodyAwake(dynamic_body, true) || physics.getBodyAwake(dynamic_body, observed_awake)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " awake APIs unexpectedly succeeded after body destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyAwake(karma::physics::backend::kInvalidBodyId, true)
        || physics.getBodyAwake(karma::physics::backend::kInvalidBodyId, observed_awake)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " awake APIs unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }

    physics.shutdown();
    return true;
}

bool RunBodyMotionLockApiChecks(BackendKind backend) {
    karma::physics::PhysicsSystem physics;
    physics.setBackend(backend);
    physics.init();

    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (motion-lock-api check)\n";
        return false;
    }

    BodyDesc dynamic_desc{};
    dynamic_desc.is_static = false;
    dynamic_desc.mass = 1.0f;
    dynamic_desc.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    dynamic_desc.rotation_locked = false;
    dynamic_desc.translation_locked = false;
    const BodyId dynamic_body = physics.createBody(dynamic_desc);
    if (dynamic_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create dynamic body (motion-lock-api check)\n";
        physics.shutdown();
        return false;
    }

    bool rotation_locked = false;
    bool translation_locked = false;
    if (!physics.getBodyRotationLocked(dynamic_body, rotation_locked)
        || !physics.getBodyTranslationLocked(dynamic_body, translation_locked)
        || rotation_locked
        || translation_locked) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " motion-lock default state mismatch after dynamic create\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    const bool set_rotation_locked = physics.setBodyRotationLocked(dynamic_body, true);
    if (backend == BackendKind::PhysX) {
        if (!set_rotation_locked) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " motion-lock rotation runtime mutation unexpectedly failed\n";
            physics.destroyBody(dynamic_body);
            physics.shutdown();
            return false;
        }
        if (!physics.getBodyRotationLocked(dynamic_body, rotation_locked)
            || !physics.getBodyTranslationLocked(dynamic_body, translation_locked)
            || !rotation_locked
            || translation_locked) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " motion-lock rotation runtime mutation roundtrip mismatch\n";
            physics.destroyBody(dynamic_body);
            physics.shutdown();
            return false;
        }
    } else {
        if (set_rotation_locked) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " motion-lock rotation mutation unexpectedly succeeded in unsupported backend\n";
            physics.destroyBody(dynamic_body);
            physics.shutdown();
            return false;
        }
        if (!physics.getBodyRotationLocked(dynamic_body, rotation_locked)
            || !physics.getBodyTranslationLocked(dynamic_body, translation_locked)
            || rotation_locked
            || translation_locked) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " motion-lock state changed despite unsupported rotation mutation\n";
            physics.destroyBody(dynamic_body);
            physics.shutdown();
            return false;
        }
    }

    const bool set_translation_with_rotation = physics.setBodyTranslationLocked(dynamic_body, true);
    if (set_translation_with_rotation) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " motion-lock mutation unexpectedly allowed dynamic body with both locks enabled\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyRotationLocked(dynamic_body, rotation_locked)
        || !physics.getBodyTranslationLocked(dynamic_body, translation_locked)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " motion-lock query failed after conflicting lock mutation attempt\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (backend == BackendKind::PhysX && (!rotation_locked || translation_locked)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " motion-lock state mismatch after conflicting lock rejection\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }
    if (backend != BackendKind::PhysX && (rotation_locked || translation_locked)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " motion-lock state changed in unsupported backend after conflicting lock attempt\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (backend == BackendKind::PhysX) {
        if (!physics.setBodyRotationLocked(dynamic_body, false)
            || !physics.setBodyTranslationLocked(dynamic_body, true)
            || !physics.getBodyRotationLocked(dynamic_body, rotation_locked)
            || !physics.getBodyTranslationLocked(dynamic_body, translation_locked)
            || rotation_locked
            || !translation_locked
            || !physics.setBodyTranslationLocked(dynamic_body, false)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " motion-lock translation runtime mutation roundtrip mismatch\n";
            physics.destroyBody(dynamic_body);
            physics.shutdown();
            return false;
        }
    }

    BodyDesc invalid_both_locks_desc = dynamic_desc;
    invalid_both_locks_desc.rotation_locked = true;
    invalid_both_locks_desc.translation_locked = true;
    if (physics.createBody(invalid_both_locks_desc) != karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " createBody unexpectedly accepted dynamic body with both locks enabled\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    BodyDesc static_desc{};
    static_desc.is_static = true;
    const BodyId static_body = physics.createBody(static_desc);
    if (static_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create static body (motion-lock-api check)\n";
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    if (physics.setBodyRotationLocked(static_body, true)
        || physics.getBodyRotationLocked(static_body, rotation_locked)
        || physics.setBodyTranslationLocked(static_body, true)
        || physics.getBodyTranslationLocked(static_body, translation_locked)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " motion-lock APIs unexpectedly succeeded on static body\n";
        physics.destroyBody(static_body);
        physics.destroyBody(dynamic_body);
        physics.shutdown();
        return false;
    }

    physics.destroyBody(static_body);
    physics.destroyBody(dynamic_body);
    if (physics.setBodyRotationLocked(dynamic_body, false)
        || physics.getBodyRotationLocked(dynamic_body, rotation_locked)
        || physics.setBodyTranslationLocked(dynamic_body, false)
        || physics.getBodyTranslationLocked(dynamic_body, translation_locked)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " motion-lock APIs unexpectedly succeeded after body destroy\n";
        physics.shutdown();
        return false;
    }

    if (physics.setBodyRotationLocked(karma::physics::backend::kInvalidBodyId, false)
        || physics.getBodyRotationLocked(karma::physics::backend::kInvalidBodyId, rotation_locked)
        || physics.setBodyTranslationLocked(karma::physics::backend::kInvalidBodyId, false)
        || physics.getBodyTranslationLocked(karma::physics::backend::kInvalidBodyId, translation_locked)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " motion-lock APIs unexpectedly succeeded for invalid id\n";
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
            if (tile == karma::physics::backend::kInvalidBodyId) {
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
    if (dynamic_body == karma::physics::backend::kInvalidBodyId) {
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
    if (dynamic_body == karma::physics::backend::kInvalidBodyId) {
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

    if (physics.getBodyTransform(karma::physics::backend::kInvalidBodyId, out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTransform unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTransform(karma::physics::backend::kInvalidBodyId, probe)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTransform unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    bool gravity_enabled = false;
    if (physics.getBodyGravityEnabled(karma::physics::backend::kInvalidBodyId, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyGravityEnabled unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyGravityEnabled(karma::physics::backend::kInvalidBodyId, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyGravityEnabled unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyKinematic(karma::physics::backend::kInvalidBodyId, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyKinematic unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyKinematic(karma::physics::backend::kInvalidBodyId, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyKinematic unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyAwake(karma::physics::backend::kInvalidBodyId, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAwake unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyAwake(karma::physics::backend::kInvalidBodyId, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAwake unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyForce(karma::physics::backend::kInvalidBodyId, glm::vec3(1.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyForce unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyLinearImpulse(karma::physics::backend::kInvalidBodyId, glm::vec3(1.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyLinearImpulse unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyTorque(karma::physics::backend::kInvalidBodyId, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyTorque unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyAngularImpulse(karma::physics::backend::kInvalidBodyId, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyAngularImpulse unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    glm::vec3 velocity_out{};
    if (physics.setBodyLinearVelocity(karma::physics::backend::kInvalidBodyId, glm::vec3(0.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyLinearVelocity unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyLinearVelocity(karma::physics::backend::kInvalidBodyId, velocity_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyLinearVelocity unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyAngularVelocity(karma::physics::backend::kInvalidBodyId, glm::vec3(0.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAngularVelocity unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyAngularVelocity(karma::physics::backend::kInvalidBodyId, velocity_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAngularVelocity unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    float damping_out = 0.0f;
    if (physics.setBodyLinearDamping(karma::physics::backend::kInvalidBodyId, 0.0f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyLinearDamping unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyLinearDamping(karma::physics::backend::kInvalidBodyId, damping_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyLinearDamping unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyAngularDamping(karma::physics::backend::kInvalidBodyId, 0.0f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAngularDamping unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyAngularDamping(karma::physics::backend::kInvalidBodyId, damping_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAngularDamping unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    bool lock_out = false;
    if (physics.setBodyRotationLocked(karma::physics::backend::kInvalidBodyId, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyRotationLocked unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyRotationLocked(karma::physics::backend::kInvalidBodyId, lock_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRotationLocked unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTranslationLocked(karma::physics::backend::kInvalidBodyId, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTranslationLocked unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyTranslationLocked(karma::physics::backend::kInvalidBodyId, lock_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTranslationLocked unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTrigger(karma::physics::backend::kInvalidBodyId, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTrigger unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyTrigger(karma::physics::backend::kInvalidBodyId, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTrigger unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    CollisionMask mask_out{};
    if (physics.setBodyCollisionMask(karma::physics::backend::kInvalidBodyId, CollisionMask{})) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyCollisionMask unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyCollisionMask(karma::physics::backend::kInvalidBodyId, mask_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyCollisionMask unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    float material_out = 0.0f;
    if (physics.setBodyFriction(karma::physics::backend::kInvalidBodyId, 0.5f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyFriction unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyFriction(karma::physics::backend::kInvalidBodyId, material_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyFriction unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyRestitution(karma::physics::backend::kInvalidBodyId, 0.2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyRestitution unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyRestitution(karma::physics::backend::kInvalidBodyId, material_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRestitution unexpectedly succeeded for invalid id\n";
        physics.shutdown();
        return false;
    }
    physics.destroyBody(karma::physics::backend::kInvalidBodyId);

    BodyDesc desc{};
    desc.is_static = true;
    desc.transform.position = glm::vec3(1.0f, 2.0f, 3.0f);
    desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const BodyId body = physics.createBody(desc);
    if (body == karma::physics::backend::kInvalidBodyId) {
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
    if (physics.getBodyKinematic(bogus, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyKinematic unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyKinematic(bogus, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyKinematic unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyAwake(bogus, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAwake unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyAwake(bogus, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAwake unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyForce(bogus, glm::vec3(1.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyForce unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyLinearImpulse(bogus, glm::vec3(1.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyLinearImpulse unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyTorque(bogus, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyTorque unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyAngularImpulse(bogus, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyAngularImpulse unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyLinearVelocity(bogus, glm::vec3(0.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyLinearVelocity unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyLinearVelocity(bogus, velocity_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyLinearVelocity unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyAngularVelocity(bogus, glm::vec3(0.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAngularVelocity unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyAngularVelocity(bogus, velocity_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAngularVelocity unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyLinearDamping(bogus, 0.0f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyLinearDamping unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyLinearDamping(bogus, damping_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyLinearDamping unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyAngularDamping(bogus, 0.0f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAngularDamping unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyAngularDamping(bogus, damping_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAngularDamping unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyRotationLocked(bogus, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyRotationLocked unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyRotationLocked(bogus, lock_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRotationLocked unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTranslationLocked(bogus, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTranslationLocked unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyTranslationLocked(bogus, lock_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTranslationLocked unexpectedly succeeded for unknown id\n";
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
    if (physics.setBodyFriction(bogus, 0.6f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyFriction unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyFriction(bogus, material_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyFriction unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyRestitution(bogus, 0.4f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyRestitution unexpectedly succeeded for unknown id\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyRestitution(bogus, material_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRestitution unexpectedly succeeded for unknown id\n";
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
    if (physics.getBodyKinematic(body, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyKinematic unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyKinematic(body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyKinematic unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyAwake(body, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAwake unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyAwake(body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAwake unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyForce(body, glm::vec3(1.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyForce unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyLinearImpulse(body, glm::vec3(1.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyLinearImpulse unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyTorque(body, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyTorque unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyAngularImpulse(body, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyAngularImpulse unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyLinearVelocity(body, glm::vec3(0.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyLinearVelocity unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyLinearVelocity(body, velocity_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyLinearVelocity unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyAngularVelocity(body, glm::vec3(0.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAngularVelocity unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyAngularVelocity(body, velocity_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAngularVelocity unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyLinearDamping(body, 0.0f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyLinearDamping unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyLinearDamping(body, damping_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyLinearDamping unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyAngularDamping(body, 0.0f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAngularDamping unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyAngularDamping(body, damping_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAngularDamping unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyRotationLocked(body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyRotationLocked unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyRotationLocked(body, lock_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRotationLocked unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyTranslationLocked(body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTranslationLocked unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyTranslationLocked(body, lock_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTranslationLocked unexpectedly succeeded after destroy\n";
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
    if (physics.setBodyFriction(body, 0.2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyFriction unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyFriction(body, material_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyFriction unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.setBodyRestitution(body, 0.2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyRestitution unexpectedly succeeded after destroy\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyRestitution(body, material_out)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRestitution unexpectedly succeeded after destroy\n";
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
    if (static_body == karma::physics::backend::kInvalidBodyId) {
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
    if (no_gravity_body == karma::physics::backend::kInvalidBodyId) {
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
    if (runtime_toggle_body == karma::physics::backend::kInvalidBodyId) {
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
    if (unlocked_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create unlocked body (rotation-lock check)\n";
        physics.shutdown();
        return false;
    }

    BodyDesc locked_desc = unlocked_desc;
    locked_desc.rotation_locked = true;
    locked_desc.transform.position = glm::vec3(2.0f, 5.0f, 0.0f);
    const BodyId locked_body = physics.createBody(locked_desc);
    if (locked_body == karma::physics::backend::kInvalidBodyId) {
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
    if (unlocked_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create unlocked body (translation-lock check)\n";
        physics.shutdown();
        return false;
    }

    BodyDesc locked_desc = unlocked_desc;
    locked_desc.translation_locked = true;
    locked_desc.transform.position = glm::vec3(8.0f, 5.0f, 0.0f);
    const BodyId locked_body = physics.createBody(locked_desc);
    if (locked_body == karma::physics::backend::kInvalidBodyId) {
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
        if (body == karma::physics::backend::kInvalidBodyId) {
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
    if (ground == karma::physics::backend::kInvalidBodyId) {
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
    if (body == karma::physics::backend::kInvalidBodyId) {
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
    if (lower_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend) << " failed to create lower static body (raycast check)\n";
        physics.shutdown();
        return false;
    }

    BodyDesc upper_desc{};
    upper_desc.is_static = true;
    upper_desc.transform.position = glm::vec3(0.0f, 3.0f, 0.0f);
    upper_desc.transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const BodyId upper_body = physics.createBody(upper_desc);
    if (upper_body == karma::physics::backend::kInvalidBodyId) {
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
    bool lock_enabled = false;
    float material_value = 0.0f;

    const BodyId before_init = physics.createBody(desc);
    if (before_init != karma::physics::backend::kInvalidBodyId) {
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
    if (physics.getBodyKinematic(1, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyKinematic succeeded before init\n";
        return false;
    }
    if (physics.setBodyKinematic(1, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyKinematic succeeded before init\n";
        return false;
    }
    if (physics.getBodyAwake(1, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAwake succeeded before init\n";
        return false;
    }
    if (physics.setBodyAwake(1, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAwake succeeded before init\n";
        return false;
    }
    if (physics.addBodyForce(1, glm::vec3(1.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyForce succeeded before init\n";
        return false;
    }
    if (physics.addBodyLinearImpulse(1, glm::vec3(1.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyLinearImpulse succeeded before init\n";
        return false;
    }
    if (physics.addBodyTorque(1, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyTorque succeeded before init\n";
        return false;
    }
    if (physics.addBodyAngularImpulse(1, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyAngularImpulse succeeded before init\n";
        return false;
    }
    if (physics.getBodyRotationLocked(1, lock_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRotationLocked succeeded before init\n";
        return false;
    }
    if (physics.setBodyRotationLocked(1, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyRotationLocked succeeded before init\n";
        return false;
    }
    if (physics.getBodyTranslationLocked(1, lock_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTranslationLocked succeeded before init\n";
        return false;
    }
    if (physics.setBodyTranslationLocked(1, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTranslationLocked succeeded before init\n";
        return false;
    }
    if (physics.getBodyFriction(1, material_value)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyFriction succeeded before init\n";
        return false;
    }
    if (physics.setBodyFriction(1, 0.5f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyFriction succeeded before init\n";
        return false;
    }
    if (physics.getBodyRestitution(1, material_value)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRestitution succeeded before init\n";
        return false;
    }
    if (physics.setBodyRestitution(1, 0.2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyRestitution succeeded before init\n";
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
    if (body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to create body after init in uninitialized-api check\n";
        physics.shutdown();
        return false;
    }
    physics.destroyBody(body);
    physics.shutdown();

    const BodyId after_shutdown = physics.createBody(desc);
    if (after_shutdown != karma::physics::backend::kInvalidBodyId) {
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
    if (physics.getBodyKinematic(body, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyKinematic succeeded after shutdown\n";
        return false;
    }
    if (physics.setBodyKinematic(body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyKinematic succeeded after shutdown\n";
        return false;
    }
    if (physics.getBodyAwake(body, gravity_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyAwake succeeded after shutdown\n";
        return false;
    }
    if (physics.setBodyAwake(body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyAwake succeeded after shutdown\n";
        return false;
    }
    if (physics.addBodyForce(body, glm::vec3(1.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyForce succeeded after shutdown\n";
        return false;
    }
    if (physics.addBodyLinearImpulse(body, glm::vec3(1.0f, 0.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyLinearImpulse succeeded after shutdown\n";
        return false;
    }
    if (physics.addBodyTorque(body, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyTorque succeeded after shutdown\n";
        return false;
    }
    if (physics.addBodyAngularImpulse(body, glm::vec3(0.0f, 1.0f, 0.0f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " addBodyAngularImpulse succeeded after shutdown\n";
        return false;
    }
    if (physics.getBodyRotationLocked(body, lock_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRotationLocked succeeded after shutdown\n";
        return false;
    }
    if (physics.setBodyRotationLocked(body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyRotationLocked succeeded after shutdown\n";
        return false;
    }
    if (physics.getBodyTranslationLocked(body, lock_enabled)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyTranslationLocked succeeded after shutdown\n";
        return false;
    }
    if (physics.setBodyTranslationLocked(body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyTranslationLocked succeeded after shutdown\n";
        return false;
    }
    if (physics.getBodyFriction(body, material_value)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyFriction succeeded after shutdown\n";
        return false;
    }
    if (physics.setBodyFriction(body, 0.4f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyFriction succeeded after shutdown\n";
        return false;
    }
    if (physics.getBodyRestitution(body, material_value)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " getBodyRestitution succeeded after shutdown\n";
        return false;
    }
    if (physics.setBodyRestitution(body, 0.1f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " setBodyRestitution succeeded after shutdown\n";
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
        if (ground == karma::physics::backend::kInvalidBodyId) {
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
        if (body == karma::physics::backend::kInvalidBodyId) {
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

bool RunRuntimeCommandTraceClassificationChecks() {
    using karma::physics::detail::ClassifyRuntimeCommandTraceStageTag;
    using karma::physics::detail::ClassifyRuntimeCommandTraceOperationTag;
    using karma::physics::detail::ClassifyRuntimeCommandTraceOperationPrecedenceTag;
    using karma::physics::detail::ClassifyRuntimeCommandTraceOutcomeTag;
    using karma::physics::detail::ClassifyRuntimeCommandTraceFailureCauseTag;
    using karma::physics::detail::ClassifyRuntimeCommandTraceFailureCausePrecedenceTag;
    using karma::physics::detail::ClassifyRuntimeCommandTraceDecisionPathTag;

    auto expect_tag = [](std::string_view actual, std::string_view expected, std::string_view label) -> bool {
        if (actual != expected) {
            std::cerr << "runtime command trace classification mismatch for " << label << ": expected '" << expected
                      << "' got '" << actual << "'\n";
            return false;
        }
        return true;
    };

    if (!expect_tag(ClassifyRuntimeCommandTraceStageTag(false, false), "create", "stage-create")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceStageTag(true, false), "update", "stage-update")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceStageTag(false, true), "recovery", "stage-recovery-create-path")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceStageTag(true, true), "recovery", "stage-recovery-priority")) {
        return false;
    }

    if (!expect_tag(ClassifyRuntimeCommandTraceOperationTag(true, false, false, false),
                    "linear_force",
                    "operation-linear-force")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationTag(false, true, false, false),
                    "linear_impulse",
                    "operation-linear-impulse")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationTag(false, false, true, false),
                    "angular_torque",
                    "operation-angular-torque")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationTag(false, false, false, true),
                    "angular_impulse",
                    "operation-angular-impulse")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationTag(true, true, true, true),
                    "linear_force",
                    "operation-deterministic-priority")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationTag(false, false, false, false),
                    "none",
                    "operation-none")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationPrecedenceTag(true, false, false, false),
                    "none",
                    "operation-precedence-single")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationPrecedenceTag(true, true, true, true),
                    "linear_force_first",
                    "operation-precedence-force-first")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationPrecedenceTag(false, true, true, true),
                    "linear_impulse_second",
                    "operation-precedence-impulse-second")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationPrecedenceTag(false, false, true, true),
                    "angular_torque_third",
                    "operation-precedence-torque-third")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationPrecedenceTag(false, false, false, false),
                    "none",
                    "operation-precedence-none")) {
        return false;
    }

    if (!expect_tag(ClassifyRuntimeCommandTraceOutcomeTag(true, true, false, true, false, false),
                    "stale_runtime_binding_body",
                    "stale-runtime")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOutcomeTag(true, false, false, false, false, false),
                    "ineligible_non_dynamic",
                    "ineligible-non-dynamic")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOutcomeTag(true, true, true, false, false, false),
                    "ineligible_kinematic",
                    "ineligible-kinematic")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOutcomeTag(true, true, false, false, true, false),
                    "runtime_apply_failed",
                    "runtime-apply-failed")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOutcomeTag(true, true, false, false, false, true),
                    "recovery_applied",
                    "recovery-applied")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOutcomeTag(false, true, false, false, false, false),
                    "none",
                    "none")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceFailureCauseTag(true, false),
                    "stale_binding",
                    "failure-cause-stale-binding")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceFailureCauseTag(false, true),
                    "backend_reject",
                    "failure-cause-backend-reject")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceFailureCauseTag(false, false),
                    "none",
                    "failure-cause-none")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceFailureCauseTag(true, true),
                    "stale_binding",
                    "failure-cause-deterministic-priority")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceFailureCausePrecedenceTag(true, false),
                    "stale_binding_first",
                    "failure-cause-precedence-stale-first")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceFailureCausePrecedenceTag(false, true),
                    "backend_reject_first",
                    "failure-cause-precedence-backend-first")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceFailureCausePrecedenceTag(true, true),
                    "stale_binding_first",
                    "failure-cause-precedence-priority")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceFailureCausePrecedenceTag(false, false),
                    "none",
                    "failure-cause-precedence-none")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(false, true, false, false, false, false),
                    "skipped_no_command",
                    "decision-skipped-no-command")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(true, false, false, false, false, false),
                    "skipped_ineligible_non_dynamic",
                    "decision-skipped-ineligible-non-dynamic")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(true, true, true, false, false, false),
                    "skipped_ineligible_kinematic",
                    "decision-skipped-ineligible-kinematic")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(true, true, false, false, false, false),
                    "applied_runtime",
                    "decision-applied-runtime")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(true, true, false, true, false, false),
                    "failed_runtime",
                    "decision-failed-runtime-stale")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(true, true, false, false, true, false),
                    "failed_runtime",
                    "decision-failed-runtime-backend")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(true, true, false, false, false, true),
                    "recovered_via_fallback",
                    "decision-recovered-via-fallback")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(false, true, false, true, true, true),
                    "recovered_via_fallback",
                    "decision-deterministic-priority-recovery")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationTag(false, true, false, false),
                    "linear_impulse",
                    "combined-operation-linear-impulse")
        || !expect_tag(ClassifyRuntimeCommandTraceStageTag(true, false),
                       "update",
                       "combined-stage-update")
        || !expect_tag(ClassifyRuntimeCommandTraceOperationPrecedenceTag(false, true, false, false),
                       "none",
                       "combined-operation-precedence-single")
        || !expect_tag(ClassifyRuntimeCommandTraceOutcomeTag(true, true, false, false, true, false),
                       "runtime_apply_failed",
                       "combined-outcome-runtime-apply-failed")
        || !expect_tag(ClassifyRuntimeCommandTraceFailureCauseTag(false, true),
                       "backend_reject",
                       "combined-cause-backend-reject")
        || !expect_tag(ClassifyRuntimeCommandTraceFailureCausePrecedenceTag(false, true),
                       "backend_reject_first",
                       "combined-cause-precedence-backend")
        || !expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(true, true, false, false, true, false),
                       "failed_runtime",
                       "combined-decision-failed")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationTag(false, false, false, true),
                    "angular_impulse",
                    "combined-operation-angular-impulse")
        || !expect_tag(ClassifyRuntimeCommandTraceStageTag(false, true),
                       "recovery",
                       "combined-stage-recovery")
        || !expect_tag(ClassifyRuntimeCommandTraceOutcomeTag(true, true, false, false, false, true),
                       "recovery_applied",
                       "combined-outcome-recovery-applied")
        || !expect_tag(ClassifyRuntimeCommandTraceFailureCauseTag(false, false),
                       "none",
                       "combined-cause-none")
        || !expect_tag(ClassifyRuntimeCommandTraceFailureCausePrecedenceTag(false, false),
                       "none",
                       "combined-cause-precedence-none")
        || !expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(true, true, false, false, false, true),
                       "recovered_via_fallback",
                       "combined-decision-recovery")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationTag(true, false, false, false),
                    "linear_force",
                    "combined-operation-linear-force")
        || !expect_tag(ClassifyRuntimeCommandTraceStageTag(true, false),
                       "update",
                       "combined-stage-update-stale")
        || !expect_tag(ClassifyRuntimeCommandTraceOutcomeTag(true, true, false, true, false, false),
                       "stale_runtime_binding_body",
                       "combined-outcome-stale-runtime")
        || !expect_tag(ClassifyRuntimeCommandTraceFailureCauseTag(true, false),
                       "stale_binding",
                       "combined-cause-stale-binding")
        || !expect_tag(ClassifyRuntimeCommandTraceFailureCausePrecedenceTag(true, false),
                       "stale_binding_first",
                       "combined-cause-precedence-stale")
        || !expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(true, true, false, true, false, false),
                       "failed_runtime",
                       "combined-decision-stale-failed")) {
        return false;
    }
    if (!expect_tag(ClassifyRuntimeCommandTraceOperationTag(true, true, false, true),
                    "linear_force",
                    "matrix-operation-priority")
        || !expect_tag(ClassifyRuntimeCommandTraceOperationPrecedenceTag(true, true, false, true),
                       "linear_force_first",
                       "matrix-operation-precedence")
        || !expect_tag(ClassifyRuntimeCommandTraceFailureCauseTag(true, true),
                       "stale_binding",
                       "matrix-cause-priority")
        || !expect_tag(ClassifyRuntimeCommandTraceFailureCausePrecedenceTag(true, true),
                       "stale_binding_first",
                       "matrix-cause-precedence-priority")
        || !expect_tag(ClassifyRuntimeCommandTraceDecisionPathTag(true, true, false, true, true, false),
                       "failed_runtime",
                       "matrix-decision-failure")) {
        return false;
    }

    return true;
}

bool RunScenePhysicsComponentContractChecks() {
    using karma::scene::ColliderReconcileAction;
    using karma::scene::ColliderIntentComponent;
    using karma::scene::ColliderShapeKind;
    using karma::scene::CollisionMaskIntentComponent;
    using karma::scene::ControllerColliderCompatibility;
    using karma::scene::ControllerGeometryReconcileAction;
    using karma::scene::ControllerVelocityOwnership;
    using karma::scene::PhysicsTransformAuthority;
    using karma::scene::PhysicsTransformOwnershipComponent;
    using karma::scene::PhysicsComponentValidationError;
    using karma::scene::PlayerControllerIntentComponent;
    using karma::scene::RigidBodyAwakeReconcileAction;
    using karma::scene::RigidBodyKinematicReconcileAction;
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

    RigidBodyIntentComponent conflicting_locks_rigidbody{};
    conflicting_locks_rigidbody.dynamic = true;
    conflicting_locks_rigidbody.rotation_locked = true;
    conflicting_locks_rigidbody.translation_locked = true;
    if (karma::scene::ValidateRigidBodyIntent(conflicting_locks_rigidbody, &error)
        || error != PhysicsComponentValidationError::ConflictingMotionLocks) {
        std::cerr << "scene physics component contract: conflicting dynamic motion locks were not rejected\n";
        return false;
    }

    RigidBodyIntentComponent invalid_static_kinematic_rigidbody{};
    invalid_static_kinematic_rigidbody.dynamic = false;
    invalid_static_kinematic_rigidbody.kinematic = true;
    if (karma::scene::ValidateRigidBodyIntent(invalid_static_kinematic_rigidbody, &error)
        || error != PhysicsComponentValidationError::InvalidKinematicState) {
        std::cerr << "scene physics component contract: static rigidbody with kinematic intent was not rejected\n";
        return false;
    }

    RigidBodyIntentComponent invalid_linear_damping_rigidbody{};
    invalid_linear_damping_rigidbody.linear_damping = -0.1f;
    if (karma::scene::ValidateRigidBodyIntent(invalid_linear_damping_rigidbody, &error)
        || error != PhysicsComponentValidationError::NonPositiveDimension) {
        std::cerr << "scene physics component contract: negative linear damping was not rejected\n";
        return false;
    }

    RigidBodyIntentComponent invalid_angular_damping_rigidbody{};
    invalid_angular_damping_rigidbody.angular_damping = std::numeric_limits<float>::quiet_NaN();
    if (karma::scene::ValidateRigidBodyIntent(invalid_angular_damping_rigidbody, &error)
        || error != PhysicsComponentValidationError::NonFiniteValue) {
        std::cerr << "scene physics component contract: non-finite angular damping was not rejected\n";
        return false;
    }

    RigidBodyIntentComponent nan_velocity_rigidbody{};
    nan_velocity_rigidbody.linear_velocity.x = std::numeric_limits<float>::quiet_NaN();
    if (karma::scene::ValidateRigidBodyIntent(nan_velocity_rigidbody, &error)
        || error != PhysicsComponentValidationError::NonFiniteValue) {
        std::cerr << "scene physics component contract: rigidbody NaN velocity was not rejected\n";
        return false;
    }

    RigidBodyIntentComponent nan_force_rigidbody{};
    nan_force_rigidbody.linear_force.x = std::numeric_limits<float>::quiet_NaN();
    if (karma::scene::ValidateRigidBodyIntent(nan_force_rigidbody, &error)
        || error != PhysicsComponentValidationError::NonFiniteValue) {
        std::cerr << "scene physics component contract: rigidbody NaN force command was not rejected\n";
        return false;
    }

    RigidBodyIntentComponent nan_impulse_rigidbody{};
    nan_impulse_rigidbody.linear_impulse.y = std::numeric_limits<float>::quiet_NaN();
    if (karma::scene::ValidateRigidBodyIntent(nan_impulse_rigidbody, &error)
        || error != PhysicsComponentValidationError::NonFiniteValue) {
        std::cerr << "scene physics component contract: rigidbody NaN impulse command was not rejected\n";
        return false;
    }

    RigidBodyIntentComponent nan_torque_rigidbody{};
    nan_torque_rigidbody.angular_torque.z = std::numeric_limits<float>::quiet_NaN();
    if (karma::scene::ValidateRigidBodyIntent(nan_torque_rigidbody, &error)
        || error != PhysicsComponentValidationError::NonFiniteValue) {
        std::cerr << "scene physics component contract: rigidbody NaN torque command was not rejected\n";
        return false;
    }

    RigidBodyIntentComponent nan_angular_impulse_rigidbody{};
    nan_angular_impulse_rigidbody.angular_impulse.x = std::numeric_limits<float>::quiet_NaN();
    if (karma::scene::ValidateRigidBodyIntent(nan_angular_impulse_rigidbody, &error)
        || error != PhysicsComponentValidationError::NonFiniteValue) {
        std::cerr << "scene physics component contract: rigidbody NaN angular impulse command was not rejected\n";
        return false;
    }

    RigidBodyIntentComponent force_enable_toggle_rigidbody{};
    force_enable_toggle_rigidbody.linear_force = glm::vec3(2.0f, 0.0f, 0.0f);
    force_enable_toggle_rigidbody.linear_force_enabled = false;
    if (karma::scene::HasRuntimeLinearForceCommand(force_enable_toggle_rigidbody)) {
        std::cerr << "scene physics component contract: disabled persistent linear force command was not gated\n";
        return false;
    }
    force_enable_toggle_rigidbody.linear_force_enabled = true;
    if (!karma::scene::HasRuntimeLinearForceCommand(force_enable_toggle_rigidbody)) {
        std::cerr << "scene physics component contract: enabled persistent linear force command was not detected\n";
        return false;
    }

    RigidBodyIntentComponent torque_enable_toggle_rigidbody{};
    torque_enable_toggle_rigidbody.angular_torque = glm::vec3(0.0f, 3.0f, 0.0f);
    torque_enable_toggle_rigidbody.angular_torque_enabled = false;
    if (karma::scene::HasRuntimeAngularTorqueCommand(torque_enable_toggle_rigidbody)) {
        std::cerr << "scene physics component contract: disabled persistent angular torque command was not gated\n";
        return false;
    }
    torque_enable_toggle_rigidbody.angular_torque_enabled = true;
    if (!karma::scene::HasRuntimeAngularTorqueCommand(torque_enable_toggle_rigidbody)) {
        std::cerr << "scene physics component contract: enabled persistent angular torque command was not detected\n";
        return false;
    }

    RigidBodyIntentComponent clear_command_rigidbody{};
    clear_command_rigidbody.linear_force = glm::vec3(1.0f, 0.0f, 0.0f);
    clear_command_rigidbody.linear_impulse = glm::vec3(0.3f, 0.0f, 0.0f);
    clear_command_rigidbody.angular_torque = glm::vec3(0.0f, 2.0f, 0.0f);
    clear_command_rigidbody.angular_impulse = glm::vec3(0.0f, 0.6f, 0.0f);
    clear_command_rigidbody.clear_runtime_commands_requested = true;
    if (!karma::scene::HasRuntimeCommandClearRequest(clear_command_rigidbody)) {
        std::cerr << "scene physics component contract: runtime clear request was not detected\n";
        return false;
    }
    karma::scene::ClearRuntimeCommandIntents(clear_command_rigidbody);
    if (karma::scene::HasRuntimeCommandClearRequest(clear_command_rigidbody)
        || karma::scene::HasRuntimeLinearForceCommand(clear_command_rigidbody)
        || karma::scene::HasRuntimeLinearImpulseCommand(clear_command_rigidbody)
        || karma::scene::HasRuntimeAngularTorqueCommand(clear_command_rigidbody)
        || karma::scene::HasRuntimeAngularImpulseCommand(clear_command_rigidbody)) {
        std::cerr << "scene physics component contract: runtime command clear helper did not reset command state\n";
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

    ColliderIntentComponent invalid_friction = default_collider;
    invalid_friction.friction = -0.1f;
    if (karma::scene::ValidateColliderIntent(invalid_friction, &error)
        || error != PhysicsComponentValidationError::NonPositiveDimension) {
        std::cerr << "scene physics component contract: invalid friction material value was not rejected\n";
        return false;
    }

    ColliderIntentComponent invalid_restitution = default_collider;
    invalid_restitution.restitution = 1.5f;
    if (karma::scene::ValidateColliderIntent(invalid_restitution, &error)
        || error != PhysicsComponentValidationError::NonPositiveDimension) {
        std::cerr << "scene physics component contract: out-of-range restitution was not rejected\n";
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

    ColliderIntentComponent collider_material_change = default_collider;
    collider_material_change.friction = 0.9f;
    if (karma::scene::ClassifyColliderReconcileAction(default_collider, collider_material_change)
        != ColliderReconcileAction::UpdateRuntimeProperties) {
        std::cerr << "scene physics component contract: collider material reconcile classification mismatch\n";
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

    if (karma::scene::ClassifyRigidBodyKinematicReconcileAction(default_rigidbody, default_rigidbody)
        != RigidBodyKinematicReconcileAction::NoOp) {
        std::cerr << "scene physics component contract: rigidbody kinematic no-op reconcile mismatch\n";
        return false;
    }
    RigidBodyIntentComponent kinematic_toggle_rigidbody = default_rigidbody;
    kinematic_toggle_rigidbody.kinematic = true;
    if (karma::scene::ClassifyRigidBodyKinematicReconcileAction(default_rigidbody, kinematic_toggle_rigidbody)
        != RigidBodyKinematicReconcileAction::UpdateRuntimeKinematic) {
        std::cerr << "scene physics component contract: rigidbody kinematic toggle reconcile mismatch\n";
        return false;
    }
    if (karma::scene::ClassifyRigidBodyKinematicReconcileAction(default_rigidbody, invalid_static_kinematic_rigidbody)
        != RigidBodyKinematicReconcileAction::RejectInvalidIntent) {
        std::cerr << "scene physics component contract: invalid rigidbody kinematic reconcile mismatch\n";
        return false;
    }

    if (karma::scene::ClassifyRigidBodyAwakeReconcileAction(default_rigidbody, default_rigidbody)
        != RigidBodyAwakeReconcileAction::NoOp) {
        std::cerr << "scene physics component contract: rigidbody awake no-op reconcile mismatch\n";
        return false;
    }
    RigidBodyIntentComponent awake_toggle_rigidbody = default_rigidbody;
    awake_toggle_rigidbody.awake = false;
    if (karma::scene::ClassifyRigidBodyAwakeReconcileAction(default_rigidbody, awake_toggle_rigidbody)
        != RigidBodyAwakeReconcileAction::UpdateRuntimeAwakeState) {
        std::cerr << "scene physics component contract: rigidbody awake toggle reconcile mismatch\n";
        return false;
    }
    if (karma::scene::ClassifyRigidBodyAwakeReconcileAction(default_rigidbody, invalid_static_kinematic_rigidbody)
        != RigidBodyAwakeReconcileAction::RejectInvalidIntent) {
        std::cerr << "scene physics component contract: invalid rigidbody awake reconcile mismatch\n";
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

    RigidBodyIntentComponent non_dynamic_rigidbody{};
    non_dynamic_rigidbody.dynamic = false;
    compatibility = karma::scene::ClassifyControllerColliderCompatibility(
        default_controller, &default_collider, &non_dynamic_rigidbody);
    if (compatibility != ControllerColliderCompatibility::EnabledControllerRequiresDynamicRigidBody
        || karma::scene::IsControllerColliderCompatible(compatibility)) {
        std::cerr << "scene physics component contract: non-dynamic rigidbody compatibility mismatch\n";
        return false;
    }

    if (karma::scene::ClassifyControllerVelocityOwnership(&default_controller, ControllerColliderCompatibility::Compatible)
        != ControllerVelocityOwnership::ControllerIntent
        || !karma::scene::IsControllerVelocityOwner(ControllerVelocityOwnership::ControllerIntent)) {
        std::cerr << "scene physics component contract: controller velocity ownership mismatch\n";
        return false;
    }
    if (karma::scene::ClassifyControllerVelocityOwnership(
            &controller_disabled, ControllerColliderCompatibility::CompatibleControllerDisabled)
        != ControllerVelocityOwnership::RigidbodyIntent
        || karma::scene::IsControllerVelocityOwner(ControllerVelocityOwnership::RigidbodyIntent)) {
        std::cerr << "scene physics component contract: disabled-controller velocity ownership mismatch\n";
        return false;
    }
    if (karma::scene::ClassifyControllerVelocityOwnership(
            &default_controller, ControllerColliderCompatibility::EnabledControllerRequiresDynamicRigidBody)
        != ControllerVelocityOwnership::RigidbodyIntent) {
        std::cerr << "scene physics component contract: non-dynamic controller velocity ownership mismatch\n";
        return false;
    }
    if (karma::scene::ClassifyControllerVelocityOwnership(nullptr, ControllerColliderCompatibility::Compatible)
        != ControllerVelocityOwnership::RigidbodyIntent) {
        std::cerr << "scene physics component contract: null-controller velocity ownership mismatch\n";
        return false;
    }

    if (karma::scene::ClassifyControllerGeometryReconcileAction(
            default_controller, default_controller, ControllerColliderCompatibility::Compatible)
        != ControllerGeometryReconcileAction::NoOp) {
        std::cerr << "scene physics component contract: controller geometry no-op reconcile mismatch\n";
        return false;
    }
    PlayerControllerIntentComponent controller_half_extents_change = default_controller;
    controller_half_extents_change.half_extents.x += 0.2f;
    if (karma::scene::ClassifyControllerGeometryReconcileAction(
            default_controller, controller_half_extents_change, ControllerColliderCompatibility::Compatible)
        != ControllerGeometryReconcileAction::RebuildRuntimeShape) {
        std::cerr << "scene physics component contract: controller half-extents reconcile mismatch\n";
        return false;
    }
    PlayerControllerIntentComponent controller_center_change = default_controller;
    controller_center_change.center.y += 0.3f;
    if (karma::scene::ClassifyControllerGeometryReconcileAction(
            default_controller, controller_center_change, ControllerColliderCompatibility::Compatible)
        != ControllerGeometryReconcileAction::RebuildRuntimeShape) {
        std::cerr << "scene physics component contract: controller center reconcile mismatch\n";
        return false;
    }
    if (karma::scene::ClassifyControllerGeometryReconcileAction(default_controller,
                                                                controller_center_change,
                                                                ControllerColliderCompatibility::CompatibleControllerDisabled)
        != ControllerGeometryReconcileAction::NoOp) {
        std::cerr << "scene physics component contract: disabled-controller geometry reconcile mismatch\n";
        return false;
    }
    PlayerControllerIntentComponent invalid_geometry_controller = default_controller;
    invalid_geometry_controller.half_extents.y = 0.0f;
    if (karma::scene::ClassifyControllerGeometryReconcileAction(default_controller,
                                                                invalid_geometry_controller,
                                                                ControllerColliderCompatibility::Compatible)
        != ControllerGeometryReconcileAction::RejectInvalidIntent) {
        std::cerr << "scene physics component contract: invalid controller geometry reconcile mismatch\n";
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
    auto expect_decision_path = [&](std::string_view actual, std::string_view expected, std::string_view label) {
        if (actual != expected) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " ecs-sync decision-path classification mismatch for " << label << ": expected '" << expected
                      << "' got '" << actual << "'\n";
            return false;
        }
        return true;
    };
    auto expect_captured_trace = [&](const std::shared_ptr<RuntimeTraceCaptureSink>& sink,
                                     std::initializer_list<std::string_view> tokens,
                                     std::string_view label) {
        if (!sink || !sink->ContainsEvent("physics.system", tokens)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " ecs-sync trace capture missing expected runtime-command tags for " << label << "\n";
            return false;
        }
        return true;
    };
    auto expect_captured_mesh_trace = [&](const std::shared_ptr<RuntimeTraceCaptureSink>& sink,
                                          std::initializer_list<std::string_view> tokens,
                                          std::string_view label) {
        if (!sink || !sink->ContainsEvent("physics.system", tokens)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " ecs-sync trace capture missing expected static-mesh ingest tags for " << label << "\n";
            return false;
        }
        return true;
    };

    const std::string static_mesh_asset = ResolveTestAssetPath("demo/worlds/r55man-2/world.glb");
    const std::string missing_mesh_asset = ResolveTestAssetPath("demo/worlds/phase4x-missing-static-mesh.glb");
    const std::string non_mesh_asset = ResolveTestAssetPath("README.md");

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
    auto trace_sink = std::make_shared<RuntimeTraceCaptureSink>();
    auto capture_logger = std::make_shared<spdlog::logger>("physics_backend_parity_trace_capture", trace_sink);
    capture_logger->set_level(spdlog::level::trace);
    const auto previous_default_logger = spdlog::default_logger();
    struct TraceLoggerRestoreScope final {
        explicit TraceLoggerRestoreScope(std::shared_ptr<spdlog::logger> previous) : previous_(std::move(previous)) {}
        ~TraceLoggerRestoreScope() {
            if (previous_) {
                spdlog::set_default_logger(previous_);
            }
            spdlog::drop("physics.system");
        }

     private:
        std::shared_ptr<spdlog::logger> previous_{};
    } trace_logger_restore(previous_default_logger);
    spdlog::set_default_logger(capture_logger);
    spdlog::drop("physics.system");
    karma::common::logging::EnableTraceChannels("physics.system");

    const auto entity_create = world.createEntity();
    world.add<TransformComponent>(entity_create, make_transform_component(glm::vec3(0.0f, 3.0f, 0.0f)));
    RigidBodyIntentComponent create_rigidbody{};
    create_rigidbody.linear_damping = 0.35f;
    create_rigidbody.angular_damping = 0.7f;
    world.add<RigidBodyIntentComponent>(entity_create, create_rigidbody);
    world.add<ColliderIntentComponent>(entity_create, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_create, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId created_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_create, created_body)
        || created_body == karma::physics::backend::kInvalidBodyId
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
    float create_linear_damping = 0.0f;
    float create_angular_damping = 0.0f;
    if (!physics.getBodyLinearDamping(created_body, create_linear_damping)
        || !physics.getBodyAngularDamping(created_body, create_angular_damping)
        || !NearlyEqual(create_linear_damping, create_rigidbody.linear_damping, 5e-3f)
        || !NearlyEqual(create_angular_damping, create_rigidbody.angular_damping, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not apply rigidbody damping on create\n";
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
    BodyId rebuilt_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_create, rebuilt_body)
        || rebuilt_body == karma::physics::backend::kInvalidBodyId
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
    mesh_collider.mesh_path = static_mesh_asset;
    world.add<ColliderIntentComponent>(entity_mesh, mesh_collider);
    world.add<PhysicsTransformOwnershipComponent>(entity_mesh, PhysicsTransformOwnershipComponent{});

    trace_sink->Clear();
    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_mesh)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create static-mesh runtime body from mesh path\n";
        physics.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='create_success'", "cause='none'"},
                                    "create success")) {
        physics.shutdown();
        return false;
    }

    auto* mesh_dynamic_flip = world.tryGet<RigidBodyIntentComponent>(entity_mesh);
    mesh_dynamic_flip->dynamic = true;
    trace_sink->Clear();
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_mesh)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not teardown static-mesh runtime body after unsupported dynamic transition\n";
        physics.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='reject'",
                                     "cause='ineligible_dynamic_mesh_intent'"},
                                    "ineligible dynamic mesh-intent reject")) {
        physics.shutdown();
        return false;
    }

    mesh_dynamic_flip->dynamic = false;
    auto* mesh_invalid_path = world.tryGet<ColliderIntentComponent>(entity_mesh);
    mesh_invalid_path->mesh_path = "   ";
    trace_sink->Clear();
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_mesh)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not teardown static-mesh runtime body for invalid mesh-path intent\n";
        physics.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='reject'", "cause='invalid_intent'"},
                                    "invalid-intent reject")) {
        physics.shutdown();
        return false;
    }

    mesh_invalid_path->mesh_path = missing_mesh_asset;
    trace_sink->Clear();
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_mesh)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not teardown static-mesh runtime body for missing mesh path\n";
        physics.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='reject'",
                                     "cause='mesh_asset_load_or_cook_failed'"},
                                    "mesh-asset load/cook failure reject")) {
        physics.shutdown();
        return false;
    }

    mesh_invalid_path->mesh_path = non_mesh_asset;
    trace_sink->Clear();
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_mesh)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not teardown static-mesh runtime body for backend reject create path\n";
        physics.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='reject'", "cause='backend_reject_create'"},
                                    "backend reject create")) {
        physics.shutdown();
        return false;
    }

    mesh_invalid_path->mesh_path = static_mesh_asset;
    trace_sink->Clear();
    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_mesh)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not recreate static-mesh runtime body after returning to supported static intent\n";
        physics.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='recovery_recreate_success'", "cause='none'"},
                                    "recovery recreate success after reject correction")) {
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
    ColliderIntentComponent runtime_properties_collider_intent{};
    runtime_properties_collider_intent.friction = 0.35f;
    runtime_properties_collider_intent.restitution = 0.15f;
    world.add<ColliderIntentComponent>(entity_runtime_properties, runtime_properties_collider_intent);
    world.add<PhysicsTransformOwnershipComponent>(entity_runtime_properties, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId runtime_properties_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_runtime_properties, runtime_properties_body)
        || runtime_properties_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create runtime-properties fixture body\n";
        physics.shutdown();
        return false;
    }
    float runtime_friction = 0.0f;
    float runtime_restitution = 0.0f;
    if (!physics.getBodyFriction(runtime_properties_body, runtime_friction)
        || !physics.getBodyRestitution(runtime_properties_body, runtime_restitution)
        || !NearlyEqual(runtime_friction, runtime_properties_collider_intent.friction, 5e-3f)
        || !NearlyEqual(runtime_restitution, runtime_properties_collider_intent.restitution, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not apply collider material properties on create\n";
        physics.shutdown();
        return false;
    }

    auto* runtime_properties_collider = world.tryGet<ColliderIntentComponent>(entity_runtime_properties);
    runtime_properties_collider->is_trigger = true;
    sync.preSimulate(world);

    BodyId trigger_updated_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_runtime_properties, trigger_updated_body)
        || trigger_updated_body == karma::physics::backend::kInvalidBodyId) {
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

    BodyId post_filter_update_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_runtime_properties, post_filter_update_body)
        || post_filter_update_body == karma::physics::backend::kInvalidBodyId) {
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

    const BodyId pre_material_friction_update_body = post_filter_update_body;
    runtime_properties_collider->friction = 0.9f;
    sync.preSimulate(world);

    BodyId post_material_friction_update_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_runtime_properties, post_material_friction_update_body)
        || post_material_friction_update_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lost runtime-properties fixture after friction transition\n";
        physics.shutdown();
        return false;
    }
    if (post_material_friction_update_body != pre_material_friction_update_body) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly rebuilt body for supported runtime friction mutation\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyFriction(post_material_friction_update_body, runtime_friction)
        || !NearlyEqual(runtime_friction, runtime_properties_collider->friction, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync friction transition did not converge to expected runtime value\n";
        physics.shutdown();
        return false;
    }

    const BodyId pre_material_restitution_update_body = post_material_friction_update_body;
    runtime_properties_collider->restitution = 0.4f;
    sync.preSimulate(world);

    BodyId post_material_restitution_update_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_runtime_properties, post_material_restitution_update_body)
        || post_material_restitution_update_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lost runtime-properties fixture after restitution transition\n";
        physics.shutdown();
        return false;
    }
    if (backend == BackendKind::Jolt
        && post_material_restitution_update_body == pre_material_restitution_update_body) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync expected deterministic rebuild fallback for unsupported runtime restitution mutation\n";
        physics.shutdown();
        return false;
    }
    if (backend == BackendKind::PhysX
        && post_material_restitution_update_body != pre_material_restitution_update_body) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly rebuilt body for supported runtime restitution mutation\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyRestitution(post_material_restitution_update_body, runtime_restitution)
        || !NearlyEqual(runtime_restitution, runtime_properties_collider->restitution, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync restitution transition did not converge to expected runtime value\n";
        physics.shutdown();
        return false;
    }

    const auto entity_damping = world.createEntity();
    world.add<TransformComponent>(entity_damping, make_transform_component(glm::vec3(-5.0f, 6.0f, 0.0f)));
    RigidBodyIntentComponent damping_rigidbody{};
    damping_rigidbody.linear_damping = 0.25f;
    damping_rigidbody.angular_damping = 0.9f;
    world.add<RigidBodyIntentComponent>(entity_damping, damping_rigidbody);
    world.add<ColliderIntentComponent>(entity_damping, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_damping, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId damping_body_before = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_damping, damping_body_before)
        || damping_body_before == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create damping fixture runtime body\n";
        physics.shutdown();
        return false;
    }

    float runtime_linear_damping = 0.0f;
    float runtime_angular_damping = 0.0f;
    if (!physics.getBodyLinearDamping(damping_body_before, runtime_linear_damping)
        || !physics.getBodyAngularDamping(damping_body_before, runtime_angular_damping)
        || !NearlyEqual(runtime_linear_damping, damping_rigidbody.linear_damping, 5e-3f)
        || !NearlyEqual(runtime_angular_damping, damping_rigidbody.angular_damping, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync damping fixture did not apply create-time damping\n";
        physics.shutdown();
        return false;
    }

    auto* damping_rigidbody_mut = world.tryGet<RigidBodyIntentComponent>(entity_damping);
    damping_rigidbody_mut->linear_damping = 0.05f;
    damping_rigidbody_mut->angular_damping = 0.4f;
    sync.preSimulate(world);

    BodyId damping_body_after = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_damping, damping_body_after)
        || damping_body_after != damping_body_before) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync damping mutation unexpectedly caused body-id churn\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyLinearDamping(damping_body_after, runtime_linear_damping)
        || !physics.getBodyAngularDamping(damping_body_after, runtime_angular_damping)
        || !NearlyEqual(runtime_linear_damping, damping_rigidbody_mut->linear_damping, 5e-3f)
        || !NearlyEqual(runtime_angular_damping, damping_rigidbody_mut->angular_damping, 5e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync damping mutation did not update runtime damping values\n";
        physics.shutdown();
        return false;
    }

    const auto entity_motion_lock = world.createEntity();
    world.add<TransformComponent>(entity_motion_lock, make_transform_component(glm::vec3(-7.0f, 6.0f, 0.0f)));
    RigidBodyIntentComponent motion_lock_rigidbody{};
    motion_lock_rigidbody.rotation_locked = false;
    motion_lock_rigidbody.translation_locked = false;
    world.add<RigidBodyIntentComponent>(entity_motion_lock, motion_lock_rigidbody);
    world.add<ColliderIntentComponent>(entity_motion_lock, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_motion_lock, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId motion_lock_body_before = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_motion_lock, motion_lock_body_before)
        || motion_lock_body_before == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create motion-lock fixture runtime body\n";
        physics.shutdown();
        return false;
    }
    bool runtime_rotation_locked = true;
    bool runtime_translation_locked = true;
    if (!physics.getBodyRotationLocked(motion_lock_body_before, runtime_rotation_locked)
        || !physics.getBodyTranslationLocked(motion_lock_body_before, runtime_translation_locked)
        || runtime_rotation_locked
        || runtime_translation_locked) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync motion-lock fixture did not start unlocked\n";
        physics.shutdown();
        return false;
    }

    auto* motion_lock_rigidbody_mut = world.tryGet<RigidBodyIntentComponent>(entity_motion_lock);
    motion_lock_rigidbody_mut->rotation_locked = true;
    motion_lock_rigidbody_mut->translation_locked = false;
    sync.preSimulate(world);

    BodyId motion_lock_body_after_rotation_enable = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_motion_lock, motion_lock_body_after_rotation_enable)
        || motion_lock_body_after_rotation_enable == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lost motion-lock fixture after valid rotation-lock transition\n";
        physics.shutdown();
        return false;
    }
    if (backend == BackendKind::PhysX && motion_lock_body_after_rotation_enable != motion_lock_body_before) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly rebuilt body for supported runtime rotation-lock mutation\n";
        physics.shutdown();
        return false;
    }
    if (backend == BackendKind::Jolt && motion_lock_body_after_rotation_enable == motion_lock_body_before) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync expected deterministic rebuild fallback for unsupported runtime rotation-lock mutation\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyRotationLocked(motion_lock_body_after_rotation_enable, runtime_rotation_locked)
        || !physics.getBodyTranslationLocked(motion_lock_body_after_rotation_enable, runtime_translation_locked)
        || !runtime_rotation_locked
        || runtime_translation_locked) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync valid rotation-lock transition did not converge to expected runtime state\n";
        physics.shutdown();
        return false;
    }

    sync.preSimulate(world);
    BodyId motion_lock_body_after_rotation_noop = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_motion_lock, motion_lock_body_after_rotation_noop)
        || motion_lock_body_after_rotation_noop != motion_lock_body_after_rotation_enable) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync motion-lock fixture was not stable after valid rotation-lock transition\n";
        physics.shutdown();
        return false;
    }

    motion_lock_rigidbody_mut->rotation_locked = false;
    motion_lock_rigidbody_mut->translation_locked = false;
    sync.preSimulate(world);
    BodyId motion_lock_body_after_disable = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_motion_lock, motion_lock_body_after_disable)
        || motion_lock_body_after_disable == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lost motion-lock fixture after lock-disable transition\n";
        physics.shutdown();
        return false;
    }
    if (backend == BackendKind::PhysX && motion_lock_body_after_disable != motion_lock_body_after_rotation_enable) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly rebuilt body for supported lock-disable runtime mutation\n";
        physics.shutdown();
        return false;
    }
    if (backend == BackendKind::Jolt && motion_lock_body_after_disable == motion_lock_body_after_rotation_enable) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync expected deterministic rebuild fallback for unsupported lock-disable runtime mutation\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyRotationLocked(motion_lock_body_after_disable, runtime_rotation_locked)
        || !physics.getBodyTranslationLocked(motion_lock_body_after_disable, runtime_translation_locked)
        || runtime_rotation_locked
        || runtime_translation_locked) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lock-disable transition did not converge to unlocked runtime state\n";
        physics.shutdown();
        return false;
    }

    motion_lock_rigidbody_mut->rotation_locked = true;
    motion_lock_rigidbody_mut->translation_locked = true;
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_motion_lock)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly retained runtime body for invalid dynamic both-lock intent\n";
        physics.shutdown();
        return false;
    }
    sync.preSimulate(world);
    if (sync.hasRuntimeBinding(entity_motion_lock)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync invalid both-lock rejection was not stable across repeated pre-sim passes\n";
        physics.shutdown();
        return false;
    }

    motion_lock_rigidbody_mut->rotation_locked = false;
    motion_lock_rigidbody_mut->translation_locked = false;
    sync.preSimulate(world);
    BodyId motion_lock_body_after_recovery = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_motion_lock, motion_lock_body_after_recovery)
        || motion_lock_body_after_recovery == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not recreate motion-lock fixture after invalid both-lock recovery\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyRotationLocked(motion_lock_body_after_recovery, runtime_rotation_locked)
        || !physics.getBodyTranslationLocked(motion_lock_body_after_recovery, runtime_translation_locked)
        || runtime_rotation_locked
        || runtime_translation_locked) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync recovered motion-lock fixture did not converge to unlocked runtime state\n";
        physics.shutdown();
        return false;
    }

    const auto entity_kinematic = world.createEntity();
    world.add<TransformComponent>(entity_kinematic, make_transform_component(glm::vec3(-9.0f, 6.0f, 0.0f)));
    RigidBodyIntentComponent kinematic_rigidbody{};
    kinematic_rigidbody.dynamic = true;
    kinematic_rigidbody.kinematic = false;
    world.add<RigidBodyIntentComponent>(entity_kinematic, kinematic_rigidbody);
    world.add<ColliderIntentComponent>(entity_kinematic, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_kinematic, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId kinematic_body_before_toggle = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_kinematic, kinematic_body_before_toggle)
        || kinematic_body_before_toggle == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create kinematic fixture runtime body\n";
        physics.shutdown();
        return false;
    }

    bool runtime_kinematic_enabled = true;
    if (!physics.getBodyKinematic(kinematic_body_before_toggle, runtime_kinematic_enabled)
        || runtime_kinematic_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync kinematic fixture did not start in non-kinematic state\n";
        physics.shutdown();
        return false;
    }

    auto* kinematic_rigidbody_mut = world.tryGet<RigidBodyIntentComponent>(entity_kinematic);
    kinematic_rigidbody_mut->kinematic = true;
    sync.preSimulate(world);

    BodyId kinematic_body_after_enable = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_kinematic, kinematic_body_after_enable)
        || kinematic_body_after_enable == karma::physics::backend::kInvalidBodyId
        || !physics.getBodyKinematic(kinematic_body_after_enable, runtime_kinematic_enabled)
        || !runtime_kinematic_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync kinematic-enable transition did not converge to expected runtime state\n";
        physics.shutdown();
        return false;
    }

    // Force runtime mutation failure by deleting the backend body behind ECS binding.
    physics.destroyBody(kinematic_body_after_enable);
    kinematic_rigidbody_mut->kinematic = false;
    sync.preSimulate(world);

    BodyId kinematic_body_after_forced_fallback = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_kinematic, kinematic_body_after_forced_fallback)
        || kinematic_body_after_forced_fallback == karma::physics::backend::kInvalidBodyId
        || kinematic_body_after_forced_fallback == kinematic_body_after_enable
        || !physics.getBodyKinematic(kinematic_body_after_forced_fallback, runtime_kinematic_enabled)
        || runtime_kinematic_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync kinematic mutation fallback did not deterministically rebuild/recover runtime state\n";
        physics.shutdown();
        return false;
    }

    const auto entity_awake = world.createEntity();
    world.add<TransformComponent>(entity_awake, make_transform_component(glm::vec3(-11.0f, 6.0f, 0.0f)));
    RigidBodyIntentComponent awake_rigidbody{};
    awake_rigidbody.dynamic = true;
    awake_rigidbody.awake = true;
    world.add<RigidBodyIntentComponent>(entity_awake, awake_rigidbody);
    world.add<ColliderIntentComponent>(entity_awake, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_awake, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId awake_body_before_toggle = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_awake, awake_body_before_toggle)
        || awake_body_before_toggle == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create awake fixture runtime body\n";
        physics.shutdown();
        return false;
    }

    bool runtime_awake_enabled = false;
    if (!physics.getBodyAwake(awake_body_before_toggle, runtime_awake_enabled) || !runtime_awake_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync awake fixture did not start in awake state\n";
        physics.shutdown();
        return false;
    }

    auto* awake_rigidbody_mut = world.tryGet<RigidBodyIntentComponent>(entity_awake);
    awake_rigidbody_mut->awake = false;
    sync.preSimulate(world);

    BodyId awake_body_after_sleep = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_awake, awake_body_after_sleep)
        || awake_body_after_sleep != awake_body_before_toggle
        || !physics.getBodyAwake(awake_body_after_sleep, runtime_awake_enabled)
        || runtime_awake_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync awake->sleep transition did not converge without body-id churn\n";
        physics.shutdown();
        return false;
    }

    awake_rigidbody_mut->awake = true;
    sync.preSimulate(world);
    BodyId awake_body_after_wake = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_awake, awake_body_after_wake)
        || awake_body_after_wake != awake_body_before_toggle
        || !physics.getBodyAwake(awake_body_after_wake, runtime_awake_enabled)
        || !runtime_awake_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync sleep->awake transition did not converge without body-id churn\n";
        physics.shutdown();
        return false;
    }

    // Force runtime mutation failure by deleting the backend body behind ECS binding.
    physics.destroyBody(awake_body_after_wake);
    awake_rigidbody_mut->awake = false;
    sync.preSimulate(world);

    BodyId awake_body_after_forced_fallback = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_awake, awake_body_after_forced_fallback)
        || awake_body_after_forced_fallback == karma::physics::backend::kInvalidBodyId
        || awake_body_after_forced_fallback == awake_body_after_wake
        || !physics.getBodyAwake(awake_body_after_forced_fallback, runtime_awake_enabled)
        || runtime_awake_enabled) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync awake mutation fallback did not deterministically rebuild/recover runtime state\n";
        physics.shutdown();
        return false;
    }

    const auto entity_force_impulse = world.createEntity();
    world.add<TransformComponent>(entity_force_impulse, make_transform_component(glm::vec3(-13.0f, 6.0f, 0.0f)));
    RigidBodyIntentComponent force_impulse_rigidbody{};
    force_impulse_rigidbody.dynamic = true;
    force_impulse_rigidbody.gravity_enabled = false;
    force_impulse_rigidbody.linear_force = glm::vec3(24.0f, 0.0f, 0.0f);
    force_impulse_rigidbody.linear_impulse = glm::vec3(1.1f, 0.0f, 0.0f);
    force_impulse_rigidbody.angular_torque = glm::vec3(0.0f, 16.0f, 0.0f);
    force_impulse_rigidbody.angular_impulse = glm::vec3(0.0f, 0.8f, 0.0f);
    force_impulse_rigidbody.linear_force_enabled = true;
    force_impulse_rigidbody.angular_torque_enabled = true;
    world.add<RigidBodyIntentComponent>(entity_force_impulse, force_impulse_rigidbody);
    world.add<ColliderIntentComponent>(entity_force_impulse, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_force_impulse, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId force_impulse_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_force_impulse, force_impulse_body)
        || force_impulse_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to create force/impulse fixture runtime body\n";
        physics.shutdown();
        return false;
    }

    auto* force_impulse_rigidbody_mut = world.tryGet<RigidBodyIntentComponent>(entity_force_impulse);
    if (!force_impulse_rigidbody_mut || karma::scene::HasRuntimeLinearImpulseCommand(*force_impulse_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*force_impulse_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not consume one-shot command after successful runtime apply\n";
        physics.shutdown();
        return false;
    }

    glm::vec3 force_impulse_velocity_before{};
    if (!physics.getBodyLinearVelocity(force_impulse_body, force_impulse_velocity_before)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync force/impulse fixture failed to read runtime velocity before simulation\n";
        physics.shutdown();
        return false;
    }
    glm::vec3 force_impulse_angular_before{};
    if (!physics.getBodyAngularVelocity(force_impulse_body, force_impulse_angular_before)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync force/impulse fixture failed to read runtime angular velocity before simulation\n";
        physics.shutdown();
        return false;
    }
    physics.beginFrame(1.0f / 60.0f);
    physics.simulateFixedStep(1.0f / 60.0f);
    physics.endFrame();
    glm::vec3 force_impulse_velocity_after{};
    if (!physics.getBodyLinearVelocity(force_impulse_body, force_impulse_velocity_after)
        || !(force_impulse_velocity_after.x > force_impulse_velocity_before.x + 1e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync force command did not apply expected runtime acceleration\n";
        physics.shutdown();
        return false;
    }
    glm::vec3 force_impulse_angular_after{};
    if (!physics.getBodyAngularVelocity(force_impulse_body, force_impulse_angular_after)
        || !(force_impulse_angular_after.y > force_impulse_angular_before.y + 1e-3f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync torque command did not apply expected runtime angular acceleration\n";
        physics.shutdown();
        return false;
    }

    force_impulse_rigidbody_mut->linear_force = glm::vec3(0.0f, 0.0f, 0.0f);
    force_impulse_rigidbody_mut->angular_torque = glm::vec3(0.0f, 0.0f, 0.0f);
    const BodyId force_impulse_body_before_noop = force_impulse_body;
    sync.preSimulate(world);
    BodyId force_impulse_body_after_noop = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_force_impulse, force_impulse_body_after_noop)
        || force_impulse_body_after_noop != force_impulse_body_before_noop) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync force/impulse fixture unexpectedly churned body id after impulse consume\n";
        physics.shutdown();
        return false;
    }
    glm::vec3 force_impulse_velocity_no_force_before{};
    if (!physics.getBodyLinearVelocity(force_impulse_body_after_noop, force_impulse_velocity_no_force_before)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync force/impulse fixture failed to read runtime velocity before no-force step\n";
        physics.shutdown();
        return false;
    }
    physics.beginFrame(1.0f / 60.0f);
    physics.simulateFixedStep(1.0f / 60.0f);
    physics.endFrame();
    glm::vec3 force_impulse_velocity_no_force_after{};
    if (!physics.getBodyLinearVelocity(force_impulse_body_after_noop, force_impulse_velocity_no_force_after)
        || !NearlyEqualVec3(force_impulse_velocity_no_force_after, force_impulse_velocity_no_force_before, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync consumed linear impulse command appeared to re-apply when command remained zero\n";
        physics.shutdown();
        return false;
    }
    glm::vec3 force_impulse_angular_no_torque_before{};
    if (!physics.getBodyAngularVelocity(force_impulse_body_after_noop, force_impulse_angular_no_torque_before)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync force/impulse fixture failed to read runtime angular velocity before no-torque step\n";
        physics.shutdown();
        return false;
    }
    physics.beginFrame(1.0f / 60.0f);
    physics.simulateFixedStep(1.0f / 60.0f);
    physics.endFrame();
    glm::vec3 force_impulse_angular_no_torque_after{};
    if (!physics.getBodyAngularVelocity(force_impulse_body_after_noop, force_impulse_angular_no_torque_after)
        || !NearlyEqualVec3(force_impulse_angular_no_torque_after, force_impulse_angular_no_torque_before, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync consumed angular impulse command appeared to re-apply when command remained zero\n";
        physics.shutdown();
        return false;
    }

    force_impulse_rigidbody_mut->linear_force = glm::vec3(11.0f, 0.0f, 0.0f);
    force_impulse_rigidbody_mut->linear_impulse = glm::vec3(0.4f, 0.0f, 0.0f);
    force_impulse_rigidbody_mut->angular_torque = glm::vec3(0.0f, 10.0f, 0.0f);
    force_impulse_rigidbody_mut->angular_impulse = glm::vec3(0.0f, 0.35f, 0.0f);
    force_impulse_rigidbody_mut->linear_force_enabled = true;
    force_impulse_rigidbody_mut->angular_torque_enabled = true;
    force_impulse_rigidbody_mut->clear_runtime_commands_requested = true;
    const BodyId force_impulse_body_before_clear_request = force_impulse_body_after_noop;
    sync.preSimulate(world);
    BodyId force_impulse_body_after_clear_request = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_force_impulse, force_impulse_body_after_clear_request)
        || force_impulse_body_after_clear_request != force_impulse_body_before_clear_request
        || !force_impulse_rigidbody_mut
        || karma::scene::HasRuntimeCommandClearRequest(*force_impulse_rigidbody_mut)
        || karma::scene::HasRuntimeLinearForceCommand(*force_impulse_rigidbody_mut)
        || karma::scene::HasRuntimeLinearImpulseCommand(*force_impulse_rigidbody_mut)
        || karma::scene::HasRuntimeAngularTorqueCommand(*force_impulse_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*force_impulse_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync runtime clear request did not clear command intents deterministically\n";
        physics.shutdown();
        return false;
    }

    const auto entity_non_dynamic_commands = world.createEntity();
    world.add<TransformComponent>(entity_non_dynamic_commands, make_transform_component(glm::vec3(-13.0f, 6.0f, 0.0f)));
    RigidBodyIntentComponent non_dynamic_commands_rigidbody{};
    non_dynamic_commands_rigidbody.dynamic = false;
    non_dynamic_commands_rigidbody.gravity_enabled = false;
    non_dynamic_commands_rigidbody.linear_force = glm::vec3(13.0f, 0.0f, 0.0f);
    non_dynamic_commands_rigidbody.linear_force_enabled = true;
    non_dynamic_commands_rigidbody.linear_impulse = glm::vec3(0.75f, 0.0f, 0.0f);
    non_dynamic_commands_rigidbody.angular_torque = glm::vec3(0.0f, 8.5f, 0.0f);
    non_dynamic_commands_rigidbody.angular_torque_enabled = true;
    non_dynamic_commands_rigidbody.angular_impulse = glm::vec3(0.0f, 0.55f, 0.0f);
    world.add<RigidBodyIntentComponent>(entity_non_dynamic_commands, non_dynamic_commands_rigidbody);
    world.add<ColliderIntentComponent>(entity_non_dynamic_commands, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_non_dynamic_commands, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    auto* non_dynamic_commands_rigidbody_mut = world.tryGet<RigidBodyIntentComponent>(entity_non_dynamic_commands);
    BodyId non_dynamic_commands_body_before = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_non_dynamic_commands, non_dynamic_commands_body_before)
        || non_dynamic_commands_body_before == karma::physics::backend::kInvalidBodyId
        || !non_dynamic_commands_rigidbody_mut
        || !karma::scene::HasRuntimeLinearForceCommand(*non_dynamic_commands_rigidbody_mut)
        || !karma::scene::HasRuntimeLinearImpulseCommand(*non_dynamic_commands_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularTorqueCommand(*non_dynamic_commands_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularImpulseCommand(*non_dynamic_commands_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync non-dynamic ineligible command fixture failed to keep runtime body and pending commands stable\n";
        physics.shutdown();
        return false;
    }
    if (physics.addBodyForce(non_dynamic_commands_body_before, non_dynamic_commands_rigidbody_mut->linear_force)
        || physics.addBodyLinearImpulse(non_dynamic_commands_body_before,
                                        non_dynamic_commands_rigidbody_mut->linear_impulse)
        || physics.addBodyTorque(non_dynamic_commands_body_before, non_dynamic_commands_rigidbody_mut->angular_torque)
        || physics.addBodyAngularImpulse(non_dynamic_commands_body_before,
                                         non_dynamic_commands_rigidbody_mut->angular_impulse)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync non-dynamic ineligible command fixture observed unexpected API success\n";
        physics.shutdown();
        return false;
    }

    sync.preSimulate(world);
    BodyId non_dynamic_commands_body_second = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_non_dynamic_commands, non_dynamic_commands_body_second)
        || non_dynamic_commands_body_second != non_dynamic_commands_body_before
        || !karma::scene::HasRuntimeLinearImpulseCommand(*non_dynamic_commands_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularImpulseCommand(*non_dynamic_commands_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync non-dynamic ineligible command fixture was not stable across repeated pre-sim passes\n";
        physics.shutdown();
        return false;
    }

    non_dynamic_commands_rigidbody_mut->clear_runtime_commands_requested = true;
    sync.preSimulate(world);
    BodyId non_dynamic_commands_body_after_clear = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_non_dynamic_commands, non_dynamic_commands_body_after_clear)
        || non_dynamic_commands_body_after_clear != non_dynamic_commands_body_before
        || karma::scene::HasRuntimeCommandClearRequest(*non_dynamic_commands_rigidbody_mut)
        || karma::scene::HasRuntimeLinearForceCommand(*non_dynamic_commands_rigidbody_mut)
        || karma::scene::HasRuntimeLinearImpulseCommand(*non_dynamic_commands_rigidbody_mut)
        || karma::scene::HasRuntimeAngularTorqueCommand(*non_dynamic_commands_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*non_dynamic_commands_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync non-dynamic ineligible command clear request did not reconcile deterministically\n";
        physics.shutdown();
        return false;
    }

    non_dynamic_commands_rigidbody_mut->dynamic = true;
    non_dynamic_commands_rigidbody_mut->linear_force = glm::vec3(9.0f, 0.0f, 0.0f);
    non_dynamic_commands_rigidbody_mut->linear_impulse = glm::vec3(0.45f, 0.0f, 0.0f);
    non_dynamic_commands_rigidbody_mut->angular_torque = glm::vec3(0.0f, 7.5f, 0.0f);
    non_dynamic_commands_rigidbody_mut->angular_impulse = glm::vec3(0.0f, 0.35f, 0.0f);
    sync.preSimulate(world);
    BodyId non_dynamic_commands_body_after_recovery = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_non_dynamic_commands, non_dynamic_commands_body_after_recovery)
        || non_dynamic_commands_body_after_recovery == karma::physics::backend::kInvalidBodyId
        || karma::scene::HasRuntimeLinearImpulseCommand(*non_dynamic_commands_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*non_dynamic_commands_rigidbody_mut)
        || !karma::scene::HasRuntimeLinearForceCommand(*non_dynamic_commands_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularTorqueCommand(*non_dynamic_commands_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync non-dynamic ineligible command recovery did not deterministically converge\n";
        physics.shutdown();
        return false;
    }

    const auto entity_ineligible_commands = world.createEntity();
    world.add<TransformComponent>(entity_ineligible_commands, make_transform_component(glm::vec3(-15.0f, 6.0f, 0.0f)));
    RigidBodyIntentComponent ineligible_commands_rigidbody{};
    ineligible_commands_rigidbody.dynamic = true;
    ineligible_commands_rigidbody.kinematic = true;
    ineligible_commands_rigidbody.gravity_enabled = false;
    ineligible_commands_rigidbody.linear_force = glm::vec3(14.0f, 0.0f, 0.0f);
    ineligible_commands_rigidbody.linear_force_enabled = true;
    ineligible_commands_rigidbody.linear_impulse = glm::vec3(0.8f, 0.0f, 0.0f);
    ineligible_commands_rigidbody.angular_torque = glm::vec3(0.0f, 9.0f, 0.0f);
    ineligible_commands_rigidbody.angular_torque_enabled = true;
    ineligible_commands_rigidbody.angular_impulse = glm::vec3(0.0f, 0.6f, 0.0f);
    world.add<RigidBodyIntentComponent>(entity_ineligible_commands, ineligible_commands_rigidbody);
    world.add<ColliderIntentComponent>(entity_ineligible_commands, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_ineligible_commands, PhysicsTransformOwnershipComponent{});

    trace_sink->Clear();
    sync.preSimulate(world);
    auto* ineligible_commands_rigidbody_mut = world.tryGet<RigidBodyIntentComponent>(entity_ineligible_commands);
    BodyId ineligible_commands_body_before = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_ineligible_commands, ineligible_commands_body_before)
        || ineligible_commands_body_before == karma::physics::backend::kInvalidBodyId
        || !ineligible_commands_rigidbody_mut
        || !karma::scene::HasRuntimeLinearForceCommand(*ineligible_commands_rigidbody_mut)
        || !karma::scene::HasRuntimeLinearImpulseCommand(*ineligible_commands_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularTorqueCommand(*ineligible_commands_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularImpulseCommand(*ineligible_commands_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync ineligible command fixture failed to keep runtime body and pending commands stable\n";
        physics.shutdown();
        return false;
    }
    if (!expect_captured_trace(trace_sink,
                               {"stage='create'",
                                "operation='linear_force'",
                                "outcome='ineligible_kinematic'",
                                "operation_precedence='linear_force_first'",
                                "decision_path='skipped_ineligible_kinematic'"},
                               "skip/ineligible emitted trace tags")) {
        physics.shutdown();
        return false;
    }
    if (!expect_decision_path(karma::physics::detail::ClassifyRuntimeCommandTraceDecisionPathTag(
                                  true, true, true, false, false, false),
                              "skipped_ineligible_kinematic",
                              "fixture-skip-kinematic")) {
        physics.shutdown();
        return false;
    }
    if (physics.addBodyForce(ineligible_commands_body_before, ineligible_commands_rigidbody_mut->linear_force)
        || physics.addBodyLinearImpulse(ineligible_commands_body_before,
                                        ineligible_commands_rigidbody_mut->linear_impulse)
        || physics.addBodyTorque(ineligible_commands_body_before, ineligible_commands_rigidbody_mut->angular_torque)
        || physics.addBodyAngularImpulse(ineligible_commands_body_before,
                                         ineligible_commands_rigidbody_mut->angular_impulse)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync kinematic ineligible command fixture observed unexpected API success\n";
        physics.shutdown();
        return false;
    }

    sync.preSimulate(world);
    BodyId ineligible_commands_body_second = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_ineligible_commands, ineligible_commands_body_second)
        || ineligible_commands_body_second != ineligible_commands_body_before
        || !karma::scene::HasRuntimeLinearImpulseCommand(*ineligible_commands_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularImpulseCommand(*ineligible_commands_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync ineligible command fixture was not stable across repeated pre-sim passes\n";
        physics.shutdown();
        return false;
    }

    ineligible_commands_rigidbody_mut->clear_runtime_commands_requested = true;
    sync.preSimulate(world);
    BodyId ineligible_commands_body_after_clear = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_ineligible_commands, ineligible_commands_body_after_clear)
        || ineligible_commands_body_after_clear != ineligible_commands_body_before
        || karma::scene::HasRuntimeCommandClearRequest(*ineligible_commands_rigidbody_mut)
        || karma::scene::HasRuntimeLinearForceCommand(*ineligible_commands_rigidbody_mut)
        || karma::scene::HasRuntimeLinearImpulseCommand(*ineligible_commands_rigidbody_mut)
        || karma::scene::HasRuntimeAngularTorqueCommand(*ineligible_commands_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*ineligible_commands_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync ineligible command clear request did not reconcile deterministically\n";
        physics.shutdown();
        return false;
    }

    ineligible_commands_rigidbody_mut->linear_impulse = glm::vec3(0.55f, 0.0f, 0.0f);
    ineligible_commands_rigidbody_mut->angular_impulse = glm::vec3(0.0f, 0.45f, 0.0f);
    ineligible_commands_rigidbody_mut->kinematic = false;
    trace_sink->Clear();
    sync.preSimulate(world);
    BodyId ineligible_commands_body_after_recovery = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_ineligible_commands, ineligible_commands_body_after_recovery)
        || ineligible_commands_body_after_recovery != ineligible_commands_body_before
        || karma::scene::HasRuntimeLinearImpulseCommand(*ineligible_commands_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*ineligible_commands_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync ineligible command recovery did not consume one-shot commands without churn\n";
        physics.shutdown();
        return false;
    }
    if (!expect_captured_trace(trace_sink,
                               {"stage='update'",
                                "operation='linear_impulse'",
                                "outcome='none'",
                                "operation_precedence='linear_impulse_second'",
                                "decision_path='applied_runtime'"},
                               "success/apply emitted trace tags")) {
        physics.shutdown();
        return false;
    }
    if (!expect_decision_path(karma::physics::detail::ClassifyRuntimeCommandTraceDecisionPathTag(
                                  true, true, false, false, false, false),
                              "applied_runtime",
                              "fixture-applied-runtime")) {
        physics.shutdown();
        return false;
    }

    const auto entity_stale_runtime_commands = world.createEntity();
    world.add<TransformComponent>(entity_stale_runtime_commands, make_transform_component(glm::vec3(-16.0f, 6.0f, 0.0f)));
    RigidBodyIntentComponent stale_runtime_rigidbody{};
    stale_runtime_rigidbody.dynamic = true;
    stale_runtime_rigidbody.gravity_enabled = false;
    world.add<RigidBodyIntentComponent>(entity_stale_runtime_commands, stale_runtime_rigidbody);
    world.add<ColliderIntentComponent>(entity_stale_runtime_commands, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_stale_runtime_commands, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    auto* stale_runtime_rigidbody_mut = world.tryGet<RigidBodyIntentComponent>(entity_stale_runtime_commands);
    BodyId stale_runtime_body_before = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_stale_runtime_commands, stale_runtime_body_before)
        || stale_runtime_body_before == karma::physics::backend::kInvalidBodyId
        || !stale_runtime_rigidbody_mut) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync stale-runtime fixture failed to create initial runtime body\n";
        physics.shutdown();
        return false;
    }

    stale_runtime_rigidbody_mut->linear_force = glm::vec3(10.0f, 0.0f, 0.0f);
    stale_runtime_rigidbody_mut->linear_impulse = glm::vec3(0.6f, 0.0f, 0.0f);
    stale_runtime_rigidbody_mut->angular_torque = glm::vec3(0.0f, 8.0f, 0.0f);
    stale_runtime_rigidbody_mut->angular_impulse = glm::vec3(0.0f, 0.5f, 0.0f);
    stale_runtime_rigidbody_mut->kinematic = true;

    physics.destroyBody(stale_runtime_body_before);
    if (physics.addBodyForce(stale_runtime_body_before, stale_runtime_rigidbody_mut->linear_force)
        || physics.addBodyLinearImpulse(stale_runtime_body_before, stale_runtime_rigidbody_mut->linear_impulse)
        || physics.addBodyTorque(stale_runtime_body_before, stale_runtime_rigidbody_mut->angular_torque)
        || physics.addBodyAngularImpulse(stale_runtime_body_before, stale_runtime_rigidbody_mut->angular_impulse)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync stale-runtime fixture observed unexpected API success for stale body id\n";
        physics.shutdown();
        return false;
    }
    BodyTransform stale_runtime_probe{};
    const bool stale_runtime_binding_body = !physics.getBodyTransform(stale_runtime_body_before, stale_runtime_probe);
    if (std::string_view(karma::physics::detail::ClassifyRuntimeCommandTraceFailureCauseTag(stale_runtime_binding_body, true))
        != "stale_binding") {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync stale-runtime fixture did not classify stale_binding failure cause deterministically\n";
        physics.shutdown();
        return false;
    }

    sync.preSimulate(world);
    BodyId stale_runtime_body_after_failure = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_stale_runtime_commands, stale_runtime_body_after_failure)
        || stale_runtime_body_after_failure == karma::physics::backend::kInvalidBodyId
        || stale_runtime_body_after_failure == stale_runtime_body_before
        || !karma::scene::HasRuntimeLinearForceCommand(*stale_runtime_rigidbody_mut)
        || !karma::scene::HasRuntimeLinearImpulseCommand(*stale_runtime_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularTorqueCommand(*stale_runtime_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularImpulseCommand(*stale_runtime_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync stale-runtime fixture did not preserve command intents across failure/rebuild\n";
        physics.shutdown();
        return false;
    }

    sync.preSimulate(world);
    BodyId stale_runtime_body_second = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_stale_runtime_commands, stale_runtime_body_second)
        || stale_runtime_body_second != stale_runtime_body_after_failure
        || !karma::scene::HasRuntimeLinearImpulseCommand(*stale_runtime_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularImpulseCommand(*stale_runtime_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync stale-runtime fixture was not stable across repeated pre-sim while failure persisted\n";
        physics.shutdown();
        return false;
    }

    stale_runtime_rigidbody_mut->clear_runtime_commands_requested = true;
    sync.preSimulate(world);
    BodyId stale_runtime_body_after_clear = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_stale_runtime_commands, stale_runtime_body_after_clear)
        || stale_runtime_body_after_clear != stale_runtime_body_after_failure
        || karma::scene::HasRuntimeCommandClearRequest(*stale_runtime_rigidbody_mut)
        || karma::scene::HasRuntimeLinearForceCommand(*stale_runtime_rigidbody_mut)
        || karma::scene::HasRuntimeLinearImpulseCommand(*stale_runtime_rigidbody_mut)
        || karma::scene::HasRuntimeAngularTorqueCommand(*stale_runtime_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*stale_runtime_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync stale-runtime fixture clear request did not reconcile deterministically\n";
        physics.shutdown();
        return false;
    }

    stale_runtime_rigidbody_mut->linear_force = glm::vec3(7.5f, 0.0f, 0.0f);
    stale_runtime_rigidbody_mut->linear_impulse = glm::vec3(0.35f, 0.0f, 0.0f);
    stale_runtime_rigidbody_mut->angular_torque = glm::vec3(0.0f, 6.5f, 0.0f);
    stale_runtime_rigidbody_mut->angular_impulse = glm::vec3(0.0f, 0.3f, 0.0f);
    stale_runtime_rigidbody_mut->kinematic = false;
    sync.preSimulate(world);
    BodyId stale_runtime_body_after_recovery = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_stale_runtime_commands, stale_runtime_body_after_recovery)
        || stale_runtime_body_after_recovery == karma::physics::backend::kInvalidBodyId
        || karma::scene::HasRuntimeLinearImpulseCommand(*stale_runtime_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*stale_runtime_rigidbody_mut)
        || !karma::scene::HasRuntimeLinearForceCommand(*stale_runtime_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularTorqueCommand(*stale_runtime_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync stale-runtime fixture recovery did not deterministically converge\n";
        physics.shutdown();
        return false;
    }

    const auto entity_backend_command_failure = world.createEntity();
    world.add<TransformComponent>(entity_backend_command_failure, make_transform_component(glm::vec3(-17.0f, 6.0f, 0.0f)));
    RigidBodyIntentComponent backend_failure_rigidbody{};
    backend_failure_rigidbody.dynamic = true;
    backend_failure_rigidbody.gravity_enabled = false;
    world.add<RigidBodyIntentComponent>(entity_backend_command_failure, backend_failure_rigidbody);
    world.add<ColliderIntentComponent>(entity_backend_command_failure, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_backend_command_failure, PhysicsTransformOwnershipComponent{});

    sync.preSimulate(world);
    BodyId backend_failure_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_backend_command_failure, backend_failure_body)
        || backend_failure_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync backend-failure fixture did not create initial runtime body\n";
        physics.shutdown();
        return false;
    }

    auto* backend_failure_rigidbody_mut = world.tryGet<RigidBodyIntentComponent>(entity_backend_command_failure);
    if (!backend_failure_rigidbody_mut) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync backend-failure fixture missing rigidbody intent component\n";
        physics.shutdown();
        return false;
    }

    backend_failure_rigidbody_mut->linear_force = glm::vec3(8.0f, 0.0f, 0.0f);
    backend_failure_rigidbody_mut->angular_torque = glm::vec3(0.0f, 7.0f, 0.0f);
    backend_failure_rigidbody_mut->linear_impulse = glm::vec3(0.7f, 0.0f, 0.0f);
    backend_failure_rigidbody_mut->angular_impulse = glm::vec3(0.0f, 0.5f, 0.0f);

    if (!physics.setBodyKinematic(backend_failure_body, true)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync backend-failure fixture could not force runtime kinematic state for failure injection\n";
        physics.shutdown();
        return false;
    }
    BodyTransform backend_failure_probe{};
    const bool backend_failure_stale_binding = !physics.getBodyTransform(backend_failure_body, backend_failure_probe);
    if (std::string_view(karma::physics::detail::ClassifyRuntimeCommandTraceFailureCauseTag(backend_failure_stale_binding, true))
        != "backend_reject") {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync backend-failure fixture did not classify backend_reject failure cause deterministically\n";
        physics.shutdown();
        return false;
    }
    if (!expect_decision_path(karma::physics::detail::ClassifyRuntimeCommandTraceDecisionPathTag(
                                  true, true, false, false, true, false),
                              "failed_runtime",
                              "fixture-failed-runtime")) {
        physics.shutdown();
        return false;
    }
    trace_sink->Clear();
    sync.preSimulate(world);
    BodyId backend_failure_body_after_recovery = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_backend_command_failure, backend_failure_body_after_recovery)
        || backend_failure_body_after_recovery == karma::physics::backend::kInvalidBodyId
        || backend_failure_body_after_recovery == backend_failure_body
        || karma::scene::HasRuntimeLinearImpulseCommand(*backend_failure_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*backend_failure_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync backend-failure fixture did not deterministically recover or consume one-shot commands\n";
        physics.shutdown();
        return false;
    }
    if (!karma::scene::HasRuntimeLinearForceCommand(*backend_failure_rigidbody_mut)
        || !karma::scene::HasRuntimeAngularTorqueCommand(*backend_failure_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync backend-failure fixture unexpectedly cleared persistent commands during recovery\n";
        physics.shutdown();
        return false;
    }
    if (!expect_captured_trace(trace_sink,
                               {"stage='update'",
                                "operation='linear_force'",
                                "outcome='runtime_apply_failed'",
                                "cause='backend_reject'",
                                "operation_precedence='linear_force_first'",
                                "cause_precedence='backend_reject_first'",
                                "decision_path='failed_runtime'"},
                               "failure/cause emitted trace tags")) {
        physics.shutdown();
        return false;
    }
    if (!expect_captured_trace(trace_sink,
                               {"stage='recovery'",
                                "operation='linear_force'",
                                "outcome='recovery_applied'",
                                "operation_precedence='linear_force_first'",
                                "decision_path='recovered_via_fallback'"},
                               "recovery emitted trace tags")) {
        physics.shutdown();
        return false;
    }
    if (!expect_decision_path(karma::physics::detail::ClassifyRuntimeCommandTraceDecisionPathTag(
                                  true, true, false, false, false, true),
                              "recovered_via_fallback",
                              "fixture-recovered-via-fallback")) {
        physics.shutdown();
        return false;
    }
    // One-shot commands should still drive deterministic recovery behavior after failed apply on stale runtime.
    glm::vec3 backend_failure_linear_velocity{};
    glm::vec3 backend_failure_angular_velocity{};
    if (!physics.getBodyLinearVelocity(backend_failure_body_after_recovery, backend_failure_linear_velocity)
        || !physics.getBodyAngularVelocity(backend_failure_body_after_recovery, backend_failure_angular_velocity)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync backend-failure fixture failed to read recovered runtime velocities\n";
        physics.shutdown();
        return false;
    }
    physics.beginFrame(1.0f / 60.0f);
    physics.simulateFixedStep(1.0f / 60.0f);
    physics.endFrame();
    glm::vec3 backend_failure_linear_velocity_after_step{};
    glm::vec3 backend_failure_angular_velocity_after_step{};
    if (!physics.getBodyLinearVelocity(backend_failure_body_after_recovery, backend_failure_linear_velocity_after_step)
        || !physics.getBodyAngularVelocity(backend_failure_body_after_recovery, backend_failure_angular_velocity_after_step)
        || (!(backend_failure_linear_velocity.x > 1e-3f)
            && !(backend_failure_linear_velocity_after_step.x > backend_failure_linear_velocity.x + 1e-3f))
        || (!(backend_failure_angular_velocity.y > 1e-3f)
            && !(backend_failure_angular_velocity_after_step.y > backend_failure_angular_velocity.y + 1e-3f))) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync backend-failure fixture did not apply one-shot commands after deterministic recovery\n";
        physics.shutdown();
        return false;
    }

    backend_failure_rigidbody_mut->linear_impulse = glm::vec3(0.0f, 0.0f, 0.0f);
    backend_failure_rigidbody_mut->angular_impulse = glm::vec3(0.0f, 0.0f, 0.0f);
    BodyId backend_failure_body_loop = backend_failure_body_after_recovery;
    for (int attempt = 0; attempt < 2; ++attempt) {
        if (!physics.setBodyKinematic(backend_failure_body_loop, true)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " ecs-sync backend-failure fixture failed to re-arm runtime failure condition\n";
            physics.shutdown();
            return false;
        }
        sync.preSimulate(world);
        BodyId backend_failure_body_next = karma::physics::backend::kInvalidBodyId;
        if (!sync.tryGetRuntimeBody(entity_backend_command_failure, backend_failure_body_next)
            || backend_failure_body_next == karma::physics::backend::kInvalidBodyId
            || backend_failure_body_next == backend_failure_body_loop
            || !karma::scene::HasRuntimeLinearForceCommand(*backend_failure_rigidbody_mut)
            || !karma::scene::HasRuntimeAngularTorqueCommand(*backend_failure_rigidbody_mut)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " ecs-sync backend-failure fixture did not keep persistent commands stable under repeated failures\n";
            physics.shutdown();
            return false;
        }
        backend_failure_body_loop = backend_failure_body_next;
    }

    backend_failure_rigidbody_mut->clear_runtime_commands_requested = true;
    sync.preSimulate(world);
    BodyId backend_failure_body_after_clear = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_backend_command_failure, backend_failure_body_after_clear)
        || backend_failure_body_after_clear != backend_failure_body_loop
        || karma::scene::HasRuntimeCommandClearRequest(*backend_failure_rigidbody_mut)
        || karma::scene::HasRuntimeLinearForceCommand(*backend_failure_rigidbody_mut)
        || karma::scene::HasRuntimeAngularTorqueCommand(*backend_failure_rigidbody_mut)
        || karma::scene::HasRuntimeLinearImpulseCommand(*backend_failure_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*backend_failure_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync backend-failure fixture clear request did not deterministically reconcile command state\n";
        physics.shutdown();
        return false;
    }

    sync.preSimulate(world);
    BodyId backend_failure_body_after_clear_repeat = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_backend_command_failure, backend_failure_body_after_clear_repeat)
        || backend_failure_body_after_clear_repeat != backend_failure_body_after_clear
        || karma::scene::HasRuntimeLinearForceCommand(*backend_failure_rigidbody_mut)
        || karma::scene::HasRuntimeAngularTorqueCommand(*backend_failure_rigidbody_mut)
        || karma::scene::HasRuntimeLinearImpulseCommand(*backend_failure_rigidbody_mut)
        || karma::scene::HasRuntimeAngularImpulseCommand(*backend_failure_rigidbody_mut)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync backend-failure fixture observed stale command side effects after clear reconciliation\n";
        physics.shutdown();
        return false;
    }

    const auto entity_controller = world.createEntity();
    world.add<TransformComponent>(entity_controller, make_transform_component(glm::vec3(3.0f, 6.0f, 0.0f)));
    RigidBodyIntentComponent controller_rigidbody{};
    controller_rigidbody.linear_velocity = glm::vec3(-0.25f, 0.0f, 0.4f);
    controller_rigidbody.angular_velocity = glm::vec3(0.0f, 0.7f, 0.2f);
    world.add<RigidBodyIntentComponent>(entity_controller, controller_rigidbody);
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

    auto* controller_intent = world.tryGet<PlayerControllerIntentComponent>(entity_controller);
    controller_intent->desired_velocity = glm::vec3(1.1f, 0.0f, -0.6f);
    sync.preSimulate(world);
    BodyId controller_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_controller, controller_body)
        || controller_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lost controller body before desired-velocity check\n";
        physics.shutdown();
        return false;
    }
    glm::vec3 controller_runtime_linear_velocity{};
    if (!physics.getBodyLinearVelocity(controller_body, controller_runtime_linear_velocity)
        || !NearlyEqualVec3(controller_runtime_linear_velocity, controller_intent->desired_velocity, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not apply enabled controller desired velocity to runtime body\n";
        physics.shutdown();
        return false;
    }
    glm::vec3 controller_runtime_angular_velocity{};
    if (!physics.getBodyAngularVelocity(controller_body, controller_runtime_angular_velocity)
        || !NearlyEqualVec3(controller_runtime_angular_velocity, glm::vec3(0.0f, 0.0f, 0.0f), 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not enforce zero angular velocity for enabled controller ownership\n";
        physics.shutdown();
        return false;
    }

    const BodyId controller_body_before_noop = controller_body;
    sync.preSimulate(world);
    BodyId controller_body_after_noop = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_controller, controller_body_after_noop)
        || controller_body_after_noop != controller_body_before_noop) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly rebuilt controller body when geometry was unchanged\n";
        physics.shutdown();
        return false;
    }

    BodyTransform preserved_controller_transform{};
    preserved_controller_transform.position = glm::vec3(3.4f, 6.7f, -0.2f);
    preserved_controller_transform.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    const glm::vec3 preserved_controller_linear_velocity(0.9f, 0.0f, -0.45f);
    const glm::vec3 preserved_controller_angular_velocity(0.0f, 0.0f, 0.0f);
    if (!physics.setBodyTransform(controller_body_after_noop, preserved_controller_transform)
        || !physics.setBodyLinearVelocity(controller_body_after_noop, preserved_controller_linear_velocity)
        || !physics.setBodyAngularVelocity(controller_body_after_noop, preserved_controller_angular_velocity)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync controller geometry-rebuild fixture setup failed\n";
        physics.shutdown();
        return false;
    }
    controller_intent->desired_velocity = preserved_controller_linear_velocity;
    auto* controller_collider = world.tryGet<ColliderIntentComponent>(entity_controller);
    controller_intent->half_extents.x += 0.12f;
    sync.preSimulate(world);
    BodyId controller_body_after_half_extents_rebuild = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_controller, controller_body_after_half_extents_rebuild)
        || controller_body_after_half_extents_rebuild == controller_body_after_noop) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not rebuild controller body on controller half-extents mutation\n";
        physics.shutdown();
        return false;
    }
    BodyTransform rebuilt_controller_transform{};
    if (!physics.getBodyTransform(controller_body_after_half_extents_rebuild, rebuilt_controller_transform)
        || !NearlyEqualVec3(rebuilt_controller_transform.position, preserved_controller_transform.position, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not preserve runtime transform across controller geometry rebuild\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyLinearVelocity(controller_body_after_half_extents_rebuild, controller_runtime_linear_velocity)
        || !NearlyEqualVec3(controller_runtime_linear_velocity, preserved_controller_linear_velocity, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not preserve runtime linear velocity across controller geometry rebuild\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyAngularVelocity(controller_body_after_half_extents_rebuild, controller_runtime_angular_velocity)
        || !NearlyEqualVec3(controller_runtime_angular_velocity, preserved_controller_angular_velocity, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not preserve runtime angular velocity across controller geometry rebuild\n";
        physics.shutdown();
        return false;
    }

    sync.preSimulate(world);
    BodyId controller_body_after_second_noop = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_controller, controller_body_after_second_noop)
        || controller_body_after_second_noop != controller_body_after_half_extents_rebuild) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly rebuilt controller body after unchanged geometry update\n";
        physics.shutdown();
        return false;
    }

    BodyTransform before_center_mutation_runtime_transform{};
    if (!physics.getBodyTransform(controller_body_after_second_noop, before_center_mutation_runtime_transform)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to read controller transform before center-offset query check\n";
        physics.shutdown();
        return false;
    }
    const glm::vec3 center_offset_probe_origin_before =
        before_center_mutation_runtime_transform.position + glm::vec3(0.0f, -3.0f, 0.0f);
    RaycastHit center_offset_before_hit{};
    if (!physics.raycastClosest(
            center_offset_probe_origin_before, glm::vec3(0.0f, 1.0f, 0.0f), 10.0f, center_offset_before_hit)
        || center_offset_before_hit.body != controller_body_after_second_noop) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to query controller underside before center mutation\n";
        physics.shutdown();
        return false;
    }

    controller_intent->center.y += 0.18f;
    sync.preSimulate(world);
    BodyId controller_body_after_center_rebuild = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_controller, controller_body_after_center_rebuild)
        || controller_body_after_center_rebuild == controller_body_after_second_noop) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not rebuild controller body on controller center mutation\n";
        physics.shutdown();
        return false;
    }
    BodyTransform after_center_mutation_runtime_transform{};
    if (!physics.getBodyTransform(controller_body_after_center_rebuild, after_center_mutation_runtime_transform)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to read controller transform after center-offset query check\n";
        physics.shutdown();
        return false;
    }
    const glm::vec3 center_offset_probe_origin_after =
        after_center_mutation_runtime_transform.position + glm::vec3(0.0f, -3.0f, 0.0f);
    RaycastHit center_offset_after_hit{};
    if (!physics.raycastClosest(
            center_offset_probe_origin_after, glm::vec3(0.0f, 1.0f, 0.0f), 10.0f, center_offset_after_hit)
        || center_offset_after_hit.body != controller_body_after_center_rebuild) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync failed to query controller underside after center mutation\n";
        physics.shutdown();
        return false;
    }
    if (!(center_offset_after_hit.position.y > center_offset_before_hit.position.y + 0.10f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync controller center mutation did not shift runtime underside via local offset\n";
        physics.shutdown();
        return false;
    }
    controller_body = controller_body_after_center_rebuild;

    auto* controller_rigidbody_intent = world.tryGet<RigidBodyIntentComponent>(entity_controller);
    controller_intent->enabled = false;
    controller_rigidbody_intent->linear_velocity = glm::vec3(0.55f, 0.0f, -0.35f);
    controller_rigidbody_intent->angular_velocity = glm::vec3(0.0f, -0.6f, 0.1f);
    sync.preSimulate(world);
    if (!sync.hasControllerRuntimeBinding(entity_controller)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly removed controller metadata for controller-disabled compatible state\n";
        physics.shutdown();
        return false;
    }
    if (!sync.tryGetRuntimeBody(entity_controller, controller_body)
        || controller_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lost controller body for disabled-controller velocity check\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyLinearVelocity(controller_body, controller_runtime_linear_velocity)
        || !NearlyEqualVec3(controller_runtime_linear_velocity, controller_rigidbody_intent->linear_velocity, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync disabled controller did not restore rigidbody linear velocity ownership\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyAngularVelocity(controller_body, controller_runtime_angular_velocity)
        || !NearlyEqualVec3(controller_runtime_angular_velocity, controller_rigidbody_intent->angular_velocity, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync disabled controller did not restore rigidbody angular velocity ownership\n";
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

    controller_intent->enabled = true;
    controller_intent->desired_velocity = glm::vec3(-1.2f, 0.0f, 0.8f);
    controller_rigidbody_intent->linear_velocity = glm::vec3(0.05f, 0.0f, 0.05f);
    controller_rigidbody_intent->angular_velocity = glm::vec3(0.0f, 0.2f, 0.2f);
    sync.preSimulate(world);
    if (!sync.tryGetRuntimeBody(entity_controller, controller_body)
        || controller_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lost controller body for re-enabled controller ownership check\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyLinearVelocity(controller_body, controller_runtime_linear_velocity)
        || !NearlyEqualVec3(controller_runtime_linear_velocity, controller_intent->desired_velocity, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync re-enabled controller did not reclaim linear velocity ownership\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyAngularVelocity(controller_body, controller_runtime_angular_velocity)
        || !NearlyEqualVec3(controller_runtime_angular_velocity, glm::vec3(0.0f, 0.0f, 0.0f), 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync re-enabled controller did not reclaim angular velocity ownership\n";
        physics.shutdown();
        return false;
    }

    controller_collider = world.tryGet<ColliderIntentComponent>(entity_controller);
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

    const auto entity_controller_static = world.createEntity();
    world.add<TransformComponent>(entity_controller_static, make_transform_component(glm::vec3(3.0f, 8.0f, 0.0f)));
    RigidBodyIntentComponent static_controller_rigidbody{};
    static_controller_rigidbody.dynamic = false;
    world.add<RigidBodyIntentComponent>(entity_controller_static, static_controller_rigidbody);
    world.add<ColliderIntentComponent>(entity_controller_static, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity_controller_static, PhysicsTransformOwnershipComponent{});
    PlayerControllerIntentComponent static_controller_intent{};
    static_controller_intent.enabled = true;
    static_controller_intent.desired_velocity = glm::vec3(2.0f, 0.0f, 0.0f);
    world.add<PlayerControllerIntentComponent>(entity_controller_static, static_controller_intent);

    auto* static_controller_component = world.tryGet<PlayerControllerIntentComponent>(entity_controller_static);
    auto* static_controller_collider = world.tryGet<ColliderIntentComponent>(entity_controller_static);
    auto* static_controller_rigidbody_intent = world.tryGet<RigidBodyIntentComponent>(entity_controller_static);
    if (karma::scene::ClassifyControllerColliderCompatibility(
            *static_controller_component, static_controller_collider, static_controller_rigidbody_intent)
        != ControllerColliderCompatibility::EnabledControllerRequiresDynamicRigidBody) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync controller/non-dynamic compatibility policy mismatch\n";
        physics.shutdown();
        return false;
    }

    sync.preSimulate(world);
    BodyId static_controller_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_controller_static, static_controller_body)
        || static_controller_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not retain runtime body for controller/non-dynamic rejection path\n";
        physics.shutdown();
        return false;
    }
    if (sync.hasControllerRuntimeBinding(entity_controller_static)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync unexpectedly retained controller binding for non-dynamic rigidbody\n";
        physics.shutdown();
        return false;
    }
    for (int i = 0; i < 3; ++i) {
        sync.preSimulate(world);
        BodyId observed_static_controller_body = karma::physics::backend::kInvalidBodyId;
        if (!sync.tryGetRuntimeBody(entity_controller_static, observed_static_controller_body)
            || observed_static_controller_body != static_controller_body
            || sync.hasControllerRuntimeBinding(entity_controller_static)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " ecs-sync controller/non-dynamic rejection path was not stable across repeated pre-sim passes\n";
            physics.shutdown();
            return false;
        }
    }

    world.remove<PlayerControllerIntentComponent>(entity_controller);
    sync.preSimulate(world);
    if (!sync.hasRuntimeBinding(entity_controller) || sync.hasControllerRuntimeBinding(entity_controller)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync controller metadata lifecycle did not update after controller component removal\n";
        physics.shutdown();
        return false;
    }
    if (!sync.tryGetRuntimeBody(entity_controller, controller_body)
        || controller_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync lost controller body after controller component removal\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyLinearVelocity(controller_body, controller_runtime_linear_velocity)
        || !NearlyEqualVec3(controller_runtime_linear_velocity, controller_rigidbody_intent->linear_velocity, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not restore rigidbody linear velocity ownership after controller removal\n";
        physics.shutdown();
        return false;
    }
    if (!physics.getBodyAngularVelocity(controller_body, controller_runtime_angular_velocity)
        || !NearlyEqualVec3(controller_runtime_angular_velocity, controller_rigidbody_intent->angular_velocity, 5e-2f)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " ecs-sync did not restore rigidbody angular velocity ownership after controller removal\n";
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
    BodyId push_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_push, push_body)
        || push_body == karma::physics::backend::kInvalidBodyId) {
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
    BodyId pull_body = karma::physics::backend::kInvalidBodyId;
    if (!sync.tryGetRuntimeBody(entity_pull, pull_body)
        || pull_body == karma::physics::backend::kInvalidBodyId) {
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

bool RunEngineFixedStepSyncValidationChecks(BackendKind backend) {
    using karma::physics::detail::CreateEngineSyncIfPhysicsInitialized;
    using karma::physics::detail::EngineFixedStepEvent;
    using karma::physics::detail::EngineFixedStepObserver;
    using karma::physics::detail::EngineFixedStepPhase;
    using karma::physics::detail::ResetEngineSyncBeforePhysicsShutdown;
    using karma::physics::detail::SimulateFixedStepsWithSync;
    using karma::scene::ColliderIntentComponent;
    using karma::scene::PhysicsTransformOwnershipComponent;
    using karma::scene::RigidBodyIntentComponent;
    using karma::scene::TransformComponent;

    class CapturingObserver final : public EngineFixedStepObserver {
     public:
        void onFixedStepEvent(const EngineFixedStepEvent& event) override {
            events.push_back(event);
        }

        void clear() { events.clear(); }

        std::vector<EngineFixedStepEvent> events{};
    };

    auto make_transform_component = [](const glm::vec3& position) {
        TransformComponent transform{};
        transform.local = glm::mat4(1.0f);
        transform.world = glm::mat4(1.0f);
        transform.local[3] = glm::vec4(position, 1.0f);
        transform.world[3] = glm::vec4(position, 1.0f);
        return transform;
    };

    // Lifecycle invariant: sync object must not exist if physics init did not succeed.
    {
        karma::physics::PhysicsSystem uninitialized_physics{};
        uninitialized_physics.setBackend(backend);
        auto uninitialized_sync = CreateEngineSyncIfPhysicsInitialized(uninitialized_physics);
        if (uninitialized_sync) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " engine fixed-step validation created sync despite failed physics init\n";
            return false;
        }
    }

    karma::physics::PhysicsSystem physics{};
    physics.setBackend(backend);
    physics.init();
    if (!physics.isInitialized()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to initialize (engine fixed-step validation check)\n";
        return false;
    }

    auto sync = CreateEngineSyncIfPhysicsInitialized(physics);
    if (!sync) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " engine fixed-step validation failed to create sync after physics init\n";
        physics.shutdown();
        return false;
    }

    karma::ecs::World world{};
    CapturingObserver observer{};

    // Ordering invariant: zero substeps emits no pre/sim/post events.
    physics.beginFrame(1.0f / 60.0f);
    SimulateFixedStepsWithSync(physics, sync.get(), world, 0, 1.0f / 60.0f, &observer);
    physics.endFrame();
    if (!observer.events.empty()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " engine fixed-step validation emitted events for zero substeps\n";
        physics.shutdown();
        return false;
    }

    // Ordering invariant: each substep is exactly pre -> simulate -> post.
    const int substep_count = 3;
    physics.beginFrame(1.0f / 60.0f);
    SimulateFixedStepsWithSync(physics, sync.get(), world, substep_count, 1.0f / 60.0f, &observer);
    physics.endFrame();
    if (observer.events.size() != static_cast<size_t>(substep_count * 3)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " engine fixed-step validation event count mismatch for ordered substeps\n";
        physics.shutdown();
        return false;
    }
    for (int step = 0; step < substep_count; ++step) {
        const size_t base = static_cast<size_t>(step * 3);
        if (observer.events[base].step_index != step
            || observer.events[base].phase != EngineFixedStepPhase::PreSimulate
            || observer.events[base + 1].step_index != step
            || observer.events[base + 1].phase != EngineFixedStepPhase::Simulate
            || observer.events[base + 2].step_index != step
            || observer.events[base + 2].phase != EngineFixedStepPhase::PostSimulate) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " engine fixed-step validation detected invalid substep ordering\n";
            physics.shutdown();
            return false;
        }
    }

    // Ordering invariant: when sync is absent, only simulate events remain.
    observer.clear();
    physics.beginFrame(1.0f / 60.0f);
    SimulateFixedStepsWithSync(physics, nullptr, world, 2, 1.0f / 60.0f, &observer);
    physics.endFrame();
    if (observer.events.size() != 2u
        || observer.events[0].phase != EngineFixedStepPhase::Simulate
        || observer.events[1].phase != EngineFixedStepPhase::Simulate) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " engine fixed-step validation expected simulate-only events when sync is absent\n";
        physics.shutdown();
        return false;
    }

    // Lifecycle invariant: reset clears runtime state before physics shutdown.
    const auto entity = world.createEntity();
    world.add<TransformComponent>(entity, make_transform_component(glm::vec3(0.0f, 2.0f, 0.0f)));
    world.add<RigidBodyIntentComponent>(entity, RigidBodyIntentComponent{});
    world.add<ColliderIntentComponent>(entity, ColliderIntentComponent{});
    world.add<PhysicsTransformOwnershipComponent>(entity, PhysicsTransformOwnershipComponent{});
    sync->preSimulate(world);
    BodyId runtime_body = karma::physics::backend::kInvalidBodyId;
    if (!sync->tryGetRuntimeBody(entity, runtime_body)
        || runtime_body == karma::physics::backend::kInvalidBodyId) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " engine fixed-step validation failed to create runtime body before lifecycle reset\n";
        physics.shutdown();
        return false;
    }
    BodyTransform runtime_probe{};
    if (!physics.getBodyTransform(runtime_body, runtime_probe)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " engine fixed-step validation failed to read runtime body before lifecycle reset\n";
        physics.shutdown();
        return false;
    }

    ResetEngineSyncBeforePhysicsShutdown(sync);
    if (sync) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " engine fixed-step validation failed to clear sync during lifecycle reset\n";
        physics.shutdown();
        return false;
    }
    if (physics.getBodyTransform(runtime_body, runtime_probe)) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " engine fixed-step validation left stale runtime body after lifecycle reset\n";
        physics.shutdown();
        return false;
    }

    // Reset remains deterministic/idempotent for startup-failure style cleanup paths.
    ResetEngineSyncBeforePhysicsShutdown(sync);

    physics.shutdown();
    return true;
}

bool RunAppEngineSyncLifecycleSmokeChecks(BackendKind backend) {
    using karma::physics::detail::EngineSyncLifecyclePhase;
    using karma::physics::detail::ScopedEngineSyncLifecycleObserver;

    constexpr BackendKind kInvalidBackend = static_cast<BackendKind>(999);
    ScopedClientStartupPrerequisites client_startup_prereqs{};
    if (!client_startup_prereqs.configured()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " failed to configure deterministic client startup prerequisites for lifecycle smoke checks";
        if (!client_startup_prereqs.failureReason().empty()) {
            std::cerr << ": " << client_startup_prereqs.failureReason();
        }
        std::cerr << "\n";
        return false;
    }

    // Server init-failure path: failed physics init must not leave sync active or start the engine.
    {
        EngineSyncLifecycleCapture capture{};
        ScopedEngineSyncLifecycleObserver lifecycle_scope(&capture);

        {
            karma::app::server::Engine engine{};
            ServerEngineSmokeGame game{};
            karma::app::server::EngineConfig config{};
            config.physics_backend = kInvalidBackend;
            config.enable_audio = false;
            engine.start(game, config);
            if (engine.isRunning()) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " server engine unexpectedly running after physics init failure\n";
                return false;
            }
            engine.requestStop();
            engine.tick();

            if (game.start_calls != 0 || game.tick_calls != 0 || game.shutdown_calls != 0) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " server game callbacks fired during init-failure path\n";
                return false;
            }
        }

        if (capture.countPhase(EngineSyncLifecyclePhase::CreateSucceeded) != 0
            || capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != 0) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server init-failure path left unexpected sync lifecycle activity\n";
            return false;
        }
    }

    // Server running-path requestStop()+tick behavior: no extra ticks after stop request, shutdown occurs once on teardown.
    {
        EngineSyncLifecycleCapture capture{};
        ScopedEngineSyncLifecycleObserver lifecycle_scope(&capture);
        ServerEngineSmokeGame game{};

        {
            karma::app::server::Engine engine{};
            karma::app::server::EngineConfig config{};
            config.physics_backend = backend;
            config.enable_audio = false;
            config.target_tick_hz = 1000.0f;
            config.max_substeps = 1;
            engine.start(game, config);
            if (!engine.isRunning()) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " server engine failed to start for shutdown lifecycle smoke check\n";
                return false;
            }

            engine.requestStop();
            engine.tick();
            if (engine.isRunning()) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " server engine still running after first requestStop+tick\n";
                return false;
            }
            if (game.tick_calls != 0 || game.shutdown_calls != 0) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " server requestStop+tick unexpectedly advanced callbacks before teardown\n";
                return false;
            }
            if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != 0) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " server requestStop+tick reset sync before teardown\n";
                return false;
            }

            engine.requestStop();
            engine.tick();
            if (engine.isRunning()) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " server engine still running after repeated requestStop+tick\n";
                return false;
            }
            if (game.tick_calls != 0 || game.shutdown_calls != 0) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " server repeated requestStop+tick unexpectedly advanced callbacks before teardown\n";
                return false;
            }
            if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != 0) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " server repeated requestStop+tick reset sync before teardown\n";
                return false;
            }
        }

        if (game.start_calls != 1 || game.tick_calls != 0 || game.shutdown_calls != 1) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server requestStop+tick callbacks not deterministic\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::CreateSucceeded) != 1) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server requestStop+tick lifecycle missing sync-create event\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != 1) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server requestStop+tick lifecycle missing deterministic sync-reset event\n";
            return false;
        }
        if (!capture.hasResetWhilePhysicsInitialized()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server requestStop+tick lifecycle reset was not observed before physics shutdown\n";
            return false;
        }
        if (!capture.hasCreateBeforeReset()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server requestStop+tick lifecycle create/reset ordering was not deterministic\n";
            return false;
        }
    }

    // Server repeated start/stop lifecycle cycling: each cycle preserves Phase 5g callback semantics and deterministic sync
    // create/reset pairing.
    {
        constexpr int kCycleCount = 2;
        EngineSyncLifecycleCapture capture{};
        ScopedEngineSyncLifecycleObserver lifecycle_scope(&capture);
        int aggregate_start_calls = 0;
        int aggregate_tick_calls = 0;
        int aggregate_shutdown_calls = 0;

        for (int cycle = 0; cycle < kCycleCount; ++cycle) {
            ServerEngineSmokeGame game{};

            {
                karma::app::server::Engine engine{};
                karma::app::server::EngineConfig config{};
                config.physics_backend = backend;
                config.enable_audio = false;
                config.target_tick_hz = 1000.0f;
                config.max_substeps = 1;
                engine.start(game, config);
                if (!engine.isRunning()) {
                    std::cerr << "backend=" << BackendKindName(backend)
                              << " server cycle " << cycle
                              << " failed to reach running state for repeated lifecycle cycling\n";
                    return false;
                }

                engine.requestStop();
                engine.tick();
                if (engine.isRunning()) {
                    std::cerr << "backend=" << BackendKindName(backend)
                              << " server cycle " << cycle
                              << " still running after requestStop+tick in repeated lifecycle cycling\n";
                    return false;
                }
                if (game.tick_calls != 0 || game.shutdown_calls != 0) {
                    std::cerr << "backend=" << BackendKindName(backend)
                              << " server cycle " << cycle
                              << " requestStop+tick advanced callbacks before teardown in repeated lifecycle cycling\n";
                    return false;
                }
                if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != cycle) {
                    std::cerr << "backend=" << BackendKindName(backend)
                              << " server cycle " << cycle
                              << " observed premature sync reset before teardown in repeated lifecycle cycling\n";
                    return false;
                }
            }

            aggregate_start_calls += game.start_calls;
            aggregate_tick_calls += game.tick_calls;
            aggregate_shutdown_calls += game.shutdown_calls;
        }

        if (aggregate_start_calls != kCycleCount || aggregate_tick_calls != 0
            || aggregate_shutdown_calls != kCycleCount) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server repeated lifecycle cycling callbacks were not deterministic\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::CreateSucceeded) != kCycleCount) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server repeated lifecycle cycling missing sync-create events\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != kCycleCount) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server repeated lifecycle cycling missing sync-reset events\n";
            return false;
        }
        if (!capture.hasResetWhilePhysicsInitialized()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server repeated lifecycle cycling reset was not observed before physics shutdown\n";
            return false;
        }
        if (!capture.hasDeterministicCreateResetPairs(kCycleCount)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " server repeated lifecycle cycling create/reset pairing was not deterministic\n";
            return false;
        }
    }

    // Client init-failure path: failed physics init must not leave sync active or start the game loop.
    {
        EngineSyncLifecycleCapture capture{};
        ScopedEngineSyncLifecycleObserver lifecycle_scope(&capture);

        {
            karma::app::client::Engine engine{};
            ClientEngineSmokeGame game{};
            karma::app::client::EngineConfig config{};
            config.enable_audio = false;
            config.physics_backend = kInvalidBackend;
            config.window.title = "phase5c-client-init-failure-smoke";
            config.window.width = 64;
            config.window.height = 64;
            engine.start(game, config);
            if (engine.isRunning()) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client engine unexpectedly running after physics init failure\n";
                return false;
            }
            engine.requestStop();
            engine.tick();

            if (game.start_calls != 0 || game.update_calls != 0 || game.shutdown_calls != 0) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client game callbacks fired during init-failure path\n";
                return false;
            }
        }

        if (capture.countPhase(EngineSyncLifecyclePhase::CreateSucceeded) != 0
            || capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != 0) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client init-failure path left unexpected sync lifecycle activity\n";
            return false;
        }
    }

    // Client normal-shutdown path: sync reset is invoked once before physics shutdown.
    {
        EngineSyncLifecycleCapture capture{};
        ScopedEngineSyncLifecycleObserver lifecycle_scope(&capture);
        ClientEngineSmokeGame game{};

        {
            karma::app::client::Engine engine{};
            karma::app::client::EngineConfig config{};
            config.enable_audio = false;
            config.physics_backend = backend;
            config.render_backend = static_cast<karma::renderer::backend::BackendKind>(999);
            config.window.title = "phase5d-client-shutdown-smoke";
            config.window.width = 64;
            config.window.height = 64;
            engine.start(game, config);
            if (engine.isRunning()) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client engine unexpectedly running for deterministic shutdown lifecycle smoke check\n";
                return false;
            }
            engine.requestStop();
            engine.tick();
        }

        if (game.start_calls != 0 || game.update_calls != 0 || game.shutdown_calls != 0) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client shutdown lifecycle callbacks not deterministic for non-running path\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::CreateSucceeded) != 1) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client shutdown lifecycle missing sync-create event\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != 1) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client shutdown lifecycle missing deterministic sync-reset event\n";
            return false;
        }
        if (!capture.hasResetWhilePhysicsInitialized()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client shutdown lifecycle reset was not observed before physics shutdown\n";
            return false;
        }
        if (!capture.hasCreateBeforeReset()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client shutdown lifecycle create/reset ordering was not deterministic for non-running path\n";
            return false;
        }
    }

    // Client running-path shutdown: engine reaches running state and then resets sync deterministically on shutdown.
    {
        EngineSyncLifecycleCapture capture{};
        ScopedEngineSyncLifecycleObserver lifecycle_scope(&capture);
        ClientEngineSmokeGame game{};

        {
            karma::app::client::Engine engine{};
            karma::app::client::EngineConfig config{};
            config.enable_audio = false;
            config.physics_backend = backend;
            config.window.title = "phase5e-client-running-shutdown-smoke";
            config.window.width = 64;
            config.window.height = 64;
            engine.start(game, config);
            if (!engine.isRunning()) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client engine failed to reach running state for running-path shutdown smoke check\n";
                return false;
            }
        }

        if (game.start_calls != 1 || game.shutdown_calls != 1) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client running-path shutdown callbacks were not deterministic\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::CreateSucceeded) != 1) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client running-path shutdown missing sync-create event\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != 1) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client running-path shutdown missing deterministic sync-reset event\n";
            return false;
        }
        if (!capture.hasResetWhilePhysicsInitialized()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client running-path shutdown reset was not observed before physics shutdown\n";
            return false;
        }
        if (!capture.hasCreateBeforeReset()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client running-path shutdown create/reset ordering was not deterministic\n";
            return false;
        }
    }

    // Client running-path repeated requestStop()+tick attempts remain idempotent for callback/lifecycle semantics.
    {
        EngineSyncLifecycleCapture capture{};
        ScopedEngineSyncLifecycleObserver lifecycle_scope(&capture);
        ClientEngineSmokeGame game{};

        {
            karma::app::client::Engine engine{};
            karma::app::client::EngineConfig config{};
            config.enable_audio = false;
            config.physics_backend = backend;
            config.window.title = "phase5f-client-repeated-stop-idempotence-smoke";
            config.window.width = 64;
            config.window.height = 64;
            engine.start(game, config);
            if (!engine.isRunning()) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client engine failed to reach running state for repeated-stop idempotence smoke check\n";
                return false;
            }

            engine.requestStop();
            engine.tick();
            if (engine.isRunning()) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client engine still running after first requestStop+tick attempt\n";
                return false;
            }
            if (game.update_calls != 0 || game.shutdown_calls != 0) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client requestStop+tick unexpectedly advanced callbacks before teardown\n";
                return false;
            }
            if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != 0) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client requestStop+tick reset sync before teardown\n";
                return false;
            }

            engine.requestStop();
            engine.tick();
            if (engine.isRunning()) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client engine still running after repeated requestStop+tick attempt\n";
                return false;
            }
            if (game.update_calls != 0 || game.shutdown_calls != 0) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client repeated requestStop+tick unexpectedly advanced callbacks before teardown\n";
                return false;
            }
            if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != 0) {
                std::cerr << "backend=" << BackendKindName(backend)
                          << " client repeated requestStop+tick reset sync before teardown\n";
                return false;
            }
        }

        if (game.start_calls != 1 || game.update_calls != 0 || game.shutdown_calls != 0) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client requestStop+tick callbacks were not deterministic\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::CreateSucceeded) != 1) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client requestStop+tick lifecycle missing sync-create event\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != 1) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client requestStop+tick lifecycle missing deterministic sync-reset event\n";
            return false;
        }
        if (!capture.hasResetWhilePhysicsInitialized()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client requestStop+tick lifecycle reset was not observed before physics shutdown\n";
            return false;
        }
        if (!capture.hasCreateBeforeReset()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client requestStop+tick lifecycle create/reset ordering was not deterministic\n";
            return false;
        }
    }

    // Client repeated start/stop lifecycle cycling: each running-path cycle preserves Phase 5g requestStop()+tick callback
    // semantics and deterministic sync lifecycle pairing.
    {
        constexpr int kCycleCount = 2;
        EngineSyncLifecycleCapture capture{};
        ScopedEngineSyncLifecycleObserver lifecycle_scope(&capture);
        int aggregate_start_calls = 0;
        int aggregate_update_calls = 0;
        int aggregate_shutdown_calls = 0;

        for (int cycle = 0; cycle < kCycleCount; ++cycle) {
            ClientEngineSmokeGame game{};

            {
                karma::app::client::Engine engine{};
                karma::app::client::EngineConfig config{};
                config.enable_audio = false;
                config.physics_backend = backend;
                config.window.title = "phase5h-client-start-stop-cycle-" + std::to_string(cycle);
                config.window.width = 64;
                config.window.height = 64;
                engine.start(game, config);
                if (!engine.isRunning()) {
                    std::cerr << "backend=" << BackendKindName(backend)
                              << " client cycle " << cycle
                              << " failed to reach running state for repeated lifecycle cycling\n";
                    return false;
                }

                engine.requestStop();
                engine.tick();
                if (engine.isRunning()) {
                    std::cerr << "backend=" << BackendKindName(backend)
                              << " client cycle " << cycle
                              << " still running after requestStop+tick in repeated lifecycle cycling\n";
                    return false;
                }
                if (game.update_calls != 0 || game.shutdown_calls != 0) {
                    std::cerr << "backend=" << BackendKindName(backend)
                              << " client cycle " << cycle
                              << " requestStop+tick advanced callbacks before teardown in repeated lifecycle cycling\n";
                    return false;
                }
                if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != cycle) {
                    std::cerr << "backend=" << BackendKindName(backend)
                              << " client cycle " << cycle
                              << " observed premature sync reset before teardown in repeated lifecycle cycling\n";
                    return false;
                }
            }

            aggregate_start_calls += game.start_calls;
            aggregate_update_calls += game.update_calls;
            aggregate_shutdown_calls += game.shutdown_calls;
        }

        if (aggregate_start_calls != kCycleCount || aggregate_update_calls != 0 || aggregate_shutdown_calls != 0) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client repeated lifecycle cycling callbacks were not deterministic\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::CreateSucceeded) != kCycleCount) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client repeated lifecycle cycling missing sync-create events\n";
            return false;
        }
        if (capture.countPhase(EngineSyncLifecyclePhase::ResetApplied) != kCycleCount) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client repeated lifecycle cycling missing sync-reset events\n";
            return false;
        }
        if (!capture.hasResetWhilePhysicsInitialized()) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client repeated lifecycle cycling reset was not observed before physics shutdown\n";
            return false;
        }
        if (!capture.hasDeterministicCreateResetPairs(kCycleCount)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " client repeated lifecycle cycling create/reset pairing was not deterministic\n";
            return false;
        }
    }

    return true;
}

bool RunFacadeScaffoldChecks(BackendKind backend) {
    const std::string static_mesh_asset = ResolveTestAssetPath("demo/worlds/r55man-2/world.glb");
    const std::string missing_mesh_asset = ResolveTestAssetPath("demo/worlds/phase4y-missing-static-mesh.glb");
    const std::string non_mesh_asset = ResolveTestAssetPath("README.md");
    auto expect_captured_mesh_trace = [&](const std::shared_ptr<RuntimeTraceCaptureSink>& sink,
                                          std::initializer_list<std::string_view> tokens,
                                          std::string_view label) {
        if (!sink || !sink->ContainsEvent("physics.system", tokens)) {
            std::cerr << "backend=" << BackendKindName(backend)
                      << " world facade trace capture missing expected static-mesh ingest tags for " << label << "\n";
            return false;
        }
        return true;
    };

    auto trace_sink = std::make_shared<RuntimeTraceCaptureSink>();
    auto capture_logger = std::make_shared<spdlog::logger>("physics_backend_parity_world_trace_capture", trace_sink);
    capture_logger->set_level(spdlog::level::trace);
    const auto previous_default_logger = spdlog::default_logger();
    struct TraceLoggerRestoreScope final {
        explicit TraceLoggerRestoreScope(std::shared_ptr<spdlog::logger> previous) : previous_(std::move(previous)) {}
        ~TraceLoggerRestoreScope() {
            if (previous_) {
                spdlog::set_default_logger(previous_);
            }
            spdlog::drop("physics.system");
        }

     private:
        std::shared_ptr<spdlog::logger> previous_{};
    } trace_logger_restore(previous_default_logger);
    spdlog::set_default_logger(capture_logger);
    spdlog::drop("physics.system");
    karma::common::logging::EnableTraceChannels("physics.system");

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

    trace_sink->Clear();
    karma::physics::StaticBody static_mesh = world.createStaticMesh(static_mesh_asset);
    if (!static_mesh.isValid()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade failed to create static mesh body from real mesh asset path\n";
        world.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='create_success'", "cause='none'"},
                                    "create success")) {
        world.shutdown();
        return false;
    }

    trace_sink->Clear();
    karma::physics::StaticBody invalid_intent_static_mesh = world.createStaticMesh("   ");
    if (invalid_intent_static_mesh.isValid()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade unexpectedly created static mesh body for invalid intent path\n";
        world.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='reject'", "cause='invalid_intent'"},
                                    "invalid-intent reject")) {
        world.shutdown();
        return false;
    }

    trace_sink->Clear();
    karma::physics::StaticBody load_failed_static_mesh = world.createStaticMesh(missing_mesh_asset);
    if (load_failed_static_mesh.isValid()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade unexpectedly created static mesh body for missing mesh path\n";
        world.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='reject'",
                                     "cause='mesh_asset_load_or_cook_failed'"},
                                    "mesh-asset load/cook failed reject")) {
        world.shutdown();
        return false;
    }

    trace_sink->Clear();
    karma::physics::StaticBody backend_rejected_static_mesh = world.createStaticMesh(non_mesh_asset);
    if (backend_rejected_static_mesh.isValid()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade unexpectedly created static mesh body for backend reject path\n";
        world.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='reject'", "cause='backend_reject_create'"},
                                    "backend reject create")) {
        world.shutdown();
        return false;
    }

    trace_sink->Clear();
    karma::physics::StaticBody recovered_static_mesh = world.createStaticMesh(static_mesh_asset);
    if (!recovered_static_mesh.isValid()) {
        std::cerr << "backend=" << BackendKindName(backend)
                  << " world facade failed to recover static mesh creation after reject path\n";
        world.shutdown();
        return false;
    }
    if (!expect_captured_mesh_trace(trace_sink,
                                    {"static-mesh-ingest", "outcome='recovery_recreate_success'",
                                     "cause='none'"},
                                    "recovery recreate success after reject correction")) {
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
    recovered_static_mesh.destroy();
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
    if (!RunRuntimeCommandTraceClassificationChecks()) {
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
        if (!RunColliderShapeOffsetQueryChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBodyVelocityApiChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBodyForceImpulseApiChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBodyDampingApiChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBodyMaterialApiChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBodyKinematicApiChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBodyAwakeApiChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunBodyMotionLockApiChecks(backend)) {
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
        if (!RunEngineFixedStepSyncValidationChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunAppEngineSyncLifecycleSmokeChecks(backend)) {
            return EXIT_FAILURE;
        }
        if (!RunRepeatabilityChecks(backend)) {
            return EXIT_FAILURE;
        }
    }

    std::cout << "physics backend parity checks passed\n";
    return EXIT_SUCCESS;
}
