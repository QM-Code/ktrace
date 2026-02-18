#pragma once

#include "karma/physics/backend.hpp"
#include "karma/physics/physics_system.hpp"

#include <cstdint>
#include <memory>

namespace karma::physics::detail {

struct BodyRuntimeHandle {
    physics::backend::BodyId body = physics::backend::kInvalidBodyId;
    uint64_t generation = 0;
};

struct WorldState {
    std::unique_ptr<PhysicsSystem> owned_system{};
    PhysicsSystem* system = nullptr;
    bool owns_system = false;
    float gravity = -9.8f;
    uint64_t generation = 1;
};

inline PhysicsSystem* ResolveSystem(const std::shared_ptr<WorldState>& state) {
    if (!state) {
        return nullptr;
    }
    return state->system;
}

inline std::shared_ptr<WorldState> LockState(const std::weak_ptr<WorldState>& state) {
    return state.lock();
}

inline bool IsHandleAlive(const std::weak_ptr<WorldState>& state, const BodyRuntimeHandle& handle) {
    const auto shared = LockState(state);
    const auto* system = ResolveSystem(shared);
    if (!shared || !system) {
        return false;
    }
    if (!system->isInitialized()) {
        return false;
    }
    if (handle.body == physics::backend::kInvalidBodyId) {
        return false;
    }
    return handle.generation == shared->generation;
}

inline void InvalidateHandle(BodyRuntimeHandle& handle) {
    handle.body = physics::backend::kInvalidBodyId;
    handle.generation = 0;
}

} // namespace karma::physics::detail
