#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "karma/app/server_game_interface.hpp"

#include "server/domain/actor_system.hpp"
#include "server/domain/match_system.hpp"
#include "server/domain/session_system.hpp"

namespace bz3::server {

struct SessionSnapshot {
    uint32_t session_id = 0;
    std::string session_name{};
};

class ServerGame final : public karma::app::ServerGameInterface {
 public:
    explicit ServerGame(std::string worldName);

    std::optional<uint32_t> connectSession(std::string_view session_name);
    bool disconnectSession(uint32_t session_id);
    bool onClientJoin(uint32_t client_id, std::string_view player_name);
    bool onClientLeave(uint32_t client_id);
    const std::string& lastJoinRejectReason() const;
    std::vector<SessionSnapshot> activeSessionSnapshot() const;

    void onStart() override;
    void onTick(float dt) override;
    void onShutdown() override;

 private:
    bool hasActiveSessionName(std::string_view player_name) const;

    std::string world_name_;
    domain::MatchSystem match_system_{};
    domain::SessionSystem session_system_{};
    domain::ActorSystem actor_system_{};
    std::unordered_map<uint32_t, uint32_t> session_by_client_id_{};
    std::string last_join_reject_reason_{};
    float status_log_accumulator_ = 0.0f;
};

} // namespace bz3::server
