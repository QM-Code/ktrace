#include "client/net/connection.hpp"

#include "karma/common/logging/logging.hpp"

#include <spdlog/spdlog.h>

namespace bz3::client::net {

void ClientConnection::handleTransportDisconnected() {
    KARMA_TRACE("net.client",
                "ClientConnection: disconnected from {}:{}",
                host_,
                port_);
    connected_ = false;
    pending_world_package_ = {};
    active_world_transfer_ = {};
}

void ClientConnection::handleTransportConnected() {
    KARMA_TRACE("net.client",
                "ClientConnection: transport reconnected to {}:{}; replaying join bootstrap",
                host_,
                port_);
    connected_ = true;
    join_sent_ = false;
    leave_sent_ = false;
    assigned_client_id_ = 0;
    init_received_ = false;
    join_bootstrap_complete_logged_ = false;
    init_world_name_.clear();
    init_server_name_.clear();
    pending_world_package_ = {};
    active_world_transfer_ = {};
    if (!sendJoinRequest()) {
        spdlog::error("ClientConnection: failed to resend join request after reconnect");
        should_exit_ = true;
        requestDisconnect();
    }
}

} // namespace bz3::client::net
