#include "net/protocol_codec.hpp"

#include "messages.pb.h"
#include "net/protocol_codec/wire_common.hpp"

namespace bz3::net {

std::vector<std::byte> EncodeServerJoinResponse(bool accepted, std::string_view reason) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::JOIN_RESPONSE);
    auto* join = message.mutable_join_response();
    join->set_accepted(accepted);
    if (!reason.empty()) {
        join->set_reason(std::string(reason));
    }
    return detail::SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerInit(uint32_t client_id,
                                        std::string_view server_name,
                                        std::string_view world_name,
                                        uint32_t protocol_version,
                                        std::string_view world_hash,
                                        uint64_t world_size,
                                        std::string_view world_id,
                                        std::string_view world_revision,
                                        std::string_view world_content_hash,
                                        std::string_view world_manifest_hash,
                                        uint32_t world_manifest_file_count,
                                        const std::vector<WorldManifestEntry>& world_manifest,
                                        const std::vector<std::byte>& world_data) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::INIT);
    auto* init = message.mutable_init();
    init->set_client_id(client_id);
    init->set_server_name(std::string(server_name));
    init->set_world_name(std::string(world_name));
    init->set_protocol_version(protocol_version);
    if (!world_hash.empty()) {
        init->set_world_hash(std::string(world_hash));
    }
    if (world_size > 0) {
        init->set_world_size(world_size);
    }
    if (!world_id.empty()) {
        init->set_world_id(std::string(world_id));
    }
    if (!world_revision.empty()) {
        init->set_world_revision(std::string(world_revision));
    }
    if (!world_content_hash.empty()) {
        init->set_world_content_hash(std::string(world_content_hash));
    }
    if (!world_manifest_hash.empty()) {
        init->set_world_manifest_hash(std::string(world_manifest_hash));
    }
    const uint32_t manifest_count = !world_manifest.empty()
        ? static_cast<uint32_t>(world_manifest.size())
        : world_manifest_file_count;
    if (manifest_count > 0) {
        init->set_world_manifest_file_count(manifest_count);
    }
    for (const auto& entry : world_manifest) {
        auto* wire_entry = init->add_world_manifest();
        wire_entry->set_path(entry.path);
        wire_entry->set_size(entry.size);
        wire_entry->set_hash(entry.hash);
    }
    if (!world_data.empty()) {
        init->set_world_data(reinterpret_cast<const char*>(world_data.data()),
                             static_cast<int>(world_data.size()));
    }
    return detail::SerializeOrEmpty(message);
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
    return detail::SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerPlayerSpawn(uint32_t client_id) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::PLAYER_SPAWN);
    auto* spawn = message.mutable_player_spawn();
    spawn->set_client_id(client_id);
    return detail::SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerPlayerDeath(uint32_t client_id) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::PLAYER_DEATH);
    auto* death = message.mutable_player_death();
    death->set_client_id(client_id);
    return detail::SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerCreateShot(uint32_t source_client_id,
                                              uint32_t global_shot_id,
                                              const Vec3& position,
                                              const Vec3& velocity) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::CREATE_SHOT);
    auto* shot = message.mutable_create_shot();
    shot->set_source_client_id(source_client_id);
    shot->set_global_shot_id(global_shot_id);
    detail::SetVec3(shot->mutable_position(), position);
    detail::SetVec3(shot->mutable_velocity(), velocity);
    return detail::SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerRemoveShot(uint32_t shot_id, bool is_global_id) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::REMOVE_SHOT);
    auto* remove = message.mutable_remove_shot();
    remove->set_shot_id(shot_id);
    remove->set_is_global_id(is_global_id);
    return detail::SerializeOrEmpty(message);
}

} // namespace bz3::net
