#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

#include "karma/ecs/entity.hpp"

namespace karma::ecs { class World; }

namespace bz3::server::domain {

class SessionSystem {
 public:
    void onStart(karma::ecs::World& world);
    void onTick(karma::ecs::World& world, float dt);
    void onShutdown(karma::ecs::World& world);

    karma::ecs::Entity createSession(karma::ecs::World& world, std::string_view session_name);
    void disconnectSession(karma::ecs::World& world, karma::ecs::Entity session_entity);
    void destroySession(karma::ecs::World& world, karma::ecs::Entity session_entity);
    std::optional<karma::ecs::Entity> findSessionById(const karma::ecs::World& world, uint32_t session_id) const;

    size_t sessionCount(const karma::ecs::World& world) const;
    std::vector<karma::ecs::Entity> activeSessions(const karma::ecs::World& world) const;
    size_t activeSessionCount(const karma::ecs::World& world) const;

 private:
    std::vector<karma::ecs::Entity> session_entities_{};
    uint32_t next_session_id_ = 1;
};

} // namespace bz3::server::domain
