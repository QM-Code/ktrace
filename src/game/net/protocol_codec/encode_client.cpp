#include "net/protocol_codec.hpp"

#include "messages.pb.h"
#include "net/protocol_codec/wire_common.hpp"

namespace bz3::net {

std::vector<std::byte> EncodeClientJoinRequest(std::string_view player_name,
                                               uint32_t protocol_version,
                                               std::string_view cached_world_hash,
                                               std::string_view cached_world_id,
                                               std::string_view cached_world_revision,
                                               std::string_view cached_world_content_hash,
                                               std::string_view cached_world_manifest_hash,
                                               uint32_t cached_world_manifest_file_count,
                                               const std::vector<WorldManifestEntry>& cached_world_manifest,
                                               std::string_view auth_payload) {
    karma::ClientMsg message{};
    message.set_type(karma::ClientMsg::JOIN_REQUEST);
    message.set_client_id(0);
    auto* join = message.mutable_join_request();
    join->set_name(std::string(player_name));
    join->set_protocol_version(protocol_version);
    if (!cached_world_hash.empty()) {
        join->set_cached_world_hash(std::string(cached_world_hash));
    }
    if (!cached_world_id.empty()) {
        join->set_cached_world_id(std::string(cached_world_id));
    }
    if (!cached_world_revision.empty()) {
        join->set_cached_world_revision(std::string(cached_world_revision));
    }
    if (!cached_world_content_hash.empty()) {
        join->set_cached_world_content_hash(std::string(cached_world_content_hash));
    }
    if (!cached_world_manifest_hash.empty()) {
        join->set_cached_world_manifest_hash(std::string(cached_world_manifest_hash));
    }
    if (cached_world_manifest_file_count > 0) {
        join->set_cached_world_manifest_file_count(cached_world_manifest_file_count);
    }
    for (const auto& entry : cached_world_manifest) {
        auto* wire_entry = join->add_cached_world_manifest();
        wire_entry->set_path(entry.path);
        wire_entry->set_size(entry.size);
        wire_entry->set_hash(entry.hash);
    }
    if (!auth_payload.empty()) {
        join->set_auth_payload(std::string(auth_payload));
    }
    return detail::SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeClientLeave(uint32_t client_id) {
    karma::ClientMsg message{};
    message.set_type(karma::ClientMsg::PLAYER_LEAVE);
    message.set_client_id(client_id);
    message.mutable_player_leave();
    return detail::SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeClientRequestPlayerSpawn(uint32_t client_id) {
    karma::ClientMsg message{};
    message.set_type(karma::ClientMsg::REQUEST_PLAYER_SPAWN);
    message.set_client_id(client_id);
    message.mutable_request_player_spawn();
    return detail::SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeClientCreateShot(uint32_t client_id,
                                              uint32_t local_shot_id,
                                              const Vec3& position,
                                              const Vec3& velocity) {
    karma::ClientMsg message{};
    message.set_type(karma::ClientMsg::CREATE_SHOT);
    message.set_client_id(client_id);
    auto* shot = message.mutable_create_shot();
    shot->set_local_shot_id(local_shot_id);
    detail::SetVec3(shot->mutable_position(), position);
    detail::SetVec3(shot->mutable_velocity(), velocity);
    return detail::SerializeOrEmpty(message);
}

} // namespace bz3::net
