#include "net/protocol_codec/wire_common.hpp"

namespace bz3::net::detail {

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
        case karma::ServerMsg::kPlayerSpawn:
            return "player_spawn";
        case karma::ServerMsg::kPlayerDeath:
            return "player_death";
        case karma::ServerMsg::kCreateShot:
            return "create_shot";
        case karma::ServerMsg::kRemoveShot:
            return "remove_shot";
        case karma::ServerMsg::kSessionSnapshot:
            return "session_snapshot";
        case karma::ServerMsg::kWorldTransferBegin:
            return "world_transfer_begin";
        case karma::ServerMsg::kWorldTransferChunk:
            return "world_transfer_chunk";
        case karma::ServerMsg::kWorldTransferEnd:
            return "world_transfer_end";
        case karma::ServerMsg::kPlayerLeave:
            return "player_leave";
        default:
            return "other";
    }
}

Vec3 ToVec3(const karma::Vec3& wire) {
    return Vec3{
        wire.x(),
        wire.y(),
        wire.z()};
}

void SetVec3(karma::Vec3* wire, const Vec3& value) {
    if (!wire) {
        return;
    }
    wire->set_x(value.x);
    wire->set_y(value.y);
    wire->set_z(value.z);
}

} // namespace bz3::net::detail
