#include "physics/backends/backend_factory_internal.hpp"

#include "karma/common/logging.hpp"

#include <string>
#include <unordered_map>

#if defined(KARMA_HAS_PHYSICS_JOLT)
#include <Jolt/Jolt.h>
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyID.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/EPhysicsUpdateError.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <mutex>
#include <thread>

#include <spdlog/spdlog.h>
#endif

namespace karma::physics_backend {
namespace {

#if defined(KARMA_HAS_PHYSICS_JOLT)

constexpr JPH::ObjectLayer kObjectLayerNonMoving = 0;
constexpr JPH::ObjectLayer kObjectLayerMoving = 1;
constexpr JPH::BroadPhaseLayer kBroadPhaseNonMoving(0);
constexpr JPH::BroadPhaseLayer kBroadPhaseMoving(1);
constexpr uint kBroadPhaseLayerCount = 2;

constexpr uint32_t kMaxBodies = 4096;
constexpr uint32_t kNumBodyMutexes = 0;
constexpr uint32_t kMaxBodyPairs = 65536;
constexpr uint32_t kMaxContactConstraints = 8192;

class BroadPhaseLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface {
 public:
    BroadPhaseLayerInterfaceImpl() {
        object_to_broad_phase_[kObjectLayerNonMoving] = kBroadPhaseNonMoving;
        object_to_broad_phase_[kObjectLayerMoving] = kBroadPhaseMoving;
    }

    uint GetNumBroadPhaseLayers() const override {
        return kBroadPhaseLayerCount;
    }

    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return object_to_broad_phase_[static_cast<size_t>(layer)];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer layer) const override {
        if (layer == kBroadPhaseNonMoving) {
            return "NonMoving";
        }
        if (layer == kBroadPhaseMoving) {
            return "Moving";
        }
        return "Unknown";
    }
#endif

 private:
    JPH::BroadPhaseLayer object_to_broad_phase_[2];
};

class ObjectLayerPairFilterImpl final : public JPH::ObjectLayerPairFilter {
 public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::ObjectLayer) const override {
        return true;
    }
};

class ObjectVsBroadPhaseLayerFilterImpl final : public JPH::ObjectVsBroadPhaseLayerFilter {
 public:
    bool ShouldCollide(JPH::ObjectLayer, JPH::BroadPhaseLayer layer) const override {
        return layer == kBroadPhaseNonMoving || layer == kBroadPhaseMoving;
    }
};

JPH::RVec3 ToJoltRVec3(const glm::vec3& value) {
    return JPH::RVec3(value.x, value.y, value.z);
}

JPH::Vec3 ToJoltVec3(const glm::vec3& value) {
    return JPH::Vec3(value.x, value.y, value.z);
}

JPH::Quat ToJoltQuat(const glm::quat& value) {
    return JPH::Quat(value.x, value.y, value.z, value.w);
}

glm::vec3 ToGlmVec3(const JPH::RVec3& value) {
    return glm::vec3(static_cast<float>(value.GetX()),
                     static_cast<float>(value.GetY()),
                     static_cast<float>(value.GetZ()));
}

glm::quat ToGlmQuat(const JPH::Quat& value) {
    return glm::quat(value.GetW(), value.GetX(), value.GetY(), value.GetZ());
}

void JoltTrace(const char* fmt, ...) {
    if (!karma::logging::ShouldTraceChannel("physics.jolt")) {
        return;
    }

    va_list list;
    va_start(list, fmt);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), fmt, list);
    va_end(list);
    KARMA_TRACE("physics.jolt", "{}", buffer);
}

std::mutex g_jolt_runtime_mutex;
int g_jolt_runtime_users = 0;

bool AcquireJoltRuntime() {
    std::lock_guard<std::mutex> lock(g_jolt_runtime_mutex);
    if (g_jolt_runtime_users == 0) {
        JPH::RegisterDefaultAllocator();
        JPH::Trace = &JoltTrace;
        JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = [](const char* expr, const char* msg, const char* file, uint line) {
            spdlog::error("Jolt assert failed: {} {} ({}:{})", expr, msg ? msg : "", file, line);
            return true;
        });

        JPH::Factory::sInstance = new JPH::Factory();
        JPH::RegisterTypes();
    }
    ++g_jolt_runtime_users;
    return true;
}

