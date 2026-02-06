#include "net/protocol_codec.hpp"

#include "messages.pb.h"

namespace bz3::net {

namespace {

std::vector<std::byte> ToBytes(const std::string& buffer) {
    const auto* ptr = reinterpret_cast<const std::byte*>(buffer.data());
    return std::vector<std::byte>(ptr, ptr + buffer.size());
}

std::vector<std::byte> SerializeOrEmpty(const karma::ServerMsg& message) {
    std::string payload;
    if (!message.SerializeToString(&payload)) {
        return {};
    }
    return ToBytes(payload);
}

std::vector<std::byte> SerializeOrEmpty(const karma::ClientMsg& message) {
    std::string payload;
    if (!message.SerializeToString(&payload)) {
        return {};
    }
    return ToBytes(payload);
}

const char* PayloadCaseName(karma::ServerMsg::PayloadCase payload_case) {
    switch (payload_case) {
        case karma::ServerMsg::kJoinResponse:
            return "join_response";
        case karma::ServerMsg::kInit:
            return "init";
        case karma::ServerMsg::kPlayerJoin:
            return "player_join";
        case karma::ServerMsg::kSessionSnapshot:
            return "session_snapshot";
        case karma::ServerMsg::kPlayerLeave:
            return "player_leave";
        default:
            return "other";
    }
}

} // namespace

std::optional<ClientMessage> DecodeClientMessage(const void* data, size_t size) {
    if (!data || size == 0) {
        return std::nullopt;
    }

    karma::ClientMsg wire{};
    if (!wire.ParseFromArray(data, static_cast<int>(size))) {
        return std::nullopt;
    }

    ClientMessage out{};
    out.client_id = wire.client_id();

    switch (wire.payload_case()) {
        case karma::ClientMsg::kJoinRequest:
            out.type = ClientMessageType::JoinRequest;
            out.player_name = wire.join_request().name();
            out.protocol_version = wire.join_request().protocol_version();
            return out;
        case karma::ClientMsg::kPlayerJoin:
            out.type = ClientMessageType::JoinRequest;
            out.player_name = wire.player_join().name();
            out.protocol_version = wire.player_join().protocol_version();
            return out;
        case karma::ClientMsg::kPlayerLeave:
            out.type = ClientMessageType::PlayerLeave;
            return out;
        default:
            out.type = ClientMessageType::Unknown;
            return out;
    }
}

std::optional<ServerMessage> DecodeServerMessage(const void* data, size_t size) {
    if (!data || size == 0) {
        return std::nullopt;
    }

    karma::ServerMsg wire{};
    if (!wire.ParseFromArray(data, static_cast<int>(size))) {
        return std::nullopt;
    }

    ServerMessage out{};
    switch (wire.payload_case()) {
        case karma::ServerMsg::kJoinResponse:
            out.type = ServerMessageType::JoinResponse;
            out.join_accepted = wire.join_response().accepted();
            out.reason = wire.join_response().reason();
            return out;
        case karma::ServerMsg::kInit:
            out.type = ServerMessageType::Init;
            out.client_id = wire.init().client_id();
            out.server_name = wire.init().server_name();
            out.world_name = wire.init().world_name();
            out.protocol_version = wire.init().protocol_version();
            return out;
        case karma::ServerMsg::kSessionSnapshot:
            out.type = ServerMessageType::SessionSnapshot;
            out.sessions.clear();
            out.sessions.reserve(static_cast<size_t>(wire.session_snapshot().sessions_size()));
            for (const auto& session : wire.session_snapshot().sessions()) {
                out.sessions.push_back(SessionSnapshotEntry{
                    session.session_id(),
                    session.session_name()});
            }
            return out;
        default:
            out.type = ServerMessageType::Unknown;
            out.other_payload = PayloadCaseName(wire.payload_case());
            return out;
    }
}

std::vector<std::byte> EncodeClientJoinRequest(std::string_view player_name, uint32_t protocol_version) {
    karma::ClientMsg message{};
    message.set_type(karma::ClientMsg::JOIN_REQUEST);
    message.set_client_id(0);
    auto* join = message.mutable_join_request();
    join->set_name(std::string(player_name));
    join->set_protocol_version(protocol_version);
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeClientLeave(uint32_t client_id) {
    karma::ClientMsg message{};
    message.set_type(karma::ClientMsg::PLAYER_LEAVE);
    message.set_client_id(client_id);
    message.mutable_player_leave();
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerJoinResponse(bool accepted, std::string_view reason) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::JOIN_RESPONSE);
    auto* join = message.mutable_join_response();
    join->set_accepted(accepted);
    if (!reason.empty()) {
        join->set_reason(std::string(reason));
    }
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerInit(uint32_t client_id,
                                        std::string_view server_name,
                                        std::string_view world_name,
                                        uint32_t protocol_version) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::INIT);
    auto* init = message.mutable_init();
    init->set_client_id(client_id);
    init->set_server_name(std::string(server_name));
    init->set_world_name(std::string(world_name));
    init->set_protocol_version(protocol_version);
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerSessionSnapshot(const std::vector<SessionSnapshotEntry>& sessions) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::SESSION_SNAPSHOT);
    auto* snapshot = message.mutable_session_snapshot();
    for (const auto& session : sessions) {
        auto* entry = snapshot->add_sessions();
        entry->set_session_id(session.session_id);
        entry->set_session_name(session.session_name);
    }
    return SerializeOrEmpty(message);
}

} // namespace bz3::net
