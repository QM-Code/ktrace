#include "server/net/transport_event_source/internal.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"
#include "net/protocol_codec.hpp"
#include "net/protocol.hpp"

#include <algorithm>
#include <cstddef>
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
                                   karma::config::ReadStringConfig("serverName", app_name_),
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
    if (world_package.empty()) {
        return true;
    }

    const uint32_t configured_chunk_size = static_cast<uint32_t>(
        karma::config::ReadUInt16Config({"network.WorldTransferChunkBytes"},
                                        static_cast<uint16_t>(16 * 1024)));
    const uint32_t chunk_size = std::max<uint32_t>(1, configured_chunk_size);
    const uint32_t max_retry_attempts = static_cast<uint32_t>(
        karma::config::ReadUInt16Config({"network.WorldTransferRetryAttempts"},
                                        static_cast<uint16_t>(2)));
    const std::string transfer_id = std::to_string(client_id) + "-" + std::to_string(next_transfer_id_++);
    const uint32_t total_chunk_count =
        static_cast<uint32_t>((world_package.size() + chunk_size - 1) / chunk_size);

    size_t next_offset = 0;
    uint32_t next_chunk_index = 0;
    for (uint32_t attempt = 0; attempt <= max_retry_attempts; ++attempt) {
        const uint32_t resume_chunk_index = next_chunk_index;
        const size_t resume_offset = next_offset;
        if (!sendWorldTransferBegin(peer,
                                    transfer_id,
                                    world_id,
                                    world_revision,
                                    world_package.size(),
                                    chunk_size,
                                    world_hash,
                                    world_content_hash,
                                    is_delta,
                                    delta_base_world_id,
                                    delta_base_world_revision,
                                    delta_base_world_hash,
                                    delta_base_world_content_hash)) {
            KARMA_TRACE("net.server",
                        "ServerEventSource: world transfer begin send failed client_id={} transfer_id='{}' attempt={}/{} resume_chunk={}",
                        client_id,
                        transfer_id,
                        attempt + 1,
                        max_retry_attempts + 1,
                        resume_chunk_index);
            if (attempt == max_retry_attempts) {
                return false;
            }
            continue;
        }

        bool chunk_send_failed = false;
        while (next_offset < world_package.size()) {
            const size_t remaining = world_package.size() - next_offset;
            const size_t this_chunk_size = std::min<size_t>(remaining, chunk_size);
            std::vector<std::byte> chunk{};
            chunk.insert(chunk.end(),
                         world_package.begin() + static_cast<std::ptrdiff_t>(next_offset),
                         world_package.begin() + static_cast<std::ptrdiff_t>(next_offset + this_chunk_size));
            if (!sendWorldTransferChunk(peer, transfer_id, next_chunk_index, chunk)) {
                chunk_send_failed = true;
                KARMA_TRACE("net.server",
                            "ServerEventSource: world transfer chunk send failed client_id={} transfer_id='{}' attempt={}/{} chunk_index={} chunk_bytes={}",
                            client_id,
                            transfer_id,
                            attempt + 1,
                            max_retry_attempts + 1,
                            next_chunk_index,
                            chunk.size());
                break;
            }
            next_offset += this_chunk_size;
            ++next_chunk_index;
        }

        if (chunk_send_failed) {
            if (attempt == max_retry_attempts) {
                return false;
            }
            KARMA_TRACE("net.server",
                        "ServerEventSource: world transfer retry scheduled client_id={} transfer_id='{}' next_chunk={} sent_chunks={} bytes_sent={} attempt={}/{}",
                        client_id,
                        transfer_id,
                        next_chunk_index,
                        next_chunk_index - resume_chunk_index,
                        next_offset - resume_offset,
                        attempt + 1,
                        max_retry_attempts + 1);
            continue;
        }

        if (!sendWorldTransferEnd(peer,
                                  transfer_id,
                                  total_chunk_count,
                                  world_package.size(),
                                  world_hash,
                                  world_content_hash)) {
            KARMA_TRACE("net.server",
                        "ServerEventSource: world transfer end send failed client_id={} transfer_id='{}' attempt={}/{} total_chunks={}",
                        client_id,
                        transfer_id,
                        attempt + 1,
                        max_retry_attempts + 1,
                        total_chunk_count);
            if (attempt == max_retry_attempts) {
                return false;
            }
            continue;
        }

        KARMA_TRACE("net.server",
                    "ServerEventSource: world transfer sent client_id={} transfer_id='{}' mode={} chunks={} bytes={} chunk_size={} retries={} base_id='{}' base_rev='{}'",
                    client_id,
                    transfer_id,
                    is_delta ? "delta" : "full",
                    total_chunk_count,
                    world_package.size(),
                    chunk_size,
                    attempt,
                    delta_base_world_id.empty() ? "-" : std::string(delta_base_world_id),
                    delta_base_world_revision.empty() ? "-" : std::string(delta_base_world_revision));
        return true;
    }

    return false;
}

} // namespace bz3::server::net::detail
