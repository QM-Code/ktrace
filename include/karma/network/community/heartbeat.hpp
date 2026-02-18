#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "karma/common/serialization/json.hpp"

namespace karma::network {

class IHeartbeatClient;

class CommunityHeartbeat {
 public:
    CommunityHeartbeat();
    explicit CommunityHeartbeat(std::unique_ptr<IHeartbeatClient> client);
    ~CommunityHeartbeat();

    void configureFromConfig(const karma::common::serialization::Value& merged_config,
                             uint16_t listen_port,
                             const std::string& community_override);
    void update(size_t connected_client_count);

    bool enabled() const;
    int intervalSeconds() const;
    const std::string& communityUrl() const;
    const std::string& serverAddress() const;
    int maxPlayers() const;

 private:
    std::unique_ptr<IHeartbeatClient> client_{};
    bool enabled_ = false;
    int interval_seconds_ = 0;
    std::string community_url_{};
    std::string server_address_{};
    int max_players_ = 0;
    std::chrono::steady_clock::time_point next_heartbeat_time_{};
};

} // namespace karma::network
