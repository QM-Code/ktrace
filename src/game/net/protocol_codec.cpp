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
            out.shot_position = ToVec3(wire.create_shot().position());
            out.shot_velocity = ToVec3(wire.create_shot().velocity());
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
            out.world_hash = wire.init().world_hash();
            out.world_size = wire.init().world_size();
            out.world_id = wire.init().world_id();
            out.world_revision = wire.init().world_revision();
            out.world_content_hash = wire.init().world_content_hash();
            out.world_manifest_hash = wire.init().world_manifest_hash();
            out.world_manifest_file_count = wire.init().world_manifest_file_count();
            out.world_manifest.clear();
            out.world_manifest.reserve(static_cast<size_t>(wire.init().world_manifest_size()));
            for (const auto& entry : wire.init().world_manifest()) {
                out.world_manifest.push_back(WorldManifestEntry{
                    entry.path(),
                    entry.size(),
                    entry.hash()});
            }
            if (out.world_manifest_file_count == 0 && !out.world_manifest.empty()) {
                out.world_manifest_file_count = static_cast<uint32_t>(out.world_manifest.size());
            }
            if (!wire.init().world_data().empty()) {
                const std::string& world_data = wire.init().world_data();
                const auto* bytes = reinterpret_cast<const std::byte*>(world_data.data());
                out.world_data.assign(bytes, bytes + world_data.size());
            }
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
        case karma::ServerMsg::kPlayerSpawn:
            out.type = ServerMessageType::PlayerSpawn;
            out.event_client_id = wire.player_spawn().client_id();
            return out;
        case karma::ServerMsg::kPlayerDeath:
            out.type = ServerMessageType::PlayerDeath;
            out.event_client_id = wire.player_death().client_id();
            return out;
        case karma::ServerMsg::kCreateShot:
            out.type = ServerMessageType::CreateShot;
            out.event_shot_id = wire.create_shot().global_shot_id();
            out.event_client_id = wire.create_shot().source_client_id();
            out.event_shot_is_global = true;
            return out;
        case karma::ServerMsg::kRemoveShot:
            out.type = ServerMessageType::RemoveShot;
            out.event_shot_id = wire.remove_shot().shot_id();
            out.event_shot_is_global = wire.remove_shot().is_global_id();
            return out;
        case karma::ServerMsg::kWorldTransferBegin:
            out.type = ServerMessageType::WorldTransferBegin;
            out.transfer_id = wire.world_transfer_begin().transfer_id();
            out.transfer_world_id = wire.world_transfer_begin().world_id();
            out.transfer_world_revision = wire.world_transfer_begin().world_revision();
            out.transfer_total_bytes = wire.world_transfer_begin().total_bytes();
            out.transfer_chunk_size = wire.world_transfer_begin().chunk_size();
            out.transfer_world_hash = wire.world_transfer_begin().world_hash();
            out.transfer_world_content_hash = wire.world_transfer_begin().world_content_hash();
            out.transfer_is_delta = wire.world_transfer_begin().is_delta();
            out.transfer_delta_base_world_id = wire.world_transfer_begin().delta_base_world_id();
            out.transfer_delta_base_world_revision = wire.world_transfer_begin().delta_base_world_revision();
            out.transfer_delta_base_world_hash = wire.world_transfer_begin().delta_base_world_hash();
            out.transfer_delta_base_world_content_hash =
                wire.world_transfer_begin().delta_base_world_content_hash();
            return out;
        case karma::ServerMsg::kWorldTransferChunk:
            out.type = ServerMessageType::WorldTransferChunk;
            out.transfer_id = wire.world_transfer_chunk().transfer_id();
            out.transfer_chunk_index = wire.world_transfer_chunk().chunk_index();
            if (!wire.world_transfer_chunk().chunk_data().empty()) {
                const std::string& chunk_data = wire.world_transfer_chunk().chunk_data();
                const auto* bytes = reinterpret_cast<const std::byte*>(chunk_data.data());
                out.transfer_chunk_data.assign(bytes, bytes + chunk_data.size());
            }
            return out;
        case karma::ServerMsg::kWorldTransferEnd:
            out.type = ServerMessageType::WorldTransferEnd;
            out.transfer_id = wire.world_transfer_end().transfer_id();
            out.transfer_chunk_count = wire.world_transfer_end().chunk_count();
            out.transfer_total_bytes = wire.world_transfer_end().total_bytes();
            out.transfer_world_hash = wire.world_transfer_end().world_hash();
            out.transfer_world_content_hash = wire.world_transfer_end().world_content_hash();
            return out;
        default:
            out.type = ServerMessageType::Unknown;
            out.other_payload = PayloadCaseName(wire.payload_case());
            return out;
    }
}

