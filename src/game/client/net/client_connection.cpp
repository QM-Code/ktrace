#include "client/net/client_connection.hpp"

#include "karma/common/config_helpers.hpp"
#include "karma/common/logging.hpp"
#include "net/protocol_codec.hpp"
#include "net/protocol.hpp"

#include <enet.h>
#include <spdlog/spdlog.h>

#include <mutex>
#include <string>
#include <utility>

namespace bz3::client::net {

namespace {

class EnetGlobal {
 public:
    EnetGlobal() {
        std::lock_guard<std::mutex> lock(mutex());
        if (refCount()++ == 0) {
            if (enet_initialize() != 0) {
                spdlog::error("ENet client: enet_initialize() failed");
                initialized_ = false;
                return;
            }
        }
        initialized_ = true;
    }

    ~EnetGlobal() {
        std::lock_guard<std::mutex> lock(mutex());
        auto& count = refCount();
        if (count == 0) {
            return;
        }
        if (--count == 0) {
            enet_deinitialize();
        }
    }

    bool initialized() const { return initialized_; }

 private:
    static std::mutex& mutex() {
        static std::mutex m;
        return m;
    }

    static uint32_t& refCount() {
        static uint32_t count = 0;
        return count;
    }

    bool initialized_ = false;
};

EnetGlobal& GlobalEnet() {
    static EnetGlobal instance;
    return instance;
}

} // namespace

ClientConnection::ClientConnection(std::string host, uint16_t port, std::string player_name)
    : host_(std::move(host)),
      port_(port),
      player_name_(std::move(player_name)) {}

ClientConnection::~ClientConnection() {
    shutdown();
}

bool ClientConnection::start() {
    if (started_) {
        return connected_;
    }
    started_ = true;

    if (host_.empty() || port_ == 0) {
        KARMA_TRACE("net.client",
                    "ClientConnection: startup skipped (missing host/port)");
        return false;
    }

    if (!GlobalEnet().initialized()) {
        spdlog::error("ClientConnection: ENet global init failed");
        return false;
    }

    host_handle_ = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!host_handle_) {
        spdlog::error("ClientConnection: failed to create ENet client host");
        return false;
    }

    ENetAddress address{};
    if (enet_address_set_host(&address, host_.c_str()) != 0) {
        spdlog::error("ClientConnection: failed to resolve host '{}'", host_);
        closeTransport();
        return false;
    }
    address.port = port_;

    peer_ = enet_host_connect(host_handle_, &address, 2, 0);
    if (!peer_) {
        spdlog::error("ClientConnection: failed to initiate connection to {}:{}", host_, port_);
        closeTransport();
        return false;
    }

    KARMA_TRACE("net.client",
                "ClientConnection: connecting to {}:{}",
                host_,
                port_);

    const uint16_t timeout_ms =
        karma::config::ReadUInt16Config({"network.ConnectTimeoutMs"}, static_cast<uint16_t>(2000));

    ENetEvent event{};
    if (enet_host_service(host_handle_, &event, timeout_ms) <= 0 ||
        event.type != ENET_EVENT_TYPE_CONNECT) {
        spdlog::error("ClientConnection: connection timed out to {}:{}", host_, port_);
        if (peer_) {
            enet_peer_reset(peer_);
            peer_ = nullptr;
        }
        closeTransport();
        return false;
    }

    connected_ = true;
    assigned_client_id_ = 0;
    init_received_ = false;
    join_bootstrap_complete_logged_ = false;
    init_world_name_.clear();
    init_server_name_.clear();
    KARMA_TRACE("net.client",
                "ClientConnection: connected to {}:{}",
                host_,
                port_);

    if (!sendJoinRequest()) {
        spdlog::error("ClientConnection: failed to send join request");
        shutdown();
        return false;
    }

    return true;
}

