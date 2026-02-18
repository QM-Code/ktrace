#include "server/net/transport_event_source/internal.hpp"

#include "karma/common/content/sync_facade.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

namespace bz3::server::net::detail {

namespace {

std::vector<karma::content::ManifestEntry> ToContentManifest(
    const std::vector<WorldManifestEntry>& manifest) {
    std::vector<karma::content::ManifestEntry> converted{};
    converted.reserve(manifest.size());
    for (const auto& entry : manifest) {
        converted.push_back(karma::content::ManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }
    return converted;
}

} // namespace

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
        const karma::content::ServerContentSyncRequest sync_request{
            .world_dir = world_dir,
            .world_name = std::string(world_name),
            .world_id = std::string(world_id),
            .world_revision = std::string(world_revision),
            .world_package_hash = std::string(world_package_hash),
            .world_content_hash = std::string(world_content_hash),
            .world_manifest_hash = std::string(world_manifest_hash),
            .world_manifest_file_count = world_manifest_file_count,
            .world_manifest = ToContentManifest(world_manifest),
            .world_package = world_package,
            .cached_state = karma::content::ServerCachedContentState{
                .world_hash = state.cached_world_hash,
                .world_id = state.cached_world_id,
                .world_revision = state.cached_world_revision,
                .world_content_hash = state.cached_world_content_hash,
                .world_manifest_hash = state.cached_world_manifest_hash,
                .world_manifest_file_count = state.cached_world_manifest_file_count,
                .world_manifest = ToContentManifest(state.cached_world_manifest)}};
        const auto sync_plan = karma::content::BuildDefaultServerContentSyncPlan(sync_request,
                                                                                  "ServerEventSource");

        const bool cache_identity_match = sync_plan.cache_identity_match;
        const bool cache_hash_match = sync_plan.cache_hash_match;
        const bool cache_content_match = sync_plan.cache_content_match;
        const bool cache_manifest_match = sync_plan.cache_manifest_match;
        const bool cache_hit = sync_plan.cache_hit;
        const auto& manifest_diff = sync_plan.manifest_diff;
        const std::string cache_reason = sync_plan.cache_reason;
        const bool send_world_package = sync_plan.send_world_package;
        const bool send_manifest_entries = sync_plan.send_manifest_entries;
        static const std::vector<std::byte> empty_world_package{};
        static const std::vector<WorldManifestEntry> empty_world_manifest{};
        const auto& world_payload = empty_world_package;
        const auto& world_manifest_payload = send_manifest_entries ? world_manifest : empty_world_manifest;
        LogServerManifestDiffPlan(client_id, world_name, manifest_diff);
        const std::vector<std::byte>* transfer_payload = &empty_world_package;
        if (send_world_package) {
            transfer_payload = sync_plan.transfer_is_delta
                                   ? &sync_plan.delta_world_package
                                   : &world_package;
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
                                     sync_plan.transfer_is_delta,
                                     sync_plan.transfer_delta_base_world_id,
                                     sync_plan.transfer_delta_base_world_revision,
                                     sync_plan.transfer_delta_base_world_hash,
                                     sync_plan.transfer_delta_base_world_content_hash)) {
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
                    sync_plan.transfer_mode,
                    sync_plan.transfer_bytes,
                    sync_plan.transfer_is_delta ? 1 : 0,
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
                    sync_plan.transfer_mode,
                    sync_plan.transfer_bytes,
                    sync_plan.transfer_is_delta ? 1 : 0,
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
