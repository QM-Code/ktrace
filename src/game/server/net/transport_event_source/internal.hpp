#pragma once

#include "server/net/event_source.hpp"

#include "karma/network/server_transport.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace bz3::net {
struct ClientMessage;
}

namespace bz3::server::net::detail {

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

inline constexpr const char* kDeltaRemovedPathsFile = "__bz3_delta_removed_paths.txt";
inline constexpr const char* kDeltaMetaFile = "__bz3_delta_meta.txt";

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

std::string DefaultPlayerName(uint32_t client_id);
std::string ResolvePlayerName(const bz3::net::ClientMessage& message, uint32_t client_id);
bool NormalizeRelativePath(std::string_view raw_path, std::filesystem::path* out);
ManifestDiffPlan BuildServerManifestDiffPlan(const std::vector<WorldManifestEntry>& cached_manifest,
                                             const std::vector<WorldManifestEntry>& incoming_manifest);
void LogServerManifestDiffPlan(uint32_t client_id,
                               std::string_view world_name,
                               const ManifestDiffPlan& plan);
std::optional<std::vector<std::byte>> BuildWorldDeltaArchive(
    const std::filesystem::path& world_dir,
    const ManifestDiffPlan& diff_plan,
    std::string_view world_id,
    std::string_view target_world_revision,
    std::string_view base_world_revision);

class TransportServerEventSource final : public ServerEventSource {
 public:
    TransportServerEventSource(uint16_t port, std::string app_name);
    ~TransportServerEventSource() override;

    bool initialized() const;
    std::vector<ServerInputEvent> poll() override;

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
                      const std::vector<std::byte>& world_package) override;

    void onPlayerSpawn(uint32_t client_id) override;
    void onPlayerDeath(uint32_t client_id) override;
    void onCreateShot(uint32_t source_client_id,
                      uint32_t global_shot_id,
                      float pos_x,
                      float pos_y,
                      float pos_z,
                      float vel_x,
                      float vel_y,
                      float vel_z) override;
    void onRemoveShot(uint32_t shot_id, bool is_global_id) override;

 private:
    void emitJoinEvent(uint32_t client_id,
                       const std::string& player_name,
                       const std::string& auth_payload,
                       const std::string& peer_ip,
                       uint16_t peer_port,
                       std::vector<ServerInputEvent>& out);
    void emitLeaveEvent(uint32_t client_id, std::vector<ServerInputEvent>& out);
    void emitRequestSpawnEvent(uint32_t client_id, std::vector<ServerInputEvent>& out);
    void emitCreateShotEvent(uint32_t client_id,
                             uint32_t local_shot_id,
                             float pos_x,
                             float pos_y,
                             float pos_z,
                             float vel_x,
                             float vel_y,
                             float vel_z,
                             std::vector<ServerInputEvent>& out);

    void handleReceiveEvent(const karma::network::ServerTransportEvent& event,
                            std::vector<ServerInputEvent>& out);

    template <typename SenderFn>
    size_t broadcastToJoined(SenderFn&& sender) {
        size_t sent = 0;
        for (auto& [peer, state] : client_by_peer_) {
            if (!peer || !state.joined) {
                continue;
            }
            if (std::forward<SenderFn>(sender)(peer)) {
                ++sent;
            }
        }
        return sent;
    }

    karma::network::PeerToken findPeerByClientId(uint32_t client_id);

    bool sendServerPayload(karma::network::PeerToken peer, const std::vector<std::byte>& payload);
    bool sendJoinResponse(karma::network::PeerToken peer, bool accepted, std::string_view reason);
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
                  const std::vector<std::byte>& world_package);
    bool sendSessionSnapshot(karma::network::PeerToken peer,
                             const std::vector<SessionSnapshotEntry>& sessions);
    bool sendPlayerSpawn(karma::network::PeerToken peer, uint32_t client_id);
    bool sendPlayerDeath(karma::network::PeerToken peer, uint32_t client_id);
    bool sendCreateShot(karma::network::PeerToken peer,
                        uint32_t source_client_id,
                        uint32_t global_shot_id,
                        float pos_x,
                        float pos_y,
                        float pos_z,
                        float vel_x,
                        float vel_y,
                        float vel_z);
    bool sendRemoveShot(karma::network::PeerToken peer, uint32_t shot_id, bool is_global_id);
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
                                std::string_view delta_base_world_content_hash);
    bool sendWorldTransferChunk(karma::network::PeerToken peer,
                                std::string_view transfer_id,
                                uint32_t chunk_index,
                                const std::vector<std::byte>& chunk_data);
    bool sendWorldTransferEnd(karma::network::PeerToken peer,
                              std::string_view transfer_id,
                              uint32_t chunk_count,
                              uint64_t total_bytes,
                              std::string_view world_hash,
                              std::string_view world_content_hash);
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
                                 std::string_view delta_base_world_content_hash);

    uint32_t allocateClientId();
    bool isClientIdInUse(uint32_t client_id) const;

    static constexpr size_t kMaxClients = 50;
    static constexpr size_t kNumChannels = 2;
    static constexpr uint32_t kFirstClientId = 2;

    std::unique_ptr<karma::network::ServerTransport> transport_{};
    uint16_t port_ = 0;
    std::string app_name_{};
    bool initialized_ = false;
    uint32_t next_client_id_ = kFirstClientId;
    uint64_t next_transfer_id_ = 1;
    std::unordered_map<karma::network::PeerToken, ClientConnectionState> client_by_peer_{};
};

} // namespace bz3::server::net::detail
