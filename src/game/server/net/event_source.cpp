#include "server/net/event_source.hpp"
#include "server/net/enet_event_source.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/logging.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include <spdlog/spdlog.h>

namespace bz3::server::net {

namespace {

struct ScheduledEvent {
    double at_seconds = 0.0;
    ServerInputEvent event{};
};

class NullServerEventSource final : public ServerEventSource {
 public:
    std::vector<ServerInputEvent> poll() override {
        return {};
    }
};

class ScriptedServerEventSource final : public ServerEventSource {
 public:
    explicit ScriptedServerEventSource(std::vector<ScheduledEvent> schedule)
        : schedule_(std::move(schedule)),
          start_time_(std::chrono::steady_clock::now()) {}

    std::vector<ServerInputEvent> poll() override {
        std::vector<ServerInputEvent> ready;
        const double elapsed_seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time_).count();

        while (next_index_ < schedule_.size() && schedule_[next_index_].at_seconds <= elapsed_seconds) {
            const auto& scheduled = schedule_[next_index_];
            if (scheduled.event.type == ServerInputEvent::Type::ClientJoin) {
                KARMA_TRACE("engine.server",
                            "ServerEventSource: scripted join client_id={} name='{}' at={:.2f}s",
                            scheduled.event.join.client_id,
                            scheduled.event.join.player_name,
                            scheduled.at_seconds);
            } else {
                KARMA_TRACE("engine.server",
                            "ServerEventSource: scripted leave client_id={} at={:.2f}s",
                            scheduled.event.leave.client_id,
                            scheduled.at_seconds);
            }
            ready.push_back(scheduled.event);
            ++next_index_;
        }

        return ready;
    }

 private:
    std::vector<ScheduledEvent> schedule_{};
    std::chrono::steady_clock::time_point start_time_{};
    size_t next_index_ = 0;
};

bool ReadDouble(const karma::json::Value& value, const char* key, double* out) {
    const auto it = value.find(key);
    if (it == value.end()) {
        return false;
    }
    if (it->is_number_float()) {
        *out = it->get<double>();
        return true;
    }
    if (it->is_number_integer()) {
        *out = static_cast<double>(it->get<long long>());
        return true;
    }
    return false;
}

bool ReadUint32(const karma::json::Value& value, const char* key, uint32_t* out) {
    const auto it = value.find(key);
    if (it == value.end()) {
        return false;
    }
    if (it->is_number_unsigned()) {
        *out = static_cast<uint32_t>(it->get<unsigned long long>());
        return true;
    }
    if (it->is_number_integer()) {
        const auto raw = it->get<long long>();
        if (raw < 0) {
            return false;
        }
        *out = static_cast<uint32_t>(raw);
        return true;
    }
    return false;
}

std::vector<ScheduledEvent> LoadScriptedEventsFromConfig() {
    std::vector<ScheduledEvent> schedule;
    const auto* events_value = karma::config::ConfigStore::Get("server.startupEvents");
    if (!events_value) {
        return schedule;
    }
    if (!events_value->is_array()) {
        spdlog::warn("server.startupEvents must be an array; ignoring scripted events");
        return schedule;
    }

    for (const auto& item : *events_value) {
        if (!item.is_object()) {
            spdlog::warn("server.startupEvents item must be an object; skipping");
            continue;
        }

        const auto type_it = item.find("type");
        if (type_it == item.end() || !type_it->is_string()) {
            spdlog::warn("server.startupEvents item missing string field 'type'; skipping");
            continue;
        }

        ScheduledEvent scheduled{};
        ReadDouble(item, "atSeconds", &scheduled.at_seconds);
        if (scheduled.at_seconds < 0.0) {
            scheduled.at_seconds = 0.0;
        }

        const std::string type = type_it->get<std::string>();
        if (type == "join") {
            scheduled.event.type = ServerInputEvent::Type::ClientJoin;
            if (!ReadUint32(item, "clientId", &scheduled.event.join.client_id)) {
                spdlog::warn("server.startupEvents join item missing numeric 'clientId'; skipping");
                continue;
            }
            const auto name_it = item.find("playerName");
            if (name_it != item.end() && name_it->is_string()) {
                scheduled.event.join.player_name = name_it->get<std::string>();
            } else {
                scheduled.event.join.player_name =
                    "player-" + std::to_string(scheduled.event.join.client_id);
            }
        } else if (type == "leave") {
            scheduled.event.type = ServerInputEvent::Type::ClientLeave;
            if (!ReadUint32(item, "clientId", &scheduled.event.leave.client_id)) {
                spdlog::warn("server.startupEvents leave item missing numeric 'clientId'; skipping");
                continue;
            }
        } else {
            spdlog::warn("server.startupEvents item has unknown type '{}'; expected 'join' or 'leave'", type);
            continue;
        }

        schedule.push_back(std::move(scheduled));
    }

    std::sort(schedule.begin(),
              schedule.end(),
              [](const ScheduledEvent& a, const ScheduledEvent& b) {
                  return a.at_seconds < b.at_seconds;
              });
    return schedule;
}

} // namespace

std::unique_ptr<ServerEventSource> CreateServerEventSource(const CLIOptions& options) {
    auto scripted_events = LoadScriptedEventsFromConfig();
    if (!scripted_events.empty()) {
        KARMA_TRACE("engine.server",
                    "bz3-server: using scripted event source with {} events from server.startupEvents",
                    scripted_events.size());
        return std::make_unique<ScriptedServerEventSource>(std::move(scripted_events));
    }

    const uint16_t listen_port = options.host_port_explicit
        ? options.host_port
        : karma::config::ReadUInt16Config({"network.ServerPort"}, static_cast<uint16_t>(11899));
    if (auto enet_source = CreateEnetServerEventSource(listen_port)) {
        KARMA_TRACE("engine.server",
                    "bz3-server: using ENet event source on port {}",
                    listen_port);
        return enet_source;
    }

    if (options.host_port_explicit) {
        KARMA_TRACE("engine.server",
                    "bz3-server: ENet event source unavailable (port {}); using null event source",
                    options.host_port);
    } else {
        KARMA_TRACE("engine.server",
                    "bz3-server: ENet event source unavailable; using null event source");
    }
    return std::make_unique<NullServerEventSource>();
}

} // namespace bz3::server::net
