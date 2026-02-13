#include "server/net/transport_event_source.hpp"

#include "karma/network/server_transport.hpp"
#include "karma/network/transport_config_mapping.hpp"
#include "karma/common/world_archive.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"
#include "net/protocol_codec.hpp"
#include "net/protocol.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace bz3::server::net {

namespace {

struct ClientConnectionState {
    karma::network::PeerToken peer = 0;
    std::string peer_ip{};
    uint16_t peer_port = 0;
    uint32_t client_id = 0;
    bool joined = false;
    std::string player_name{};
    std::string cached_world_hash{};
    std::string cached_world_id{};
    std::string cached_world_revision{};
    std::string cached_world_content_hash{};
    std::string cached_world_manifest_hash{};
    uint32_t cached_world_manifest_file_count = 0;
    std::vector<WorldManifestEntry> cached_world_manifest{};
};

std::string DefaultPlayerName(uint32_t client_id) {
    return "player-" + std::to_string(client_id);
}

std::string ResolvePlayerName(const bz3::net::ClientMessage& message, uint32_t client_id) {
    if (!message.player_name.empty()) {
        return message.player_name;
    }
    return DefaultPlayerName(client_id);
}

constexpr const char* kDeltaRemovedPathsFile = "__bz3_delta_removed_paths.txt";
constexpr const char* kDeltaMetaFile = "__bz3_delta_meta.txt";

struct ManifestDiffPlan {
    bool incoming_manifest_available = false;
    size_t cached_entries = 0;
    size_t incoming_entries = 0;
    size_t unchanged_entries = 0;
    size_t added_entries = 0;
    size_t modified_entries = 0;
    size_t removed_entries = 0;
    uint64_t potential_transfer_bytes = 0;
    uint64_t reused_bytes = 0;
    uint64_t delta_transfer_bytes = 0;
    std::vector<std::string> changed_paths{};
    std::vector<std::string> removed_paths{};
};

bool NormalizeRelativePath(std::string_view raw_path, std::filesystem::path* out) {
    if (!out || raw_path.empty()) {
        return false;
    }
    std::filesystem::path normalized = std::filesystem::path(raw_path).lexically_normal();
    if (normalized.empty() || normalized == "." || normalized.is_absolute()
        || normalized.has_root_path()) {
        return false;
    }
    for (const auto& part : normalized) {
        if (part == "..") {
            return false;
        }
    }
    *out = normalized;
    return true;
}

ManifestDiffPlan BuildServerManifestDiffPlan(const std::vector<WorldManifestEntry>& cached_manifest,
                                             const std::vector<WorldManifestEntry>& incoming_manifest) {
    ManifestDiffPlan plan{};
    plan.cached_entries = cached_manifest.size();
    plan.incoming_entries = incoming_manifest.size();

    if (incoming_manifest.empty()) {
        return plan;
    }
    plan.incoming_manifest_available = true;

    for (const auto& entry : incoming_manifest) {
        plan.potential_transfer_bytes += entry.size;
    }

    if (cached_manifest.empty()) {
        plan.added_entries = incoming_manifest.size();
        plan.delta_transfer_bytes = plan.potential_transfer_bytes;
        plan.changed_paths.reserve(incoming_manifest.size());
        for (const auto& entry : incoming_manifest) {
            plan.changed_paths.push_back(entry.path);
        }
        return plan;
    }

    std::unordered_map<std::string, const WorldManifestEntry*> cached_by_path{};
    cached_by_path.reserve(cached_manifest.size());
    for (const auto& entry : cached_manifest) {
        cached_by_path[entry.path] = &entry;
    }

    for (const auto& entry : incoming_manifest) {
        const auto it = cached_by_path.find(entry.path);
        if (it == cached_by_path.end()) {
            ++plan.added_entries;
            plan.changed_paths.push_back(entry.path);
            continue;
        }
        const WorldManifestEntry& cached_entry = *it->second;
        if (cached_entry.size == entry.size && cached_entry.hash == entry.hash) {
            ++plan.unchanged_entries;
            plan.reused_bytes += entry.size;
        } else {
            ++plan.modified_entries;
            plan.changed_paths.push_back(entry.path);
        }
        cached_by_path.erase(it);
    }
    plan.removed_entries = cached_by_path.size();
    plan.removed_paths.reserve(plan.removed_entries);
    for (const auto& [path, entry_ptr] : cached_by_path) {
        (void)entry_ptr;
        plan.removed_paths.push_back(path);
    }
    plan.delta_transfer_bytes = plan.potential_transfer_bytes - plan.reused_bytes;
    return plan;
}

void LogServerManifestDiffPlan(uint32_t client_id,
                               std::string_view world_name,
                               const ManifestDiffPlan& plan) {
    if (!plan.incoming_manifest_available) {
        KARMA_TRACE("net.server",
                    "ServerEventSource: manifest diff plan skipped client_id={} world='{}' (incoming manifest unavailable, cached_entries={})",
                    client_id,
                    world_name,
                    plan.cached_entries);
        return;
    }

    KARMA_TRACE("net.server",
                "ServerEventSource: manifest diff plan client_id={} world='{}' cached_entries={} incoming_entries={} unchanged={} added={} modified={} removed={} potential_transfer_bytes={} reused_bytes={} delta_transfer_bytes={}",
                client_id,
                world_name,
                plan.cached_entries,
                plan.incoming_entries,
                plan.unchanged_entries,
                plan.added_entries,
                plan.modified_entries,
                plan.removed_entries,
                plan.potential_transfer_bytes,
                plan.reused_bytes,
                plan.delta_transfer_bytes);
}

std::optional<std::vector<std::byte>> BuildWorldDeltaArchive(
    const std::filesystem::path& world_dir,
    const ManifestDiffPlan& diff_plan,
    std::string_view world_id,
    std::string_view target_world_revision,
    std::string_view base_world_revision) {
    try {
        const auto nonce = static_cast<unsigned long long>(
            std::chrono::steady_clock::now().time_since_epoch().count());
        const std::filesystem::path staging_dir =
            std::filesystem::temp_directory_path() /
            ("bz3-world-delta-" + std::to_string(nonce));
        const std::filesystem::path archive_path = staging_dir.string() + ".zip";
        std::error_code ec;

        auto cleanup = [&]() {
            std::filesystem::remove_all(staging_dir, ec);
            ec.clear();
            std::filesystem::remove(archive_path, ec);
        };

        std::filesystem::remove_all(staging_dir, ec);
        ec.clear();
        std::filesystem::create_directories(staging_dir, ec);
        if (ec) {
            spdlog::error("ServerEventSource: failed to create delta staging dir '{}': {}",
                          staging_dir.string(),
                          ec.message());
            cleanup();
            return std::nullopt;
        }

        for (const auto& rel_path : diff_plan.changed_paths) {
            std::filesystem::path normalized_rel{};
            if (!NormalizeRelativePath(rel_path, &normalized_rel)) {
                spdlog::error("ServerEventSource: invalid delta path '{}' for world '{}'",
                              rel_path,
                              world_id);
                cleanup();
                return std::nullopt;
            }
            const std::filesystem::path source_path = world_dir / normalized_rel;
            if (!std::filesystem::exists(source_path) || !std::filesystem::is_regular_file(source_path)) {
                spdlog::error("ServerEventSource: delta source missing '{}' for world '{}'",
                              source_path.string(),
                              world_id);
                cleanup();
                return std::nullopt;
            }
            const std::filesystem::path target_path = staging_dir / normalized_rel;
            std::filesystem::create_directories(target_path.parent_path(), ec);
            if (ec) {
                spdlog::error("ServerEventSource: failed to create delta parent dir '{}': {}",
                              target_path.parent_path().string(),
                              ec.message());
                cleanup();
                return std::nullopt;
            }
            std::filesystem::copy_file(source_path,
                                       target_path,
                                       std::filesystem::copy_options::overwrite_existing,
                                       ec);
            if (ec) {
                spdlog::error("ServerEventSource: failed to copy delta file '{}' -> '{}': {}",
                              source_path.string(),
                              target_path.string(),
                              ec.message());
                cleanup();
                return std::nullopt;
            }
        }

        {
            std::ofstream removed_out(staging_dir / kDeltaRemovedPathsFile, std::ios::trunc);
            if (!removed_out) {
                spdlog::error("ServerEventSource: failed to write delta removals file for world '{}'",
                              world_id);
                cleanup();
                return std::nullopt;
            }
            for (const auto& removed : diff_plan.removed_paths) {
                removed_out << removed << '\n';
            }
        }

        {
            std::ofstream meta_out(staging_dir / kDeltaMetaFile, std::ios::trunc);
            if (!meta_out) {
                spdlog::error("ServerEventSource: failed to write delta meta file for world '{}'",
                              world_id);
                cleanup();
                return std::nullopt;
            }
            meta_out << "world_id=" << world_id << '\n';
            meta_out << "base_world_revision=" << base_world_revision << '\n';
            meta_out << "target_world_revision=" << target_world_revision << '\n';
            meta_out << "changed_entries=" << diff_plan.changed_paths.size() << '\n';
            meta_out << "removed_entries=" << diff_plan.removed_paths.size() << '\n';
        }

        auto delta_bytes = world::BuildWorldArchive(staging_dir);
        cleanup();
        return delta_bytes;
    } catch (const std::exception& ex) {
        spdlog::error("ServerEventSource: failed to build world delta archive for world '{}': {}",
                      world_id,
                      ex.what());
        return std::nullopt;
    }
}

class TransportServerEventSource final : public ServerEventSource {
 public:
    explicit TransportServerEventSource(uint16_t port) : port_(port) {
        bool custom_backend = false;
        karma::network::ServerTransportConfig transport_config =
            karma::network::ResolveServerTransportConfigFromConfig(port_,
                                                                   kMaxClients,
                                                                   kNumChannels,
                                                                   &custom_backend);
        if (custom_backend) {
            KARMA_TRACE("net.server",
                        "ServerEventSource: using custom server transport backend='{}'",
                        transport_config.backend_name);
        }

        transport_ = karma::network::CreateServerTransport(transport_config);
        if (!transport_ || !transport_->isReady()) {
            const std::string configured_backend =
                karma::network::EffectiveServerTransportBackendName(transport_config);
            spdlog::error("ServerEventSource: failed to create server transport backend={} port={}",
                          configured_backend,
                          port_);
            return;
        }

        initialized_ = true;
        KARMA_TRACE("engine.server",
                    "ServerEventSource: listening on port {} (transport={} max_clients={} channels={})",
                    port_,
                    transport_->backendName(),
                    kMaxClients,
                    kNumChannels);
    }