void ReleaseJoltRuntime() {
    std::lock_guard<std::mutex> lock(g_jolt_runtime_mutex);
    if (g_jolt_runtime_users <= 0) {
        return;
    }

    --g_jolt_runtime_users;
    if (g_jolt_runtime_users == 0) {
        JPH::UnregisterTypes();
        delete JPH::Factory::sInstance;
        JPH::Factory::sInstance = nullptr;
    }
}

class JoltBackend final : public Backend {
 public:
    const char* name() const override {
        return "jolt";
    }

    bool init() override {
        if (initialized_) {
            return true;
        }

        if (!AcquireJoltRuntime()) {
            return false;
        }
        runtime_acquired_ = true;

        temp_allocator_ = std::make_unique<JPH::TempAllocatorImpl>(32u * 1024u * 1024u);
        const uint32_t worker_count = std::max(1u, std::thread::hardware_concurrency());
        job_system_ = std::make_unique<JPH::JobSystemThreadPool>(
            JPH::cMaxPhysicsJobs,
            JPH::cMaxPhysicsBarriers,
            static_cast<int>(worker_count));

        static BroadPhaseLayerInterfaceImpl broad_phase_layers;
        static ObjectVsBroadPhaseLayerFilterImpl object_vs_broad_phase_filter;
        static ObjectLayerPairFilterImpl object_layer_pair_filter;

        physics_system_ = std::make_unique<JPH::PhysicsSystem>();
        physics_system_->Init(
            kMaxBodies,
            kNumBodyMutexes,
            kMaxBodyPairs,
            kMaxContactConstraints,
            broad_phase_layers,
            object_vs_broad_phase_filter,
            object_layer_pair_filter);
        physics_system_->SetGravity(JPH::Vec3(0.0f, -9.8f, 0.0f));

        initialized_ = true;
        KARMA_TRACE("physics.jolt", "PhysicsBackend[jolt]: initialized");
        return true;
    }

    void shutdown() override {
        if (physics_system_) {
            JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
            for (const auto& [id, body_id] : bodies_) {
                (void)id;
                if (body_interface.IsAdded(body_id)) {
                    body_interface.RemoveBody(body_id);
                }
                body_interface.DestroyBody(body_id);
            }
            bodies_.clear();
        }

        physics_system_.reset();
        job_system_.reset();
        temp_allocator_.reset();
        initialized_ = false;

        if (runtime_acquired_) {
            ReleaseJoltRuntime();
            runtime_acquired_ = false;
        }
        KARMA_TRACE("physics.jolt", "PhysicsBackend[jolt]: shutdown");
    }

    void beginFrame(float dt) override {
        frame_dt_ = dt;
    }

    void simulateFixedStep(float fixed_dt) override {
        if (!physics_system_ || fixed_dt <= 0.0f) {
            return;
        }

        const JPH::EPhysicsUpdateError update_error =
            physics_system_->Update(fixed_dt, 1, temp_allocator_.get(), job_system_.get());
        if (update_error != JPH::EPhysicsUpdateError::None) {
            KARMA_TRACE("physics.jolt",
                        "PhysicsBackend[jolt]: update warning dt={:.4f} err=0x{:x}",
                        fixed_dt,
                        static_cast<uint32_t>(update_error));
        }
    }

    void endFrame() override {
        (void)frame_dt_;
        KARMA_TRACE_CHANGED("physics.jolt",
                            std::to_string(bodies_.size()),
                            "PhysicsBackend[jolt]: active bodies={}",
                            bodies_.size());
    }

    BodyId createBody(const BodyDesc& desc) override {
        if (!physics_system_) {
            return kInvalidBodyId;
        }

        const bool is_dynamic = !desc.is_static;
        JPH::RefConst<JPH::Shape> shape = new JPH::BoxShape(JPH::Vec3(0.5f, 0.5f, 0.5f));
        JPH::BodyCreationSettings settings(
            shape,
            ToJoltRVec3(desc.transform.position),
            ToJoltQuat(desc.transform.rotation),
            is_dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static,
            is_dynamic ? kObjectLayerMoving : kObjectLayerNonMoving);
        settings.mLinearVelocity = ToJoltVec3(desc.linear_velocity);
        settings.mAngularVelocity = ToJoltVec3(desc.angular_velocity);
        if (is_dynamic && desc.mass > 0.0f) {
            settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
            settings.mMassPropertiesOverride.mMass = desc.mass;
        }

        JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
        JPH::Body* body = body_interface.CreateBody(settings);
        if (!body) {
            KARMA_TRACE("physics.jolt", "PhysicsBackend[jolt]: failed to create body");
            return kInvalidBodyId;
        }

        body_interface.AddBody(
            body->GetID(),
            is_dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);

        const BodyId id = next_body_id_++;
        bodies_.emplace(id, body->GetID());
        return id;
    }

