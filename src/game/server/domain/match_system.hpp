#pragma once

#include <string>

#include "karma/ecs/entity.hpp"

namespace karma::ecs { class World; }

namespace bz3::server::domain {

struct MatchStateComponent;

class MatchSystem {
 public:
    void onStart(karma::ecs::World& world, std::string world_name);
    void onTick(karma::ecs::World& world, float dt);
    void onShutdown(karma::ecs::World& world);

    const MatchStateComponent* tryState(const karma::ecs::World& world) const;

 private:
    karma::ecs::Entity state_entity_{};
};

} // namespace bz3::server::domain
