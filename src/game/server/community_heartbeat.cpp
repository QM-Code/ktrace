#include "server/community_heartbeat.hpp"

#include "server/heartbeat_client.hpp"
#include "server/server_game.hpp"

#include "karma/common/config_helpers.hpp"

#include <spdlog/spdlog.h>

#include <chrono>
#include <string>
#include <utility>

namespace bz3::server {

CommunityHeartbeat::CommunityHeartbeat()
    : CommunityHeartbeat(std::make_unique<HeartbeatClient>()) {}

CommunityHeartbeat::CommunityHeartbeat(std::unique_ptr<IHeartbeatClient> client)
    : client_(std::move(client)) {
    if (!client_) {
        client_ = std::make_unique<HeartbeatClient>();
    }
}

CommunityHeartbeat::~CommunityHeartbeat() = default;

void CommunityHeartbeat::configureFromConfig(const karma::json::Value& merged_config,
                                             uint16_t listen_port,
                                             const std::string& community_override) {
    std::string advertise_host = karma::config::ReadStringConfig("network.ServerAdvertiseHost", "");
    std::string server_host = karma::config::ReadStringConfig("network.ServerHost", "");
    if (advertise_host.empty() || advertise_host == "0.0.0.0") {
        advertise_host = server_host;
    }
    if (advertise_host == "0.0.0.0") {
        advertise_host.clear();
    }
    if (advertise_host.empty()) {
        server_address_ = std::to_string(listen_port);
        spdlog::warn("Community heartbeat will omit host; set network.ServerAdvertiseHost to advertise a host.");
    } else {
        server_address_ = advertise_host + ":" + std::to_string(listen_port);
    }

    max_players_ = 0;
    if (auto max_it = merged_config.find("maxPlayers");
        max_it != merged_config.end()) {
        if (max_it->is_number_integer()) {
            max_players_ = max_it->get<int>();
        } else if (max_it->is_string()) {
            try {
                max_players_ = std::stoi(max_it->get<std::string>());
            } catch (...) {
                max_players_ = 0;
            }
        }
    }

    community_url_.clear();
    enabled_ = false;
    interval_seconds_ = 0;

    if (auto it = merged_config.find("community");
        it != merged_config.end() && it->is_object()) {
        const auto& community = *it;
        if (auto server_it = community.find("server");
            server_it != community.end() && server_it->is_string()) {
            community_url_ = server_it->get<std::string>();
        }
        if (auto enabled_it = community.find("enabled");
            enabled_it != community.end() && enabled_it->is_boolean()) {
            enabled_ = enabled_it->get<bool>();
        } else if (!community_url_.empty()) {
            enabled_ = true;
        }
        if (auto interval_it = community.find("heartbeatIntervalSeconds");
            interval_it != community.end()) {
            if (interval_it->is_number_integer()) {
                interval_seconds_ = interval_it->get<int>();
            } else if (interval_it->is_string()) {
                try {
                    interval_seconds_ = std::stoi(interval_it->get<std::string>());
                } catch (...) {
                    interval_seconds_ = 0;
                }
            }
        }
    }

    if (!community_override.empty()) {
        community_url_ = community_override;
        enabled_ = true;
    }

    if (!community_url_.empty()
        && community_url_.rfind("http://", 0) != 0
        && community_url_.rfind("https://", 0) != 0) {
        community_url_ = "http://" + community_url_;
    }

    if (community_url_.empty()) {
        enabled_ = false;
    }

    next_heartbeat_time_ = std::chrono::steady_clock::time_point{};
}

void CommunityHeartbeat::update(size_t connected_client_count) {
    if (!enabled_ || interval_seconds_ <= 0 || community_url_.empty()) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    if (next_heartbeat_time_ == std::chrono::steady_clock::time_point{}) {
        next_heartbeat_time_ = now;
    }
    if (now < next_heartbeat_time_) {
        return;
    }

    const int player_count = static_cast<int>(connected_client_count);
    client_->requestHeartbeat(community_url_, server_address_, player_count, max_players_);
    next_heartbeat_time_ = now + std::chrono::seconds(interval_seconds_);
}

bool CommunityHeartbeat::enabled() const {
    return enabled_;
}

int CommunityHeartbeat::intervalSeconds() const {
    return interval_seconds_;
}

const std::string& CommunityHeartbeat::communityUrl() const {
    return community_url_;
}

const std::string& CommunityHeartbeat::serverAddress() const {
    return server_address_;
}

int CommunityHeartbeat::maxPlayers() const {
    return max_players_;
}

} // namespace bz3::server