    ~TransportServerEventSource() override = default;

    bool initialized() const { return initialized_; }

    std::vector<ServerInputEvent> poll() override {
        std::vector<ServerInputEvent> out;
        if (!transport_) {
            return out;
        }

        std::vector<karma::network::ServerTransportEvent> transport_events{};
        transport_->poll(karma::network::ServerTransportPollOptions{}, &transport_events);
        for (const auto& transport_event : transport_events) {
            switch (transport_event.type) {
                case karma::network::ServerTransportEventType::Connected: {
                    const uint32_t client_id = allocateClientId();
                    ClientConnectionState state{};
                    state.peer = transport_event.peer;
                    state.peer_ip = transport_event.peer_ip;
                    state.peer_port = transport_event.peer_port;
                    state.client_id = client_id;
                    client_by_peer_[transport_event.peer] = std::move(state);
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: transport connect client_id={} ip={} port={} (awaiting join packet)",
                                client_id,
                                transport_event.peer_ip,
                                transport_event.peer_port);
                    KARMA_TRACE("net.server",
                                "transport connect client_id={} ip={} port={}",
                                client_id,
                                transport_event.peer_ip,
                                transport_event.peer_port);
                    break;
                }
                case karma::network::ServerTransportEventType::Disconnected: {
                    const auto it = client_by_peer_.find(transport_event.peer);
                    if (it == client_by_peer_.end()) {
                        break;
                    }
                    const ClientConnectionState state = it->second;
                    client_by_peer_.erase(it);
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: transport disconnect client_id={} ip={} port={} joined={}",
                                state.client_id,
                                state.peer_ip,
                                state.peer_port,
                                state.joined ? 1 : 0);
                    KARMA_TRACE("net.server",
                                "transport disconnect client_id={} ip={} port={} joined={}",
                                state.client_id,
                                state.peer_ip,
                                state.peer_port,
                                state.joined ? 1 : 0);
                    if (state.joined) {
                        ServerInputEvent input{};
                        input.type = ServerInputEvent::Type::ClientLeave;
                        input.leave.client_id = state.client_id;
                        out.push_back(std::move(input));
                    }
                    break;
                }
                case karma::network::ServerTransportEventType::Received: {
                    handleReceiveEvent(transport_event, out);
                    break;
                }
                default:
                    break;
            }
        }

