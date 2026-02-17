#include "server/net/transport_event_source/internal.hpp"

#include "karma/common/logging.hpp"
#include "karma/network/transport_config_mapping.hpp"

#include <spdlog/spdlog.h>

namespace bz3::server::net::detail {

TransportServerEventSource::TransportServerEventSource(uint16_t port, std::string app_name)
    : port_(port),
      app_name_(std::move(app_name)) {
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

TransportServerEventSource::~TransportServerEventSource() = default;

bool TransportServerEventSource::initialized() const {
    return initialized_;
}

std::vector<ServerInputEvent> TransportServerEventSource::poll() {
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

karma::network::PeerToken TransportServerEventSource::findPeerByClientId(uint32_t client_id) {
    for (auto& [peer, state] : client_by_peer_) {
        if (state.client_id == client_id) {
            return peer;
        }
    }
    return 0;
}

uint32_t TransportServerEventSource::allocateClientId() {
    while (isClientIdInUse(next_client_id_)) {
        ++next_client_id_;
    }
    return next_client_id_++;
}

bool TransportServerEventSource::isClientIdInUse(uint32_t client_id) const {
    for (const auto& [_, state] : client_by_peer_) {
        if (state.client_id == client_id) {
            return true;
        }
    }
    return false;
}

} // namespace bz3::server::net::detail
