#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "server/cli_options.hpp"

namespace bz3::server::net {

struct ClientJoinEvent {
    uint32_t client_id = 0;
    std::string player_name{};
};

struct ClientLeaveEvent {
    uint32_t client_id = 0;
};

struct SessionSnapshotEntry {
    uint32_t session_id = 0;
    std::string session_name{};
};

struct ServerInputEvent {
    enum class Type {
        ClientJoin,
        ClientLeave
    };

    Type type = Type::ClientJoin;
    ClientJoinEvent join{};
    ClientLeaveEvent leave{};
};

class ServerEventSource {
 public:
    virtual ~ServerEventSource() = default;
    virtual std::vector<ServerInputEvent> poll() = 0;
    virtual void onJoinResult(uint32_t client_id,
                              bool accepted,
                              std::string_view reason,
                              std::string_view world_name,
                              const std::vector<SessionSnapshotEntry>& sessions) {
        (void)client_id;
        (void)accepted;
        (void)reason;
        (void)world_name;
        (void)sessions;
    }
};

std::unique_ptr<ServerEventSource> CreateServerEventSource(const CLIOptions& options);

} // namespace bz3::server::net
