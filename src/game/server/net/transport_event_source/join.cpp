#include "server/net/transport_event_source/internal.hpp"

#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

namespace bz3::server::net::detail {

void TransportServerEventSource::onJoinResult(uint32_t client_id,
                                              bool accepted,
                                              std::string_view reason,
                                              std::string_view world_name,
                                              std::string_view world_id,
                                              std::string_view world_revision,
                                              std::string_view world_package_hash,
                                              std::string_view world_content_hash,
                                              std::string_view world_manifest_hash,
                                              uint32_t world_manifest_file_count,
                                              uint64_t world_package_size,
                                              const std::filesystem::path& world_dir,
                                              const std::vector<SessionSnapshotEntry>& sessions,
                                              const std::vector<WorldManifestEntry>& world_manifest,
                                              const std::vector<std::byte>& world_package) {
    const auto peer = findPeerByClientId(client_id);
    if (peer == 0) {
        KARMA_TRACE("engine.server",
                    "ServerEventSource: join result dropped client_id={} accepted={} (peer missing)",
                    client_id,
                    accepted ? 1 : 0);
        return;
    }

    if (accepted) {
        auto state_it = client_by_peer_.find(peer);
        if (state_it == client_by_peer_.end()) {
            KARMA_TRACE("engine.server",
                        "ServerEventSource: join result dropped client_id={} accepted=1 (state missing)",
                        client_id);
            return;
        }

        state_it->second.joined = true;
        const ClientConnectionState& state = state_it->second;
        const bool cache_identity_match = !world_id.empty() && !world_revision.empty() &&
                                          state.cached_world_id == world_id &&
                                          state.cached_world_revision == world_revision;
        const bool cache_hash_match = !world_package_hash.empty() &&
                                      state.cached_world_hash == world_package_hash;
        const bool cache_content_match = !world_content_hash.empty() &&
                                         state.cached_world_content_hash == world_content_hash;
        const bool cache_manifest_match = !world_manifest_hash.empty() &&
                                          state.cached_world_manifest_hash == world_manifest_hash &&
                                          state.cached_world_manifest_file_count == world_manifest_file_count;
        const bool cache_hit =
            cache_identity_match && (cache_hash_match || cache_content_match || cache_manifest_match);
        const ManifestDiffPlan manifest_diff =
            BuildServerManifestDiffPlan(state.cached_world_manifest, world_manifest);
        std::string_view cache_reason = "miss";
        if (cache_hit) {
            if (cache_hash_match) {
                cache_reason = "package_hash";
            } else if (cache_content_match) {
                cache_reason = "content_hash";
            } else {
                cache_reason = "manifest_summary";
            }
        }
        const bool send_world_package = !world_package.empty() && !cache_hit;
        const bool send_manifest_entries = !cache_hit || !cache_manifest_match;
        static const std::vector<std::byte> empty_world_package{};
        static const std::vector<WorldManifestEntry> empty_world_manifest{};
        const auto& world_payload = empty_world_package;
        const auto& world_manifest_payload = send_manifest_entries ? world_manifest : empty_world_manifest;
        std::vector<std::byte> delta_world_package{};
        const std::vector<std::byte>* transfer_payload = &empty_world_package;
        std::string_view transfer_mode = "none";
        uint64_t transfer_bytes = 0;
        bool transfer_is_delta = false;
        std::string transfer_delta_base_world_id{};
        std::string transfer_delta_base_world_revision{};
        std::string transfer_delta_base_world_hash{};
        std::string transfer_delta_base_world_content_hash{};

        LogServerManifestDiffPlan(client_id, world_name, manifest_diff);
        if (send_world_package) {
            transfer_mode = "chunked_full";
            transfer_payload = &world_package;
            transfer_bytes = world_package.size();

            const bool can_try_delta = manifest_diff.incoming_manifest_available &&
                                       state.cached_world_id == world_id &&
                                       !state.cached_world_revision.empty() &&
                                       (manifest_diff.reused_bytes > 0 || manifest_diff.removed_entries > 0);
            if (can_try_delta) {
                const auto delta_archive = BuildWorldDeltaArchive(world_dir,
                                                                  manifest_diff,
                                                                  world_id,
                                                                  world_revision,
                                                                  state.cached_world_revision);
                if (delta_archive.has_value() && !delta_archive->empty() &&
                    delta_archive->size() < world_package.size()) {
                    delta_world_package = std::move(*delta_archive);
                    transfer_payload = &delta_world_package;
                    transfer_bytes = delta_world_package.size();
                    transfer_is_delta = true;
                    transfer_mode = "chunked_delta";
                    transfer_delta_base_world_id = state.cached_world_id;
                    transfer_delta_base_world_revision = state.cached_world_revision;
                    transfer_delta_base_world_hash = state.cached_world_hash;
                    transfer_delta_base_world_content_hash = state.cached_world_content_hash;
                }
            }
        }

        static_cast<void>(sendJoinResponse(peer, true, ""));
        static_cast<void>(sendInit(peer,
                                   client_id,
                                   world_name,
                                   world_id,
                                   world_revision,
                                   world_package_hash,
                                   world_content_hash,
                                   world_manifest_hash,
                                   world_manifest_file_count,
                                   world_package_size,
                                   world_manifest_payload,
                                   world_payload));
        if (send_world_package &&
            !sendWorldPackageChunked(peer,
                                     client_id,
                                     world_id,
                                     world_revision,
                                     world_package_hash,
                                     world_content_hash,
                                     *transfer_payload,
                                     transfer_is_delta,
                                     transfer_delta_base_world_id,
                                     transfer_delta_base_world_revision,
                                     transfer_delta_base_world_hash,
                                     transfer_delta_base_world_content_hash)) {
            spdlog::error("ServerEventSource: failed to stream world package to client_id={} world='{}'",
                          client_id,
                          world_name);
            transport_->disconnect(peer, 0);
            return;
        }
        const bool snapshot_sent = sendSessionSnapshot(peer, sessions);
        KARMA_TRACE("net.server",
                    "Session snapshot {} client_id={} world='{}' id='{}' rev='{}' sessions={} world_package_bytes={} world_transfer_mode={} world_transfer_bytes={} world_transfer_delta={} world_hash={} world_content_hash={} manifest_hash={} manifest_files={} manifest_entries_sent={} manifest_entries_total={} cache_identity_match={} cache_hash_match={} cache_content_match={} cache_manifest_match={} cache_hit={} cache_reason={}",
                    snapshot_sent ? "sent" : "send-failed",
                    client_id,
                    world_name,
                    world_id,
                    world_revision,
                    sessions.size(),
                    world_payload.size(),
                    transfer_mode,
                    transfer_bytes,
                    transfer_is_delta ? 1 : 0,
                    world_package_hash.empty() ? "-" : std::string(world_package_hash),
                    world_content_hash.empty() ? "-" : std::string(world_content_hash),
                    world_manifest_hash.empty() ? "-" : std::string(world_manifest_hash),
                    world_manifest_file_count,
                    world_manifest_payload.size(),
                    world_manifest.size(),
                    cache_identity_match ? 1 : 0,
                    cache_hash_match ? 1 : 0,
                    cache_content_match ? 1 : 0,
                    cache_manifest_match ? 1 : 0,
                    cache_hit ? 1 : 0,
                    cache_reason);
        KARMA_TRACE("engine.server",
                    "ServerEventSource: join accepted client_id={} world='{}' id='{}' rev='{}' snapshot_sessions={} world_package_bytes={} world_transfer_mode={} world_transfer_bytes={} world_transfer_delta={} cache_hit={} cache_reason={}",
                    client_id,
                    world_name,
                    world_id,
                    world_revision,
                    sessions.size(),
                    world_payload.size(),
                    transfer_mode,
                    transfer_bytes,
                    transfer_is_delta ? 1 : 0,
                    cache_hit ? 1 : 0,
                    cache_reason);
        return;
    }

    static_cast<void>(sendJoinResponse(peer, false, reason));
    auto it = client_by_peer_.find(peer);
    if (it != client_by_peer_.end()) {
        it->second.joined = false;
    }
    transport_->disconnect(peer, 0);
    KARMA_TRACE("engine.server",
                "ServerEventSource: join rejected client_id={} reason='{}'",
                client_id,
                reason);
}

} // namespace bz3::server::net::detail
