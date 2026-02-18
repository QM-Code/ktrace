#include "server/net/transport_event_source/internal.hpp"

#include "karma/common/config/helpers.hpp"
#include "karma/common/logging/logging.hpp"
#include "karma/network/content/transfer_sender.hpp"
#include "net/protocol_codec.hpp"
#include "net/protocol.hpp"

#include <string>

namespace bz3::server::net::detail {

bool TransportServerEventSource::sendServerPayload(karma::network::PeerToken peer,
                                                   const std::vector<std::byte>& payload) {
    if (payload.empty()) {
        return false;
    }
    return transport_ && transport_->sendReliable(peer, payload);
}

bool TransportServerEventSource::sendJoinResponse(karma::network::PeerToken peer,
                                                  bool accepted,
                                                  std::string_view reason) {
    return sendServerPayload(peer, bz3::net::EncodeServerJoinResponse(accepted, reason));
}

bool TransportServerEventSource::sendInit(karma::network::PeerToken peer,
                                          uint32_t client_id,
                                          std::string_view world_name,
                                          std::string_view world_id,
                                          std::string_view world_revision,
                                          std::string_view world_hash,
                                          std::string_view world_content_hash,
                                          std::string_view world_manifest_hash,
                                          uint32_t world_manifest_file_count,
                                          uint64_t world_size,
                                          const std::vector<WorldManifestEntry>& world_manifest,
                                          const std::vector<std::byte>& world_package) {
    std::vector<bz3::net::WorldManifestEntry> wire_manifest{};
    wire_manifest.reserve(world_manifest.size());
    for (const auto& entry : world_manifest) {
        wire_manifest.push_back(bz3::net::WorldManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }
    return sendServerPayload(
        peer,
        bz3::net::EncodeServerInit(client_id,
                                   karma::common::config::ReadStringConfig("serverName", app_name_),
                                   world_name,
                                   bz3::net::kProtocolVersion,
                                   world_hash,
                                   world_size,
                                   world_id,
                                   world_revision,
                                   world_content_hash,
                                   world_manifest_hash,
                                   world_manifest_file_count,
                                   wire_manifest,
                                   world_package));
}

bool TransportServerEventSource::sendSessionSnapshot(karma::network::PeerToken peer,
                                                     const std::vector<SessionSnapshotEntry>& sessions) {
    std::vector<bz3::net::SessionSnapshotEntry> wire_sessions{};
    wire_sessions.reserve(sessions.size());
    for (const auto& session : sessions) {
        wire_sessions.push_back(bz3::net::SessionSnapshotEntry{
            session.session_id,
            session.session_name});
    }
    return sendServerPayload(peer, bz3::net::EncodeServerSessionSnapshot(wire_sessions));
}

bool TransportServerEventSource::sendPlayerSpawn(karma::network::PeerToken peer, uint32_t client_id) {
    return sendServerPayload(peer, bz3::net::EncodeServerPlayerSpawn(client_id));
}

bool TransportServerEventSource::sendPlayerDeath(karma::network::PeerToken peer, uint32_t client_id) {
    return sendServerPayload(peer, bz3::net::EncodeServerPlayerDeath(client_id));
}

bool TransportServerEventSource::sendCreateShot(karma::network::PeerToken peer,
                                                uint32_t source_client_id,
                                                uint32_t global_shot_id,
                                                float pos_x,
                                                float pos_y,
                                                float pos_z,
                                                float vel_x,
                                                float vel_y,
                                                float vel_z) {
    return sendServerPayload(peer,
                             bz3::net::EncodeServerCreateShot(source_client_id,
                                                              global_shot_id,
                                                              bz3::net::Vec3{pos_x, pos_y, pos_z},
                                                              bz3::net::Vec3{vel_x, vel_y, vel_z}));
}

bool TransportServerEventSource::sendRemoveShot(karma::network::PeerToken peer,
                                                uint32_t shot_id,
                                                bool is_global_id) {
    return sendServerPayload(peer, bz3::net::EncodeServerRemoveShot(shot_id, is_global_id));
}

bool TransportServerEventSource::sendWorldTransferBegin(karma::network::PeerToken peer,
                                                        std::string_view transfer_id,
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
    return sendServerPayload(peer,
                             bz3::net::EncodeServerWorldTransferBegin(transfer_id,
                                                                      world_id,
                                                                      world_revision,
                                                                      total_bytes,
                                                                      chunk_size,
                                                                      world_hash,
                                                                      world_content_hash,
                                                                      is_delta,
                                                                      delta_base_world_id,
                                                                      delta_base_world_revision,
                                                                      delta_base_world_hash,
                                                                      delta_base_world_content_hash));
}

bool TransportServerEventSource::sendWorldTransferChunk(karma::network::PeerToken peer,
                                                        std::string_view transfer_id,
                                                        uint32_t chunk_index,
                                                        const std::vector<std::byte>& chunk_data) {
    return sendServerPayload(
        peer,
        bz3::net::EncodeServerWorldTransferChunk(transfer_id, chunk_index, chunk_data));
}

bool TransportServerEventSource::sendWorldTransferEnd(karma::network::PeerToken peer,
                                                      std::string_view transfer_id,
                                                      uint32_t chunk_count,
                                                      uint64_t total_bytes,
                                                      std::string_view world_hash,
                                                      std::string_view world_content_hash) {
    return sendServerPayload(peer,
                             bz3::net::EncodeServerWorldTransferEnd(transfer_id,
                                                                    chunk_count,
                                                                    total_bytes,
                                                                    world_hash,
                                                                    world_content_hash));
}

bool TransportServerEventSource::sendWorldPackageChunked(karma::network::PeerToken peer,
                                                         uint32_t client_id,
                                                         std::string_view world_id,
                                                         std::string_view world_revision,
                                                         std::string_view world_hash,
                                                         std::string_view world_content_hash,
                                                         const std::vector<std::byte>& world_package,
                                                         bool is_delta,
                                                         std::string_view delta_base_world_id,
                                                         std::string_view delta_base_world_revision,
                                                         std::string_view delta_base_world_hash,
                                                         std::string_view delta_base_world_content_hash) {
    const auto runtime_config = karma::network::content::ReadTransferSenderRuntimeConfig();
    const std::string transfer_id = karma::network::content::BuildTransferId(client_id,
                                                                              next_transfer_id_++);
    const auto request = karma::network::content::BuildTransferSenderRequest(
        client_id,
        transfer_id,
        world_id,
        world_revision,
        world_hash,
        world_content_hash,
        is_delta,
        delta_base_world_id,
        delta_base_world_revision,
        delta_base_world_hash,
        delta_base_world_content_hash,
        world_package,
        runtime_config);

    karma::network::content::TransferSenderCallbacks callbacks{};
    callbacks.send_begin = [this, peer](std::string_view cb_transfer_id,
                                        std::string_view cb_world_id,
                                        std::string_view cb_world_revision,
                                        uint64_t cb_total_bytes,
                                        uint32_t cb_chunk_size,
                                        std::string_view cb_world_hash,
                                        std::string_view cb_world_content_hash,
                                        bool cb_is_delta,
                                        std::string_view cb_delta_base_world_id,
                                        std::string_view cb_delta_base_world_revision,
                                        std::string_view cb_delta_base_world_hash,
                                        std::string_view cb_delta_base_world_content_hash) {
        return sendWorldTransferBegin(peer,
                                      cb_transfer_id,
                                      cb_world_id,
                                      cb_world_revision,
                                      cb_total_bytes,
                                      cb_chunk_size,
                                      cb_world_hash,
                                      cb_world_content_hash,
                                      cb_is_delta,
                                      cb_delta_base_world_id,
                                      cb_delta_base_world_revision,
                                      cb_delta_base_world_hash,
                                      cb_delta_base_world_content_hash);
    };
    callbacks.send_chunk = [this, peer](std::string_view cb_transfer_id,
                                        uint32_t cb_chunk_index,
                                        const std::vector<std::byte>& cb_chunk_data) {
        return sendWorldTransferChunk(peer, cb_transfer_id, cb_chunk_index, cb_chunk_data);
    };
    callbacks.send_end = [this, peer](std::string_view cb_transfer_id,
                                      uint32_t cb_chunk_count,
                                      uint64_t cb_total_bytes,
                                      std::string_view cb_world_hash,
                                      std::string_view cb_world_content_hash) {
        return sendWorldTransferEnd(peer,
                                    cb_transfer_id,
                                    cb_chunk_count,
                                    cb_total_bytes,
                                    cb_world_hash,
                                    cb_world_content_hash);
    };

    return karma::network::content::SendWorldPackageChunked(request, callbacks, "ServerEventSource");
}

} // namespace bz3::server::net::detail
