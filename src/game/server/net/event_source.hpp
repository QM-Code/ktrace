#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "server/cli_options.hpp"

namespace bz3::server::net {

struct ClientJoinEvent {
    uint32_t client_id = 0;
    std::string player_name{};
};

struct ClientLeaveEvent {
    uint32_t client_id = 0;
};

struct ClientRequestSpawnEvent {
    uint32_t client_id = 0;
};

struct ClientCreateShotEvent {
    uint32_t client_id = 0;
    uint32_t local_shot_id = 0;
    float pos_x = 0.0f;
    float pos_y = 0.0f;
    float pos_z = 0.0f;
    float vel_x = 0.0f;
    float vel_y = 0.0f;
    float vel_z = 0.0f;
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

struct ServerInputEvent {
    enum class Type {
        ClientJoin,
        ClientLeave,
        ClientRequestSpawn,
        ClientCreateShot
    };

    Type type = Type::ClientJoin;
    ClientJoinEvent join{};
    ClientLeaveEvent leave{};
    ClientRequestSpawnEvent request_spawn{};
    ClientCreateShotEvent create_shot{};
};

class ServerEventSource {
 public:
    virtual ~ServerEventSource() = default;
    virtual std::vector<ServerInputEvent> poll() = 0;
    virtual void onJoinResult(uint32_t client_id,
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
        (void)client_id;
        (void)accepted;
        (void)reason;
        (void)world_name;
        (void)world_id;
        (void)world_revision;
        (void)world_package_hash;
        (void)world_content_hash;
        (void)world_manifest_hash;
        (void)world_manifest_file_count;
        (void)world_package_size;
        (void)world_dir;
        (void)sessions;
        (void)world_manifest;
        (void)world_package;
    }

    virtual void onPlayerSpawn(uint32_t client_id) {
        (void)client_id;
    }

    virtual void onPlayerDeath(uint32_t client_id) {
        (void)client_id;
    }

    virtual void onCreateShot(uint32_t source_client_id,
                              uint32_t global_shot_id,
                              float pos_x,
                              float pos_y,
                              float pos_z,
                              float vel_x,
                              float vel_y,
                              float vel_z) {
        (void)source_client_id;
        (void)global_shot_id;
        (void)pos_x;
        (void)pos_y;
        (void)pos_z;
        (void)vel_x;
        (void)vel_y;
        (void)vel_z;
    }
};

std::unique_ptr<ServerEventSource> CreateServerEventSource(const CLIOptions& options);

} // namespace bz3::server::net
