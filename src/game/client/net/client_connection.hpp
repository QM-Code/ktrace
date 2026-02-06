#pragma once

#include <cstdint>
#include <string>

struct _ENetHost;
struct _ENetPeer;

namespace bz3::client::net {

class ClientConnection {
 public:
    ClientConnection(std::string host, uint16_t port, std::string player_name);
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

 private:
    bool sendJoinRequest();
    bool sendLeave();
    void closeTransport();

    _ENetHost* host_handle_ = nullptr;
    _ENetPeer* peer_ = nullptr;

    std::string host_;
    uint16_t port_ = 0;
    std::string player_name_;

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
};

} // namespace bz3::client::net
