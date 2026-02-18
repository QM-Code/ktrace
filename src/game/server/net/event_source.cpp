#include "server/net/event_source.hpp"
#include "server/net/transport_event_source.hpp"

#include "karma/common/config/store.hpp"
#include "karma/common/logging/logging.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
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
            switch (scheduled.event.type) {
                case ServerInputEvent::Type::ClientJoin:
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: scripted join client_id={} name='{}' auth_payload_present={} at={:.2f}s",
                                scheduled.event.join.client_id,
                                scheduled.event.join.player_name,
                                scheduled.event.join.auth_payload.empty() ? 0 : 1,
                                scheduled.at_seconds);
                    break;
                case ServerInputEvent::Type::ClientLeave:
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: scripted leave client_id={} at={:.2f}s",
                                scheduled.event.leave.client_id,
                                scheduled.at_seconds);
                    break;
                case ServerInputEvent::Type::ClientRequestSpawn:
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: scripted request-spawn client_id={} at={:.2f}s",
                                scheduled.event.request_spawn.client_id,
                                scheduled.at_seconds);
                    break;
                case ServerInputEvent::Type::ClientCreateShot:
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: scripted create-shot client_id={} local_id={} at={:.2f}s",
                                scheduled.event.create_shot.client_id,
                                scheduled.event.create_shot.local_shot_id,
                                scheduled.at_seconds);
                    break;
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

bool ReadDouble(const karma::common::serialization::Value& value, const char* key, double* out) {
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

bool ReadUint32(const karma::common::serialization::Value& value, const char* key, uint32_t* out) {
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

bool ReadFloat(const karma::common::serialization::Value& value, const char* key, float* out) {
    double raw = 0.0;
    if (!ReadDouble(value, key, &raw)) {
        return false;
    }
    if (!std::isfinite(raw)) {
        return false;
    }
    *out = static_cast<float>(raw);
    return std::isfinite(*out);
}

std::string NormalizeEventType(std::string type) {
    std::transform(type.begin(),
                   type.end(),
                   type.begin(),
                   [](unsigned char c) -> char {
                       if (c == '-') {
                           return '_';
                       }
                       return static_cast<char>(std::tolower(c));
                   });
    return type;
}

std::vector<ScheduledEvent> LoadScriptedEventsFromConfig() {
    std::vector<ScheduledEvent> schedule;
    const auto* events_value = karma::common::config::ConfigStore::Get("server.startupEvents");
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

        const std::string type = NormalizeEventType(type_it->get<std::string>());
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
            const auto auth_it = item.find("authPayload");
            if (auth_it != item.end() && auth_it->is_string()) {
                scheduled.event.join.auth_payload = auth_it->get<std::string>();
            }
        } else if (type == "leave") {
            scheduled.event.type = ServerInputEvent::Type::ClientLeave;
            if (!ReadUint32(item, "clientId", &scheduled.event.leave.client_id)) {
                spdlog::warn("server.startupEvents leave item missing numeric 'clientId'; skipping");
                continue;
            }
        } else if (type == "request_spawn" || type == "spawn") {
            scheduled.event.type = ServerInputEvent::Type::ClientRequestSpawn;
            if (!ReadUint32(item, "clientId", &scheduled.event.request_spawn.client_id)) {
                spdlog::warn("server.startupEvents request_spawn item missing numeric 'clientId'; skipping");
                continue;
            }
        } else if (type == "create_shot") {
            scheduled.event.type = ServerInputEvent::Type::ClientCreateShot;
            if (!ReadUint32(item, "clientId", &scheduled.event.create_shot.client_id)
                || !ReadUint32(item, "localShotId", &scheduled.event.create_shot.local_shot_id)
                || !ReadFloat(item, "posX", &scheduled.event.create_shot.pos_x)
                || !ReadFloat(item, "posY", &scheduled.event.create_shot.pos_y)
                || !ReadFloat(item, "posZ", &scheduled.event.create_shot.pos_z)
                || !ReadFloat(item, "velX", &scheduled.event.create_shot.vel_x)
                || !ReadFloat(item, "velY", &scheduled.event.create_shot.vel_y)
                || !ReadFloat(item, "velZ", &scheduled.event.create_shot.vel_z)) {
                spdlog::warn(
                    "server.startupEvents create_shot item requires numeric fields: clientId, localShotId, posX, posY, posZ, velX, velY, velZ; skipping");
                continue;
            }
        } else {
            spdlog::warn(
                "server.startupEvents item has unknown type '{}'; expected join|leave|request_spawn|create_shot",
                type);
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

std::unique_ptr<ServerEventSource> CreateServerEventSource(const karma::cli::server::AppOptions& options,
                                                           uint16_t listen_port) {
    const std::string app_name = options.app_name.empty() ? std::string("server") : options.app_name;
    auto scripted_events = LoadScriptedEventsFromConfig();
    if (!scripted_events.empty()) {
        KARMA_TRACE("engine.server",
                    "{}: using scripted event source with {} events from server.startupEvents",
                    app_name,
                    scripted_events.size());
        return std::make_unique<ScriptedServerEventSource>(std::move(scripted_events));
    }

    if (auto transport_source = CreateServerTransportEventSource(listen_port, app_name)) {
        KARMA_TRACE("engine.server",
                    "{}: using transport event source on port {}",
                    app_name,
                    listen_port);
        return transport_source;
    }

    if (options.listen_port_explicit) {
        KARMA_TRACE("engine.server",
                    "{}: transport event source unavailable (port {}); using null event source",
                    app_name,
                    options.listen_port);
    } else {
        KARMA_TRACE("engine.server",
                    "{}: transport event source unavailable; using null event source",
                    app_name);
    }
    return std::make_unique<NullServerEventSource>();
}

} // namespace bz3::server::net
