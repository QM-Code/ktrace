#pragma once

#include <cstdint>
#include <string>

#include <glm/vec3.hpp>

#include "karma/ecs/entity.hpp"

namespace bz3::server::domain {

enum class MatchPhase : uint8_t {
    Warmup = 0,
    Live = 1,
    Stopped = 2
};

struct MatchStateComponent {
    MatchPhase phase = MatchPhase::Warmup;
    uint64_t tick = 0;
    float uptime_seconds = 0.0f;
    std::string world_name{};
};

struct SessionComponent {
    uint32_t session_id = 0;
    std::string session_name{};
    bool connected = true;
    float connected_seconds = 0.0f;
};

struct ActorComponent {
    uint32_t actor_id = 0;
    karma::ecs::Entity owner_session{};
    bool alive = true;
    float health = 100.0f;
};

struct ActorMotionComponent {
    glm::vec3 position{0.0f, 0.0f, 0.0f};
    glm::vec3 velocity{0.0f, 0.0f, 0.0f};
};

} // namespace bz3::server::domain
