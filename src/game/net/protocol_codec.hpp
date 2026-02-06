#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bz3::net {

struct SessionSnapshotEntry {
    uint32_t session_id = 0;
    std::string session_name{};
};

enum class ClientMessageType {
    JoinRequest,
    PlayerLeave,
    Unknown
};

struct ClientMessage {
    ClientMessageType type = ClientMessageType::Unknown;
    uint32_t client_id = 0;
    std::string player_name{};
    uint32_t protocol_version = 0;
};

enum class ServerMessageType {
    JoinResponse,
    Init,
    SessionSnapshot,
    Unknown
};

struct ServerMessage {
    ServerMessageType type = ServerMessageType::Unknown;
    bool join_accepted = false;
    std::string reason{};
    uint32_t client_id = 0;
    std::string server_name{};
    std::string world_name{};
    uint32_t protocol_version = 0;
    std::string other_payload{};
    std::vector<SessionSnapshotEntry> sessions{};
};

std::optional<ClientMessage> DecodeClientMessage(const void* data, size_t size);
std::optional<ServerMessage> DecodeServerMessage(const void* data, size_t size);

std::vector<std::byte> EncodeClientJoinRequest(std::string_view player_name, uint32_t protocol_version);
std::vector<std::byte> EncodeClientLeave(uint32_t client_id);

std::vector<std::byte> EncodeServerJoinResponse(bool accepted, std::string_view reason);
std::vector<std::byte> EncodeServerInit(uint32_t client_id,
                                        std::string_view server_name,
                                        std::string_view world_name,
                                        uint32_t protocol_version);
std::vector<std::byte> EncodeServerSessionSnapshot(const std::vector<SessionSnapshotEntry>& sessions);

} // namespace bz3::net
