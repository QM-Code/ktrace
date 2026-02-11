#include "network/tests/loopback_enet_fixture.hpp"

#include <chrono>
#include <thread>

namespace karma::network::tests {

std::optional<LoopbackEnetEndpoint> CreateLoopbackServerEndpointAtPort(uint16_t port,
                                                                        size_t max_peers,
                                                                        size_t channel_count) {
    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    address.port = port;
    ENetHost* host = enet_host_create(&address, max_peers, channel_count, 0, 0);
    if (!host) {
        return std::nullopt;
    }

    LoopbackEnetEndpoint endpoint{};
    endpoint.host = host;
    return endpoint;
}

std::optional<LoopbackEnetEndpoint> CreateLoopbackServerEndpointAtPortWithRetry(uint16_t port,
                                                                                 uint32_t attempts,
                                                                                 uint32_t retry_sleep_ms,
                                                                                 size_t max_peers,
                                                                                 size_t channel_count) {
    for (uint32_t attempt = 0; attempt < attempts; ++attempt) {
        auto endpoint = CreateLoopbackServerEndpointAtPort(port, max_peers, channel_count);
        if (endpoint.has_value()) {
            return endpoint;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_sleep_ms));
    }
    return std::nullopt;
}

std::optional<LoopbackEnetEndpoint> CreateLoopbackClientEndpoint(uint16_t port,
                                                                 size_t channel_count) {
    ENetHost* host = enet_host_create(nullptr, 1, channel_count, 0, 0);
    if (!host) {
        return std::nullopt;
    }

    ENetAddress address{};
    if (enet_address_set_host(&address, "127.0.0.1") != 0) {
        enet_host_destroy(host);
        return std::nullopt;
    }
    address.port = port;

    ENetPeer* peer = enet_host_connect(host, &address, channel_count, 0);
    if (!peer) {
        enet_host_destroy(host);
        return std::nullopt;
    }

    LoopbackEnetEndpoint endpoint{};
    endpoint.host = host;
    endpoint.peer = peer;
    return endpoint;
}

std::optional<LoopbackEnetEndpoint> CreateLoopbackClientEndpointWithRetry(uint16_t port,
                                                                           uint32_t attempts,
                                                                           uint32_t retry_sleep_ms,
                                                                           size_t channel_count) {
    for (uint32_t attempt = 0; attempt < attempts; ++attempt) {
        auto endpoint = CreateLoopbackClientEndpoint(port, channel_count);
        if (endpoint.has_value()) {
            return endpoint;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(retry_sleep_ms));
    }
    return std::nullopt;
}

void DestroyLoopbackEndpoint(LoopbackEnetEndpoint* endpoint) {
    if (!endpoint) {
        return;
    }
    if (endpoint->host) {
        enet_host_destroy(endpoint->host);
    }
    endpoint->host = nullptr;
    endpoint->peer = nullptr;
    endpoint->connected = false;
    endpoint->disconnected = false;
}

void PumpLoopbackEndpoint(LoopbackEnetEndpoint* endpoint) {
    if (!endpoint || !endpoint->host) {
        return;
    }

    ENetEvent event{};
    while (enet_host_service(endpoint->host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                endpoint->peer = event.peer;
                endpoint->connected = true;
                ++endpoint->connect_events;
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                endpoint->connected = false;
                endpoint->disconnected = true;
                if (endpoint->peer == event.peer) {
                    endpoint->peer = nullptr;
                }
                ++endpoint->disconnect_events;
                break;
            case ENET_EVENT_TYPE_RECEIVE:
                if (event.packet) {
                    enet_packet_destroy(event.packet);
                }
                break;
            default:
                break;
        }
    }
}

bool SendLoopbackPayload(LoopbackEnetEndpoint* endpoint, const std::vector<std::byte>& payload) {
    if (!endpoint || !endpoint->host || !endpoint->peer || payload.empty()) {
        return false;
    }

    ENetPacket* packet =
        enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
    if (!packet) {
        return false;
    }
    if (enet_peer_send(endpoint->peer, 0, packet) != 0) {
        enet_packet_destroy(packet);
        return false;
    }
    enet_host_flush(endpoint->host);
    return true;
}

bool DecodeLoopbackPayloadPair(const std::vector<std::byte>& payload,
                               uint8_t* out_first,
                               uint8_t* out_second) {
    if (payload.size() < 2 || !out_first || !out_second) {
        return false;
    }
    *out_first = std::to_integer<uint8_t>(payload[0]);
    *out_second = std::to_integer<uint8_t>(payload[1]);
    return true;
}

} // namespace karma::network::tests