        return out;
    }

    void onJoinResult(uint32_t client_id,
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
                      const std::vector<std::byte>& world_package) override {
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

            const ClientConnectionState& state = state_it->second;
            const bool cache_identity_match = !world_id.empty() &&
                                              !world_revision.empty() &&
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
                                           (manifest_diff.reused_bytes > 0 ||
                                            manifest_diff.removed_entries > 0);
                if (can_try_delta) {
                    const auto delta_archive =
                        BuildWorldDeltaArchive(world_dir,
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

    void onPlayerSpawn(uint32_t client_id) override {
        const size_t sent = broadcastToJoined([client_id, this](karma::network::PeerToken peer) {
            return sendPlayerSpawn(peer, client_id);
        });
        KARMA_TRACE("net.server",
                    "ServerEventSource: broadcast player_spawn client_id={} peers={}",
                    client_id,
                    sent);
    }

    void onPlayerDeath(uint32_t client_id) override {
        const size_t sent = broadcastToJoined([client_id, this](karma::network::PeerToken peer) {
            return sendPlayerDeath(peer, client_id);
        });
        KARMA_TRACE("net.server",
                    "ServerEventSource: broadcast player_death client_id={} peers={}",
                    client_id,
                    sent);
    }

    void onCreateShot(uint32_t source_client_id,
                      uint32_t global_shot_id,
                      float pos_x,
                      float pos_y,
                      float pos_z,
                      float vel_x,
                      float vel_y,
                      float vel_z) override {
        const size_t sent = broadcastToJoined([=, this](karma::network::PeerToken peer) {
            return sendCreateShot(peer,
                                  source_client_id,
                                  global_shot_id,
                                  pos_x,
                                  pos_y,
                                  pos_z,
                                  vel_x,
                                  vel_y,
                                  vel_z);
        });
        KARMA_TRACE("net.server",
                    "ServerEventSource: broadcast create_shot source_client_id={} global_shot_id={} peers={}",
                    source_client_id,
                    global_shot_id,
                    sent);
    }

    void onRemoveShot(uint32_t shot_id, bool is_global_id) override {
        const size_t sent = broadcastToJoined([shot_id, is_global_id, this](karma::network::PeerToken peer) {
            return sendRemoveShot(peer, shot_id, is_global_id);
        });
        KARMA_TRACE("net.server",
                    "ServerEventSource: broadcast remove_shot shot_id={} global={} peers={}",
                    shot_id,
                    is_global_id ? 1 : 0,
                    sent);
    }

 private:
    void emitJoinEvent(uint32_t client_id, const std::string& player_name, std::vector<ServerInputEvent>& out) {
        ServerInputEvent input{};
        input.type = ServerInputEvent::Type::ClientJoin;
        input.join.client_id = client_id;
        input.join.player_name = player_name;
        out.push_back(std::move(input));
    }

    void emitLeaveEvent(uint32_t client_id, std::vector<ServerInputEvent>& out) {
        ServerInputEvent input{};
        input.type = ServerInputEvent::Type::ClientLeave;
        input.leave.client_id = client_id;
        out.push_back(std::move(input));
    }

    void emitRequestSpawnEvent(uint32_t client_id, std::vector<ServerInputEvent>& out) {
        ServerInputEvent input{};
        input.type = ServerInputEvent::Type::ClientRequestSpawn;
        input.request_spawn.client_id = client_id;
        out.push_back(std::move(input));
    }

    void emitCreateShotEvent(uint32_t client_id,
                             uint32_t local_shot_id,
                             float pos_x,
                             float pos_y,
                             float pos_z,
                             float vel_x,
                             float vel_y,
                             float vel_z,
                             std::vector<ServerInputEvent>& out) {
        ServerInputEvent input{};
        input.type = ServerInputEvent::Type::ClientCreateShot;
        input.create_shot.client_id = client_id;
        input.create_shot.local_shot_id = local_shot_id;
        input.create_shot.pos_x = pos_x;
        input.create_shot.pos_y = pos_y;
        input.create_shot.pos_z = pos_z;
        input.create_shot.vel_x = vel_x;
        input.create_shot.vel_y = vel_y;
        input.create_shot.vel_z = vel_z;
        out.push_back(std::move(input));
    }

    void handleReceiveEvent(const karma::network::ServerTransportEvent& event,
                            std::vector<ServerInputEvent>& out) {
        const auto peer_it = client_by_peer_.find(event.peer);
        if (peer_it == client_by_peer_.end()) {
            KARMA_TRACE("engine.server",
                        "ServerEventSource: transport receive from unknown peer ip={} port={} bytes={}",
                        event.peer_ip,
                        event.peer_port,
                        event.payload.size());
            return;
        }

        ClientConnectionState& state = peer_it->second;
        if (!event.peer_ip.empty()) {
            state.peer_ip = event.peer_ip;
        }
        if (event.peer_port != 0) {
            state.peer_port = event.peer_port;
        }

        if (event.payload.empty()) {
            KARMA_TRACE("engine.server",
                        "ServerEventSource: transport receive empty payload client_id={}",
                        state.client_id);
            return;
        }

        const auto decoded = bz3::net::DecodeClientMessage(event.payload.data(), event.payload.size());
        if (!decoded.has_value()) {
            KARMA_TRACE("engine.server",
                        "ServerEventSource: transport receive invalid protobuf client_id={} bytes={}",
                        state.client_id,
                        event.payload.size());
            return;
        }

        switch (decoded->type) {
            case bz3::net::ClientMessageType::JoinRequest: {
                const std::string player_name = ResolvePlayerName(*decoded, state.client_id);
                const uint32_t protocol_version = decoded->protocol_version;
                state.cached_world_hash = decoded->cached_world_hash;
                state.cached_world_id = decoded->cached_world_id;
                state.cached_world_revision = decoded->cached_world_revision;
                state.cached_world_content_hash = decoded->cached_world_content_hash;
                state.cached_world_manifest_hash = decoded->cached_world_manifest_hash;
                state.cached_world_manifest_file_count = decoded->cached_world_manifest_file_count;
                state.cached_world_manifest.clear();
                state.cached_world_manifest.reserve(decoded->cached_world_manifest.size());
                for (const auto& entry : decoded->cached_world_manifest) {
                    state.cached_world_manifest.push_back(WorldManifestEntry{
                        entry.path,
                        entry.size,
                        entry.hash});
                }
                KARMA_TRACE("net.server",
                            "Handshake request client_id={} name='{}' protocol={} cached_world_hash='{}' cached_world_id='{}' cached_world_revision='{}' cached_world_content_hash='{}' cached_world_manifest_hash='{}' cached_world_manifest_files={} cached_world_manifest_entries={} ip={} port={}",
                            state.client_id,
                            player_name,
                            protocol_version,
                            state.cached_world_hash.empty() ? "-" : state.cached_world_hash,
                            state.cached_world_id.empty() ? "-" : state.cached_world_id,
                            state.cached_world_revision.empty() ? "-" : state.cached_world_revision,
                            state.cached_world_content_hash.empty() ? "-" : state.cached_world_content_hash,
                            state.cached_world_manifest_hash.empty() ? "-" : state.cached_world_manifest_hash,
                            state.cached_world_manifest_file_count,
                            state.cached_world_manifest.size(),
                            state.peer_ip,
                            state.peer_port);
                if (protocol_version != bz3::net::kProtocolVersion) {
                    static_cast<void>(
                        sendJoinResponse(state.peer, false, "Protocol version mismatch."));
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: transport join rejected client_id={} name='{}' protocol={} expected={}",
                                state.client_id,
                                player_name,
                                protocol_version,
                                bz3::net::kProtocolVersion);
                    transport_->disconnect(state.peer, 0);
                    return;
                }
                if (state.joined) {
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: transport duplicate join client_id={} name='{}' ignored",
                                state.client_id,
                                player_name);
                    return;
                }

                state.joined = true;
                state.player_name = player_name;
                emitJoinEvent(state.client_id, state.player_name, out);
                KARMA_TRACE("engine.server",
                            "ServerEventSource: transport join client_id={} name='{}' protocol={} ip={} port={}",
                            state.client_id,
                            state.player_name,
                            protocol_version,
                            state.peer_ip,
                            state.peer_port);
                return;
            }
            case bz3::net::ClientMessageType::PlayerLeave: {
                if (!state.joined) {
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: transport leave before join client_id={} ignored",
                                state.client_id);
                    return;
                }

                state.joined = false;
                emitLeaveEvent(state.client_id, out);
                KARMA_TRACE("engine.server",
                            "ServerEventSource: transport leave client_id={} name='{}' ip={} port={}",
                            state.client_id,
                            state.player_name,
                            state.peer_ip,
                            state.peer_port);
                return;
            }
            case bz3::net::ClientMessageType::RequestPlayerSpawn: {
                if (!state.joined) {
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: transport request_spawn before join client_id={} ignored",
                                state.client_id);
                    return;
                }

                emitRequestSpawnEvent(state.client_id, out);
                KARMA_TRACE("engine.server",
                            "ServerEventSource: transport request_spawn client_id={} name='{}' ip={} port={}",
                            state.client_id,
                            state.player_name,
                            state.peer_ip,
                            state.peer_port);
                return;
            }
            case bz3::net::ClientMessageType::CreateShot: {
                if (!state.joined) {
                    KARMA_TRACE("engine.server",
                                "ServerEventSource: transport create_shot before join client_id={} ignored",
                                state.client_id);
                    return;
                }

                emitCreateShotEvent(state.client_id,
                                    decoded->local_shot_id,
                                    decoded->shot_position.x,
                                    decoded->shot_position.y,
                                    decoded->shot_position.z,
                                    decoded->shot_velocity.x,
                                    decoded->shot_velocity.y,
                                    decoded->shot_velocity.z,
                                    out);
                KARMA_TRACE("engine.server",
                            "ServerEventSource: transport create_shot client_id={} local_shot_id={} ip={} port={}",
                            state.client_id,
                            decoded->local_shot_id,
                            state.peer_ip,
                            state.peer_port);
                return;
            }
            default:
                KARMA_TRACE("engine.server",
                            "ServerEventSource: transport payload ignored client_id={} bytes={}",
                            state.client_id,
                            event.payload.size());
                return;
        }
    }

    template <typename SenderFn>
    size_t broadcastToJoined(SenderFn&& sender) {
        size_t sent = 0;
        for (auto& [peer, state] : client_by_peer_) {
            if (!peer || !state.joined) {
                continue;
            }
            if (sender(peer)) {
                ++sent;
            }
        }
        return sent;
    }

    karma::network::PeerToken findPeerByClientId(uint32_t client_id) {
        for (auto& [peer, state] : client_by_peer_) {
            if (state.client_id == client_id) {
                return peer;
            }
        }
        return 0;
    }

    bool sendServerPayload(karma::network::PeerToken peer, const std::vector<std::byte>& payload) {
        if (payload.empty()) {
            return false;
        }
        return transport_ && transport_->sendReliable(peer, payload);
    }

    bool sendJoinResponse(karma::network::PeerToken peer, bool accepted, std::string_view reason) {
        return sendServerPayload(peer, bz3::net::EncodeServerJoinResponse(accepted, reason));
    }

    bool sendInit(karma::network::PeerToken peer,
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
            bz3::net::EncodeServerInit(
                client_id,
                karma::config::ReadStringConfig({"serverName"}, std::string("bz3-server")),
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

    bool sendSessionSnapshot(karma::network::PeerToken peer,
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

    bool sendPlayerSpawn(karma::network::PeerToken peer, uint32_t client_id) {
        return sendServerPayload(peer, bz3::net::EncodeServerPlayerSpawn(client_id));
    }

    bool sendPlayerDeath(karma::network::PeerToken peer, uint32_t client_id) {
        return sendServerPayload(peer, bz3::net::EncodeServerPlayerDeath(client_id));
    }

    bool sendCreateShot(karma::network::PeerToken peer,
                        uint32_t source_client_id,
                        uint32_t global_shot_id,
                        float pos_x,
                        float pos_y,
                        float pos_z,
                        float vel_x,
                        float vel_y,
                        float vel_z) {
        return sendServerPayload(peer,
                                 bz3::net::EncodeServerCreateShot(
                                     source_client_id,
                                     global_shot_id,
                                 bz3::net::Vec3{pos_x, pos_y, pos_z},
                                 bz3::net::Vec3{vel_x, vel_y, vel_z}));
    }

    bool sendRemoveShot(karma::network::PeerToken peer, uint32_t shot_id, bool is_global_id) {
        return sendServerPayload(peer, bz3::net::EncodeServerRemoveShot(shot_id, is_global_id));
    }

    bool sendWorldTransferBegin(karma::network::PeerToken peer,
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

    bool sendWorldTransferChunk(karma::network::PeerToken peer,
                                std::string_view transfer_id,
                                uint32_t chunk_index,
                                const std::vector<std::byte>& chunk_data) {
        return sendServerPayload(peer,
                                 bz3::net::EncodeServerWorldTransferChunk(transfer_id,
                                                                          chunk_index,
                                                                          chunk_data));
    }

    bool sendWorldTransferEnd(karma::network::PeerToken peer,
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

    bool sendWorldPackageChunked(karma::network::PeerToken peer,
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

    uint32_t allocateClientId() {
        while (isClientIdInUse(next_client_id_)) {
            ++next_client_id_;
        }
        return next_client_id_++;
    }

    bool isClientIdInUse(uint32_t client_id) const {
        for (const auto& [_, state] : client_by_peer_) {
            if (state.client_id == client_id) {
                return true;
            }
        }
        return false;
    }

    static constexpr size_t kMaxClients = 50;
    static constexpr size_t kNumChannels = 2;
    static constexpr uint32_t kFirstClientId = 2;

    std::unique_ptr<karma::network::ServerTransport> transport_{};
    uint16_t port_ = 0;
    bool initialized_ = false;
    uint32_t next_client_id_ = kFirstClientId;
    uint64_t next_transfer_id_ = 1;
    std::unordered_map<karma::network::PeerToken, ClientConnectionState> client_by_peer_{};
};

} // namespace

std::unique_ptr<ServerEventSource> CreateServerTransportEventSource(uint16_t port) {
    auto transport_source = std::make_unique<TransportServerEventSource>(port);
    if (!transport_source->initialized()) {
        return nullptr;
    }
    return transport_source;
}

} // namespace bz3::server::net