    void destroyBody(BodyId body) override {
        if (!physics_system_) {
            return;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return;
        }

        JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
        if (body_interface.IsAdded(it->second)) {
            body_interface.RemoveBody(it->second);
        }
        body_interface.DestroyBody(it->second);
        bodies_.erase(it);
    }

    bool setBodyTransform(BodyId body, const BodyTransform& transform) override {
        if (!physics_system_) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }

        JPH::BodyInterface& body_interface = physics_system_->GetBodyInterface();
        body_interface.SetPositionAndRotation(
            it->second,
            ToJoltRVec3(transform.position),
            ToJoltQuat(transform.rotation),
            JPH::EActivation::Activate);
        return true;
    }

    bool getBodyTransform(BodyId body, BodyTransform& out_transform) const override {
        if (!physics_system_) {
            return false;
        }

        const auto it = bodies_.find(body);
        if (it == bodies_.end()) {
            return false;
        }

        JPH::BodyLockRead lock(physics_system_->GetBodyLockInterface(), it->second);
        if (!lock.Succeeded()) {
            return false;
        }

        const JPH::Body& physics_body = lock.GetBody();
        out_transform.position = ToGlmVec3(physics_body.GetPosition());
        out_transform.rotation = ToGlmQuat(physics_body.GetRotation());
        return true;
    }

    bool raycastClosest(const glm::vec3& origin,
                        const glm::vec3& direction,
                        float max_distance,
                        RaycastHit& out_hit) const override {
        if (!physics_system_ || max_distance <= 0.0f) {
            return false;
        }

        const float direction_length = glm::length(direction);
        if (!std::isfinite(direction_length) || direction_length <= 1e-6f) {
            return false;
        }

        const glm::vec3 unit_direction = direction / direction_length;
        const JPH::RRayCast ray(ToJoltRVec3(origin), ToJoltVec3(unit_direction * max_distance));
        JPH::RayCastResult result;
        if (!physics_system_->GetNarrowPhaseQuery().CastRay(ray, result)) {
            return false;
        }

        const BodyId hit_body = findBodyId(result.mBodyID);
        if (hit_body == kInvalidBodyId) {
            return false;
        }

        out_hit.body = hit_body;
        out_hit.fraction = result.mFraction;
        out_hit.distance = max_distance * result.mFraction;
        out_hit.position = ToGlmVec3(ray.GetPointOnRay(result.mFraction));
        return true;
    }

 private:
    BodyId findBodyId(JPH::BodyID body_id) const {
        for (const auto& [id, internal_id] : bodies_) {
            if (internal_id == body_id) {
                return id;
            }
        }
        return kInvalidBodyId;
    }

    std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator_{};
    std::unique_ptr<JPH::JobSystemThreadPool> job_system_{};
    std::unique_ptr<JPH::PhysicsSystem> physics_system_{};
    std::unordered_map<BodyId, JPH::BodyID> bodies_{};
    BodyId next_body_id_ = 1;
    float frame_dt_ = 0.0f;
    bool initialized_ = false;
    bool runtime_acquired_ = false;
};

#else

class JoltBackendStub final : public Backend {
 public:
    const char* name() const override { return "jolt"; }
    bool init() override {
        KARMA_TRACE("physics.jolt", "PhysicsBackend[jolt]: unavailable (not compiled)");
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

std::unique_ptr<Backend> CreateJoltBackend() {
#if defined(KARMA_HAS_PHYSICS_JOLT)
    return std::make_unique<JoltBackend>();
#else
    return std::make_unique<JoltBackendStub>();
#endif
}

} // namespace karma::physics_backend
