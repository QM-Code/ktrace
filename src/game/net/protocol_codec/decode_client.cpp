#include "net/protocol_codec.hpp"

#include "messages.pb.h"
#include "net/protocol_codec/wire_common.hpp"

namespace bz3::net {

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
            out.auth_payload = wire.join_request().auth_payload();
            out.protocol_version = wire.join_request().protocol_version();
            out.cached_world_hash = wire.join_request().cached_world_hash();
            out.cached_world_id = wire.join_request().cached_world_id();
            out.cached_world_revision = wire.join_request().cached_world_revision();
            out.cached_world_content_hash = wire.join_request().cached_world_content_hash();
            out.cached_world_manifest_hash = wire.join_request().cached_world_manifest_hash();
            out.cached_world_manifest_file_count = wire.join_request().cached_world_manifest_file_count();
            out.cached_world_manifest.clear();
            out.cached_world_manifest.reserve(
                static_cast<size_t>(wire.join_request().cached_world_manifest_size()));
            for (const auto& entry : wire.join_request().cached_world_manifest()) {
                out.cached_world_manifest.push_back(WorldManifestEntry{
                    entry.path(),
                    entry.size(),
                    entry.hash()});
            }
            return out;
        case karma::ClientMsg::kPlayerJoin:
            out.type = ClientMessageType::JoinRequest;
            out.player_name = wire.player_join().name();
            out.protocol_version = wire.player_join().protocol_version();
            return out;
        case karma::ClientMsg::kPlayerLeave:
            out.type = ClientMessageType::PlayerLeave;
            return out;
        case karma::ClientMsg::kRequestPlayerSpawn:
            out.type = ClientMessageType::RequestPlayerSpawn;
            return out;
        case karma::ClientMsg::kCreateShot:
            out.type = ClientMessageType::CreateShot;
            out.local_shot_id = wire.create_shot().local_shot_id();
            out.shot_position = detail::ToVec3(wire.create_shot().position());
            out.shot_velocity = detail::ToVec3(wire.create_shot().velocity());
            return out;
        default:
            out.type = ClientMessageType::Unknown;
            return out;
    }
}

} // namespace bz3::net
