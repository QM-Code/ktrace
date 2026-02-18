#pragma once

#include "karma/ecs/world.hpp"
#include "karma/physics/physics_system.hpp"
#include "physics/sync/ecs_sync_system.hpp"

#include <memory>
#include <vector>

namespace karma::physics::detail {

enum class EngineFixedStepPhase {
    PreSimulate,
    Simulate,
    PostSimulate
};

enum class EngineSyncLifecyclePhase {
    CreateSkippedPhysicsUninitialized,
    CreateSucceeded,
    ResetSkippedNoSync,
    ResetApplied
};

struct EngineFixedStepEvent {
    int step_index = 0;
    EngineFixedStepPhase phase = EngineFixedStepPhase::Simulate;
};

struct EngineSyncLifecycleEvent {
    EngineSyncLifecyclePhase phase = EngineSyncLifecyclePhase::CreateSkippedPhysicsUninitialized;
    bool physics_initialized = false;
};

class EngineFixedStepObserver {
 public:
    virtual ~EngineFixedStepObserver() = default;
    virtual void onFixedStepEvent(const EngineFixedStepEvent& event) = 0;
};

class EngineSyncLifecycleObserver {
 public:
    virtual ~EngineSyncLifecycleObserver() = default;
    virtual void onEngineSyncLifecycleEvent(const EngineSyncLifecycleEvent& event) = 0;
};

inline EngineSyncLifecycleObserver*& ActiveEngineSyncLifecycleObserver() {
    static thread_local EngineSyncLifecycleObserver* observer = nullptr;
    return observer;
}

class ScopedEngineSyncLifecycleObserver {
 public:
    explicit ScopedEngineSyncLifecycleObserver(EngineSyncLifecycleObserver* observer)
        : previous_(ActiveEngineSyncLifecycleObserver()) {
        ActiveEngineSyncLifecycleObserver() = observer;
    }

    ~ScopedEngineSyncLifecycleObserver() {
        ActiveEngineSyncLifecycleObserver() = previous_;
    }

 private:
    EngineSyncLifecycleObserver* previous_ = nullptr;
};

inline void RecordFixedStepEvent(EngineFixedStepObserver* observer, int step_index, EngineFixedStepPhase phase) {
    if (!observer) {
        return;
    }
    observer->onFixedStepEvent(EngineFixedStepEvent{step_index, phase});
}

inline void RecordEngineSyncLifecycleEvent(EngineSyncLifecyclePhase phase, bool physics_initialized) {
    auto* observer = ActiveEngineSyncLifecycleObserver();
    if (!observer) {
        return;
    }
    observer->onEngineSyncLifecycleEvent(EngineSyncLifecycleEvent{phase, physics_initialized});
}

inline std::unique_ptr<EcsSyncSystem> CreateEngineSyncIfPhysicsInitialized(PhysicsSystem& physics_system) {
    const bool physics_initialized = physics_system.isInitialized();
    if (!physics_initialized) {
        RecordEngineSyncLifecycleEvent(EngineSyncLifecyclePhase::CreateSkippedPhysicsUninitialized, false);
        return {};
    }
    RecordEngineSyncLifecycleEvent(EngineSyncLifecyclePhase::CreateSucceeded, true);
    return std::make_unique<EcsSyncSystem>(physics_system);
}

inline void ResetEngineSyncBeforePhysicsShutdown(std::unique_ptr<EcsSyncSystem>& sync_system,
                                                 const PhysicsSystem* physics_system = nullptr) {
    const bool physics_initialized = physics_system ? physics_system->isInitialized() : false;
    if (!sync_system) {
        RecordEngineSyncLifecycleEvent(EngineSyncLifecyclePhase::ResetSkippedNoSync, physics_initialized);
        return;
    }
    sync_system->clear();
    sync_system.reset();
    RecordEngineSyncLifecycleEvent(EngineSyncLifecyclePhase::ResetApplied, physics_initialized);
}

inline void SimulateFixedStepsWithSync(PhysicsSystem& physics_system,
                                       EcsSyncSystem* sync_system,
                                       ecs::World& world,
                                       int substep_count,
                                       float fixed_dt,
                                       EngineFixedStepObserver* observer = nullptr) {
    if (substep_count <= 0) {
        return;
    }

    for (int step = 0; step < substep_count; ++step) {
        if (sync_system) {
            RecordFixedStepEvent(observer, step, EngineFixedStepPhase::PreSimulate);
            sync_system->preSimulate(world);
        }

        RecordFixedStepEvent(observer, step, EngineFixedStepPhase::Simulate);
        physics_system.simulateFixedStep(fixed_dt);

        if (sync_system) {
            RecordFixedStepEvent(observer, step, EngineFixedStepPhase::PostSimulate);
            sync_system->postSimulate(world);
        }
    }
}

} // namespace karma::physics::detail
