#include "net/protocol_codec.hpp"

#include "messages.pb.h"
#include "net/protocol_codec/wire_common.hpp"

namespace bz3::net {

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
            out.other_payload = detail::PayloadCaseName(wire.payload_case());
            return out;
    }
}

} // namespace bz3::net
