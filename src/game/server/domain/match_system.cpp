#include "server/domain/match_system.hpp"

#include <utility>

#include "karma/ecs/world.hpp"

#include "server/domain/components.hpp"

namespace bz3::server::domain {

void MatchSystem::onStart(karma::ecs::World& world, std::string world_name) {
    if (state_entity_.isValid() && world.isAlive(state_entity_)) {
        world.destroyEntity(state_entity_);
    }

    state_entity_ = world.createEntity();
    MatchStateComponent state{};
    state.phase = MatchPhase::Live;
    state.tick = 0;
    state.uptime_seconds = 0.0f;
    state.world_name = std::move(world_name);
    world.add(state_entity_, std::move(state));
}

void MatchSystem::onTick(karma::ecs::World& world, float dt) {
    MatchStateComponent* state = world.tryGet<MatchStateComponent>(state_entity_);
    if (!state) {
        return;
    }

    state->tick += 1;
    state->uptime_seconds += dt;
}

void MatchSystem::onShutdown(karma::ecs::World& world) {
    MatchStateComponent* state = world.tryGet<MatchStateComponent>(state_entity_);
    if (state) {
        state->phase = MatchPhase::Stopped;
    }

    if (state_entity_.isValid() && world.isAlive(state_entity_)) {
        world.destroyEntity(state_entity_);
    }
    state_entity_ = {};
}

const MatchStateComponent* MatchSystem::tryState(const karma::ecs::World& world) const {
    if (!state_entity_.isValid()) {
        return nullptr;
    }
    return world.tryGet<MatchStateComponent>(state_entity_);
}

} // namespace bz3::server::domain
