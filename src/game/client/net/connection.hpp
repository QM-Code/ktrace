#pragma once

#include "karma/network/content/transfer_receiver.hpp"
#include "net/protocol_codec.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace karma::network {
class ClientTransport;
}

namespace bz3::client::net {

enum class AudioEvent {
    PlayerSpawn,
    PlayerDeath,
    ShotFire
};

class ClientConnection {
 public:
    using AudioEventCallback = std::function<void(AudioEvent)>;

    ClientConnection(std::string host,
                     uint16_t port,
                     std::string player_name,
                     std::string auth_payload = {},
                     AudioEventCallback audio_event_callback = {});
    ~ClientConnection();

    ClientConnection(const ClientConnection&) = delete;
    ClientConnection& operator=(const ClientConnection&) = delete;
    ClientConnection(ClientConnection&&) = delete;
    ClientConnection& operator=(ClientConnection&&) = delete;

    bool start();
    void poll();
    void shutdown();
    bool isConnected() const;
    bool shouldExit() const;
    bool sendRequestPlayerSpawn();
    bool sendCreateShot();

 private:
    void requestDisconnect();
    void handleIncomingPayload(const std::vector<std::byte>& payload);
    void handleServerMessage(const bz3::net::ServerMessage& message, size_t payload_bytes);
    void handleJoinResponse(const bz3::net::ServerMessage& message);
    void handleInit(const bz3::net::ServerMessage& message);
    void handleWorldTransferBegin(const bz3::net::ServerMessage& message);
    void handleWorldTransferChunk(const bz3::net::ServerMessage& message);
    void handleWorldTransferEnd(const bz3::net::ServerMessage& message);
    void handleSessionSnapshot(const bz3::net::ServerMessage& message);
    void handlePlayerSpawn(const bz3::net::ServerMessage& message);
    void handlePlayerDeath(const bz3::net::ServerMessage& message);
    void handleCreateShot(const bz3::net::ServerMessage& message);
    void handleRemoveShot(const bz3::net::ServerMessage& message);
    void handleTransportDisconnected();
    void handleTransportConnected();

    bool sendJoinRequest();
    bool sendLeave();
    bool sendPayloadReliable(const std::vector<std::byte>& payload);
    void closeTransport();

    std::unique_ptr<karma::network::ClientTransport> transport_{};

    std::string host_;
    uint16_t port_ = 0;
    std::string player_name_;
    std::string auth_payload_;

    bool started_ = false;
    bool connected_ = false;
    bool join_sent_ = false;
    bool leave_sent_ = false;
    bool should_exit_ = false;
    uint32_t assigned_client_id_ = 0;
    bool init_received_ = false;
    bool join_bootstrap_complete_logged_ = false;
    std::string init_world_name_{};
    std::string init_server_name_{};
    struct PendingWorldPackageState {
        bool active = false;
        std::string world_name{};
        std::string world_id{};
        std::string world_revision{};
        std::string world_hash{};
        std::string world_content_hash{};
        std::string world_manifest_hash{};
        uint32_t world_manifest_file_count = 0;
        uint64_t world_size = 0;
        std::vector<bz3::net::WorldManifestEntry> world_manifest{};
    };
    PendingWorldPackageState pending_world_package_{};
    karma::network::content::TransferReceiverState active_world_transfer_{};
    AudioEventCallback audio_event_callback_{};
    uint32_t next_local_shot_id_ = 1;
};

} // namespace bz3::client::net