std::vector<std::byte> EncodeClientJoinRequest(std::string_view player_name,
                                               uint32_t protocol_version,
                                               std::string_view cached_world_hash,
                                               std::string_view cached_world_id,
                                               std::string_view cached_world_revision,
                                               std::string_view cached_world_content_hash,
                                               std::string_view cached_world_manifest_hash,
                                               uint32_t cached_world_manifest_file_count,
                                               const std::vector<WorldManifestEntry>& cached_world_manifest) {
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
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeClientLeave(uint32_t client_id) {
    karma::ClientMsg message{};
    message.set_type(karma::ClientMsg::PLAYER_LEAVE);
    message.set_client_id(client_id);
    message.mutable_player_leave();
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeClientRequestPlayerSpawn(uint32_t client_id) {
    karma::ClientMsg message{};
    message.set_type(karma::ClientMsg::REQUEST_PLAYER_SPAWN);
    message.set_client_id(client_id);
    message.mutable_request_player_spawn();
    return SerializeOrEmpty(message);
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
    SetVec3(shot->mutable_position(), position);
    SetVec3(shot->mutable_velocity(), velocity);
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

std::vector<std::byte> EncodeServerPlayerSpawn(uint32_t client_id) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::PLAYER_SPAWN);
    auto* spawn = message.mutable_player_spawn();
    spawn->set_client_id(client_id);
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerPlayerDeath(uint32_t client_id) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::PLAYER_DEATH);
    auto* death = message.mutable_player_death();
    death->set_client_id(client_id);
    return SerializeOrEmpty(message);
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
    SetVec3(shot->mutable_position(), position);
    SetVec3(shot->mutable_velocity(), velocity);
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerRemoveShot(uint32_t shot_id, bool is_global_id) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::REMOVE_SHOT);
    auto* remove = message.mutable_remove_shot();
    remove->set_shot_id(shot_id);
    remove->set_is_global_id(is_global_id);
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerWorldTransferBegin(std::string_view transfer_id,
                                                      std::string_view world_id,
                                                      std::string_view world_revision,
                                                      uint64_t total_bytes,
                                                      uint32_t chunk_size,
                                                      std::string_view world_hash,
                                                      std::string_view world_content_hash,
                                                      bool is_delta,
                                                      std::string_view delta_base_world_id,
                                                      std::string_view delta_base_world_revision,
                                                      std::string_view delta_base_world_hash,
                                                      std::string_view delta_base_world_content_hash) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::WORLD_TRANSFER_BEGIN);
    auto* begin = message.mutable_world_transfer_begin();
    begin->set_transfer_id(std::string(transfer_id));
    begin->set_world_id(std::string(world_id));
    begin->set_world_revision(std::string(world_revision));
    begin->set_total_bytes(total_bytes);
    begin->set_chunk_size(chunk_size);
    if (!world_hash.empty()) {
        begin->set_world_hash(std::string(world_hash));
    }
    if (!world_content_hash.empty()) {
        begin->set_world_content_hash(std::string(world_content_hash));
    }
    begin->set_is_delta(is_delta);
    if (!delta_base_world_id.empty()) {
        begin->set_delta_base_world_id(std::string(delta_base_world_id));
    }
    if (!delta_base_world_revision.empty()) {
        begin->set_delta_base_world_revision(std::string(delta_base_world_revision));
    }
    if (!delta_base_world_hash.empty()) {
        begin->set_delta_base_world_hash(std::string(delta_base_world_hash));
    }
    if (!delta_base_world_content_hash.empty()) {
        begin->set_delta_base_world_content_hash(std::string(delta_base_world_content_hash));
    }
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerWorldTransferChunk(std::string_view transfer_id,
                                                      uint32_t chunk_index,
                                                      const std::vector<std::byte>& chunk_data) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::WORLD_TRANSFER_CHUNK);
    auto* chunk = message.mutable_world_transfer_chunk();
    chunk->set_transfer_id(std::string(transfer_id));
    chunk->set_chunk_index(chunk_index);
    if (!chunk_data.empty()) {
        chunk->set_chunk_data(reinterpret_cast<const char*>(chunk_data.data()),
                              static_cast<int>(chunk_data.size()));
    }
    return SerializeOrEmpty(message);
}

std::vector<std::byte> EncodeServerWorldTransferEnd(std::string_view transfer_id,
                                                    uint32_t chunk_count,
                                                    uint64_t total_bytes,
                                                    std::string_view world_hash,
                                                    std::string_view world_content_hash) {
    karma::ServerMsg message{};
    message.set_type(karma::ServerMsg::WORLD_TRANSFER_END);
    auto* end = message.mutable_world_transfer_end();
    end->set_transfer_id(std::string(transfer_id));
    end->set_chunk_count(chunk_count);
    end->set_total_bytes(total_bytes);
    if (!world_hash.empty()) {
        end->set_world_hash(std::string(world_hash));
    }
    if (!world_content_hash.empty()) {
        end->set_world_content_hash(std::string(world_content_hash));
    }
    return SerializeOrEmpty(message);
}

} // namespace bz3::net
