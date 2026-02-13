#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace bz3::net {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct SessionSnapshotEntry {
    uint32_t session_id = 0;
    std::string session_name{};
};

struct WorldManifestEntry {
    std::string path{};
    uint64_t size = 0;
    std::string hash{};
};

enum class ClientMessageType {
    JoinRequest,
    RequestPlayerSpawn,
    CreateShot,
    PlayerLeave,
    Unknown
};

struct ClientMessage {
    ClientMessageType type = ClientMessageType::Unknown;
    uint32_t client_id = 0;
    std::string player_name{};
    uint32_t protocol_version = 0;
    std::string cached_world_hash{};
    std::string cached_world_id{};
    std::string cached_world_revision{};
    std::string cached_world_content_hash{};
    std::string cached_world_manifest_hash{};
    uint32_t cached_world_manifest_file_count = 0;
    std::vector<WorldManifestEntry> cached_world_manifest{};
    uint32_t local_shot_id = 0;
    Vec3 shot_position{};
    Vec3 shot_velocity{};
};

enum class ServerMessageType {
    JoinResponse,
    Init,
    SessionSnapshot,
    PlayerSpawn,
    PlayerDeath,
    CreateShot,
    RemoveShot,
    WorldTransferBegin,
    WorldTransferChunk,
    WorldTransferEnd,
    Unknown
};

struct ServerMessage {
    ServerMessageType type = ServerMessageType::Unknown;
    bool join_accepted = false;
    std::string reason{};
    uint32_t client_id = 0;
    std::string server_name{};
    std::string world_name{};
    uint32_t protocol_version = 0;
    std::string world_hash{};
    uint64_t world_size = 0;
    std::string world_id{};
    std::string world_revision{};
    std::string world_content_hash{};
    std::string world_manifest_hash{};
    uint32_t world_manifest_file_count = 0;
    std::vector<WorldManifestEntry> world_manifest{};
    std::vector<std::byte> world_data{};
    std::string other_payload{};
    std::vector<SessionSnapshotEntry> sessions{};
    uint32_t event_client_id = 0;
    uint32_t event_shot_id = 0;
    bool event_shot_is_global = false;
    std::string transfer_id{};
    std::string transfer_world_id{};
    std::string transfer_world_revision{};
    std::string transfer_world_hash{};
    std::string transfer_world_content_hash{};
    bool transfer_is_delta = false;
    std::string transfer_delta_base_world_id{};
    std::string transfer_delta_base_world_revision{};
    std::string transfer_delta_base_world_hash{};
    std::string transfer_delta_base_world_content_hash{};
    uint64_t transfer_total_bytes = 0;
    uint32_t transfer_chunk_size = 0;
    uint32_t transfer_chunk_index = 0;
    uint32_t transfer_chunk_count = 0;
    std::vector<std::byte> transfer_chunk_data{};
};

std::optional<ClientMessage> DecodeClientMessage(const void* data, size_t size);
std::optional<ServerMessage> DecodeServerMessage(const void* data, size_t size);

std::vector<std::byte> EncodeClientJoinRequest(std::string_view player_name,
                                               uint32_t protocol_version,
                                               std::string_view cached_world_hash,
                                               std::string_view cached_world_id,
                                               std::string_view cached_world_revision,
                                               std::string_view cached_world_content_hash,
                                               std::string_view cached_world_manifest_hash,
                                               uint32_t cached_world_manifest_file_count,
                                               const std::vector<WorldManifestEntry>& cached_world_manifest = {});
std::vector<std::byte> EncodeClientRequestPlayerSpawn(uint32_t client_id);
std::vector<std::byte> EncodeClientCreateShot(uint32_t client_id,
                                              uint32_t local_shot_id,
                                              const Vec3& position,
                                              const Vec3& velocity);
std::vector<std::byte> EncodeClientLeave(uint32_t client_id);

std::vector<std::byte> EncodeServerJoinResponse(bool accepted, std::string_view reason);
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
                                        const std::vector<std::byte>& world_data);
std::vector<std::byte> EncodeServerSessionSnapshot(const std::vector<SessionSnapshotEntry>& sessions);
std::vector<std::byte> EncodeServerPlayerSpawn(uint32_t client_id);
std::vector<std::byte> EncodeServerPlayerDeath(uint32_t client_id);
std::vector<std::byte> EncodeServerCreateShot(uint32_t source_client_id,
                                              uint32_t global_shot_id,
                                              const Vec3& position,
                                              const Vec3& velocity);
std::vector<std::byte> EncodeServerRemoveShot(uint32_t shot_id, bool is_global_id);
std::vector<std::byte> EncodeServerWorldTransferBegin(std::string_view transfer_id,
                                                      std::string_view world_id,
                                                      std::string_view world_revision,
                                                      uint64_t total_bytes,
                                                      uint32_t chunk_size,
                                                      std::string_view world_hash,
                                                      std::string_view world_content_hash,
                                                      bool is_delta = false,
                                                      std::string_view delta_base_world_id = {},
                                                      std::string_view delta_base_world_revision = {},
                                                      std::string_view delta_base_world_hash = {},
                                                      std::string_view delta_base_world_content_hash = {});
std::vector<std::byte> EncodeServerWorldTransferChunk(std::string_view transfer_id,
                                                      uint32_t chunk_index,
                                                      const std::vector<std::byte>& chunk_data);
std::vector<std::byte> EncodeServerWorldTransferEnd(std::string_view transfer_id,
                                                    uint32_t chunk_count,
                                                    uint64_t total_bytes,
                                                    std::string_view world_hash,
                                                    std::string_view world_content_hash);

} // namespace bz3::net
