#include "client/net/connection.hpp"

#include "karma/common/logging.hpp"
#include "karma/network/client_transport.hpp"
#include "net/protocol_codec.hpp"

#include <vector>

namespace bz3::client::net {

void ClientConnection::poll() {
    if (!connected_ || !transport_) {
        return;
    }

    std::vector<karma::network::ClientTransportEvent> transport_events{};
    transport_->poll(karma::network::ClientTransportPollOptions{}, &transport_events);
    for (const auto& transport_event : transport_events) {
        switch (transport_event.type) {
            case karma::network::ClientTransportEventType::Received:
                handleIncomingPayload(transport_event.payload);
                break;
            case karma::network::ClientTransportEventType::Disconnected:
                handleTransportDisconnected();
                break;
            case karma::network::ClientTransportEventType::Connected:
                handleTransportConnected();
                break;
            default:
                break;
        }
    }
}

void ClientConnection::requestDisconnect() {
    if (transport_) {
        transport_->disconnect(0);
    }
}

void ClientConnection::handleIncomingPayload(const std::vector<std::byte>& payload) {
    if (!payload.empty()) {
        const auto message = bz3::net::DecodeServerMessage(payload.data(), payload.size());
        if (message.has_value()) {
            handleServerMessage(*message, payload.size());
        } else {
            KARMA_TRACE("net.client",
                        "ClientConnection: invalid server payload bytes={}",
                        payload.size());
        }
    } else {
        KARMA_TRACE("net.client",
                    "ClientConnection: invalid server payload bytes={}",
                    payload.size());
    }
}

void ClientConnection::handleServerMessage(const bz3::net::ServerMessage& message, size_t payload_bytes) {
    switch (message.type) {
        case bz3::net::ServerMessageType::JoinResponse:
            handleJoinResponse(message);
            break;
        case bz3::net::ServerMessageType::Init:
            handleInit(message);
            break;
        case bz3::net::ServerMessageType::WorldTransferBegin:
            handleWorldTransferBegin(message);
            break;
        case bz3::net::ServerMessageType::WorldTransferChunk:
            handleWorldTransferChunk(message);
            break;
        case bz3::net::ServerMessageType::WorldTransferEnd:
            handleWorldTransferEnd(message);
            break;
        case bz3::net::ServerMessageType::SessionSnapshot:
            handleSessionSnapshot(message);
            break;
        case bz3::net::ServerMessageType::PlayerSpawn:
            handlePlayerSpawn(message);
            break;
        case bz3::net::ServerMessageType::PlayerDeath:
            handlePlayerDeath(message);
            break;
        case bz3::net::ServerMessageType::CreateShot:
            handleCreateShot(message);
            break;
        case bz3::net::ServerMessageType::RemoveShot:
            handleRemoveShot(message);
            break;
        default:
            KARMA_TRACE("net.client",
                        "ClientConnection: server message payload={} bytes={}",
                        message.other_payload,
                        payload_bytes);
            break;
    }
}

} // namespace bz3::client::net