void ClientConnection::poll() {
    if (!connected_ || !host_handle_) {
        return;
    }

    ENetEvent event{};
    while (enet_host_service(host_handle_, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_RECEIVE: {
                if (event.packet && event.packet->data && event.packet->dataLength > 0) {
                    const auto message =
                        bz3::net::DecodeServerMessage(event.packet->data, event.packet->dataLength);
                    if (message.has_value()) {
                        switch (message->type) {
                            case bz3::net::ServerMessageType::JoinResponse:
                                if (message->join_accepted) {
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: join accepted by {}:{}",
                                                host_,
                                                port_);
                                } else {
                                    const std::string reason = message->reason.empty()
                                        ? std::string("Join rejected by server.")
                                        : message->reason;
                                    spdlog::error("ClientConnection: join rejected: {}", reason);
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: join rejected by {}:{} reason='{}'",
                                                host_,
                                                port_,
                                                reason);
                                    should_exit_ = true;
                                    enet_peer_disconnect(peer_, 0);
                                }
                                break;
                            case bz3::net::ServerMessageType::Init:
                                assigned_client_id_ = message->client_id;
                                init_received_ = true;
                                init_world_name_ = message->world_name;
                                init_server_name_ = message->server_name;
                                KARMA_TRACE("net.client",
                                            "ClientConnection: init world='{}' server='{}' client_id={} protocol={}",
                                            message->world_name,
                                            message->server_name,
                                            message->client_id,
                                            message->protocol_version);
                                if (message->protocol_version != bz3::net::kProtocolVersion) {
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: protocol mismatch server={} client={}",
                                                message->protocol_version,
                                                bz3::net::kProtocolVersion);
                                }
                                break;
                            case bz3::net::ServerMessageType::SessionSnapshot:
                                KARMA_TRACE("net.client",
                                            "ClientConnection: snapshot sessions={}",
                                            message->sessions.size());
                                for (const auto& session : message->sessions) {
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: snapshot session id={} name='{}'",
                                                session.session_id,
                                                session.session_name);
                                }
                                if (init_received_ && !join_bootstrap_complete_logged_) {
                                    join_bootstrap_complete_logged_ = true;
                                    KARMA_TRACE("net.client",
                                                "ClientConnection: join bootstrap complete client_id={} world='{}' server='{}' sessions={}",
                                                assigned_client_id_,
                                                init_world_name_,
                                                init_server_name_,
                                                message->sessions.size());
                                }
                                break;
                            default:
                                KARMA_TRACE("net.client",
                                            "ClientConnection: server message payload={} bytes={}",
                                            message->other_payload,
                                            event.packet->dataLength);
                                break;
                        }
                    } else {
                        KARMA_TRACE("net.client",
                                    "ClientConnection: invalid server payload bytes={}",
                                    event.packet ? event.packet->dataLength : 0);
                    }
                } else {
                    KARMA_TRACE("net.client",
                                "ClientConnection: invalid server payload bytes={}",
                                event.packet ? event.packet->dataLength : 0);
                }
                if (event.packet) {
                    enet_packet_destroy(event.packet);
                }
                break;
            }
            case ENET_EVENT_TYPE_DISCONNECT:
            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                KARMA_TRACE("net.client",
                            "ClientConnection: disconnected from {}:{}",
                            host_,
                            port_);
                connected_ = false;
                peer_ = nullptr;
                break;
            default:
                break;
        }
    }
}

void ClientConnection::shutdown() {
    if (!started_) {
        return;
    }
    started_ = false;

    if (connected_) {
        static_cast<void>(sendLeave());
    }

    if (peer_) {
        enet_peer_disconnect(peer_, 0);

        ENetEvent event{};
        bool disconnected = false;
        while (enet_host_service(host_handle_, &event, 50) > 0) {
            if (event.type == ENET_EVENT_TYPE_RECEIVE && event.packet) {
                enet_packet_destroy(event.packet);
            }
            if (event.type == ENET_EVENT_TYPE_DISCONNECT ||
                event.type == ENET_EVENT_TYPE_DISCONNECT_TIMEOUT) {
                disconnected = true;
                break;
            }
        }

        if (!disconnected) {
            enet_peer_reset(peer_);
        }
        peer_ = nullptr;
    }

    connected_ = false;
    closeTransport();
}

bool ClientConnection::isConnected() const {
    return connected_;
}

bool ClientConnection::shouldExit() const {
    return should_exit_;
}

bool ClientConnection::sendJoinRequest() {
    if (!connected_ || !peer_ || !host_handle_ || join_sent_) {
        return connected_ && join_sent_;
    }

    const auto payload =
        bz3::net::EncodeClientJoinRequest(player_name_, bz3::net::kProtocolVersion);
    if (payload.empty()) {
        return false;
    }

    ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
    if (!packet) {
        return false;
    }
    if (enet_peer_send(peer_, 0, packet) != 0) {
        enet_packet_destroy(packet);
        return false;
    }
    enet_host_flush(host_handle_);
    join_sent_ = true;

    KARMA_TRACE("net.client",
                "ClientConnection: sent join request name='{}' protocol={} to {}:{}",
                player_name_,
                bz3::net::kProtocolVersion,
                host_,
                port_);
    return true;
}

bool ClientConnection::sendLeave() {
    if (!connected_ || !peer_ || !host_handle_ || leave_sent_) {
        return connected_ && leave_sent_;
    }

    const auto payload = bz3::net::EncodeClientLeave(assigned_client_id_);
    if (payload.empty()) {
        return false;
    }

    ENetPacket* packet = enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
    if (!packet) {
        return false;
    }
    if (enet_peer_send(peer_, 0, packet) != 0) {
        enet_packet_destroy(packet);
        return false;
    }
    enet_host_flush(host_handle_);
    leave_sent_ = true;
    KARMA_TRACE("net.client",
                "ClientConnection: sent leave");
    return true;
}

void ClientConnection::closeTransport() {
    if (host_handle_) {
        enet_host_destroy(host_handle_);
        host_handle_ = nullptr;
    }
}

} // namespace bz3::client::net
