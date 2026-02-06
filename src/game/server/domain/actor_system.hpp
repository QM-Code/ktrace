#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "karma/ecs/entity.hpp"

namespace karma::ecs { class World; }

namespace bz3::server::domain {

class ActorSystem {
 public:
    void onStart(karma::ecs::World& world);
    void onTick(karma::ecs::World& world, float dt);
    void onShutdown(karma::ecs::World& world);

    karma::ecs::Entity spawnActorForSession(karma::ecs::World& world, karma::ecs::Entity session_entity);
    void destroyActor(karma::ecs::World& world, karma::ecs::Entity actor_entity);
    void destroyActorsForSession(karma::ecs::World& world, karma::ecs::Entity session_entity);

    size_t actorCount(const karma::ecs::World& world) const;
    size_t aliveActorCount(const karma::ecs::World& world) const;

 private:
    std::vector<karma::ecs::Entity> actor_entities_{};
    uint32_t next_actor_id_ = 1;
    float simulation_phase_ = 0.0f;
};

} // namespace bz3::server::domain
