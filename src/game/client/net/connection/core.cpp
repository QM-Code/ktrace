#include "client/net/connection.hpp"

#include "karma/network/config/reconnect_policy.hpp"
#include "karma/network/transport/client.hpp"
#include "karma/network/config/transport_mapping.hpp"
#include "karma/common/config/helpers.hpp"
#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

#include <string>
#include <utility>

namespace bz3::client::net {

ClientConnection::ClientConnection(std::string host,
                                   uint16_t port,
                                   std::string player_name,
                                   std::string auth_payload,
                                   AudioEventCallback audio_event_callback)
    : host_(std::move(host)),
      port_(port),
      player_name_(std::move(player_name)),
      auth_payload_(std::move(auth_payload)),
      audio_event_callback_(std::move(audio_event_callback)) {}

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

    bool custom_backend = false;
    karma::network::ClientTransportConfig transport_config =
        karma::network::ResolveClientTransportConfigFromConfig(&custom_backend);
    if (custom_backend) {
        KARMA_TRACE("net.client",
                    "ClientConnection: using custom client transport backend='{}'",
                    transport_config.backend_name);
    }

    transport_ = karma::network::CreateClientTransport(transport_config);
    if (!transport_ || !transport_->isReady()) {
        const std::string configured_backend =
            karma::network::EffectiveClientTransportBackendName(transport_config);
        spdlog::error("ClientConnection: failed to create client transport backend={}",
                      configured_backend);
        closeTransport();
        return false;
    }

    KARMA_TRACE("net.client",
                "ClientConnection: connecting to {}:{}",
                host_,
                port_);

    const uint32_t timeout_ms = static_cast<uint32_t>(
        karma::common::config::ReadUInt16Config({"network.ConnectTimeoutMs"}, static_cast<uint16_t>(2000)));
    const karma::network::ClientReconnectPolicy reconnect_policy =
        karma::network::ReadClientReconnectPolicyFromConfig();
    karma::network::ClientTransportConnectOptions connect_options{
        .host = host_,
        .port = port_,
        .timeout_ms = timeout_ms};
    karma::network::ApplyReconnectPolicyToConnectOptions(reconnect_policy, &connect_options);

    if (!transport_->connect(connect_options)) {
        spdlog::error("ClientConnection: connection timed out to {}:{}", host_, port_);
        closeTransport();
        return false;
    }

    connected_ = true;
    assigned_client_id_ = 0;
    init_received_ = false;
    join_bootstrap_complete_logged_ = false;
    init_world_name_.clear();
    init_server_name_.clear();
    pending_world_package_ = {};
    active_world_transfer_ = {};
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

void ClientConnection::shutdown() {
    if (!started_) {
        return;
    }
    started_ = false;

    if (connected_) {
        static_cast<void>(sendLeave());
    }

    if (transport_ && transport_->isConnected()) {
        transport_->disconnect(0);
        const bool disconnected = transport_->waitForDisconnect(50);
        if (!disconnected) {
            transport_->resetConnection();
        }
    }

    connected_ = false;
    pending_world_package_ = {};
    active_world_transfer_ = {};
    closeTransport();
}

bool ClientConnection::isConnected() const {
    return connected_;
}

bool ClientConnection::shouldExit() const {
    return should_exit_;
}

void ClientConnection::closeTransport() {
    if (transport_) {
        transport_->close();
        transport_.reset();
    }
}

} // namespace bz3::client::net
