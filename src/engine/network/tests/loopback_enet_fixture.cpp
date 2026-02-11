#include "network/tests/loopback_enet_fixture.hpp"

#include <enet.h>

#include <chrono>
#include <mutex>
#include <thread>

namespace karma::network::tests {

namespace {

bool EnsureLoopbackEnetInitialized(std::string* out_error) {
    static std::once_flag init_once;
    static bool init_ok = false;
    std::call_once(init_once, []() { init_ok = (enet_initialize() == 0); });
    if (!init_ok && out_error) {
        *out_error = "enet_initialize failed";
    }
    return init_ok;
}

void PumpLoopbackEndpointInternal(LoopbackEnetEndpoint* endpoint,
                                  std::vector<std::vector<std::byte>>* out_payloads) {
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
                if (event.packet && out_payloads && event.packet->data && event.packet->dataLength > 0) {
                    std::vector<std::byte> payload{};
                    payload.insert(payload.end(),
                                   reinterpret_cast<const std::byte*>(event.packet->data),
                                   reinterpret_cast<const std::byte*>(event.packet->data)
                                       + event.packet->dataLength);
                    out_payloads->push_back(std::move(payload));
                }
                if (event.packet) {
                    enet_packet_destroy(event.packet);
                }
                break;
            default:
                break;
        }
    }
}

} // namespace

bool InitializeLoopbackEnet(std::string* out_error) {
    return EnsureLoopbackEnetInitialized(out_error);
}

std::optional<LoopbackEnetEndpoint> CreateLoopbackServerEndpointAtPort(uint16_t port,
                                                                        size_t max_peers,
                                                                        size_t channel_count) {
    if (!EnsureLoopbackEnetInitialized(nullptr)) {
        return std::nullopt;
    }
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
    if (!EnsureLoopbackEnetInitialized(nullptr)) {
        return std::nullopt;
    }
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
    PumpLoopbackEndpointInternal(endpoint, nullptr);
}

void PumpLoopbackEndpointCapturePayloads(LoopbackEnetEndpoint* endpoint,
                                         std::vector<std::vector<std::byte>>* out_payloads) {
    PumpLoopbackEndpointInternal(endpoint, out_payloads);
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

bool DisconnectLoopbackEndpoint(LoopbackEnetEndpoint* endpoint, uint32_t data) {
    if (!endpoint || !endpoint->peer) {
        return false;
    }
    enet_peer_disconnect(endpoint->peer, data);
    return true;
}

uint16_t GetLoopbackEndpointBoundPort(const LoopbackEnetEndpoint* endpoint) {
    if (!endpoint || !endpoint->host) {
        return 0;
    }
    return endpoint->host->address.port;
}

bool LoopbackEndpointHasPeer(const LoopbackEnetEndpoint* endpoint) {
    return endpoint && endpoint->peer != nullptr;
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

void StopLoopbackEndpointPumpThread(LoopbackPumpThread* pump) {
    if (!pump) {
        return;
    }
    pump->keep_running.store(false);
    if (pump->thread.joinable()) {
        pump->thread.join();
    }
}

bool StartLoopbackEndpointPumpThread(LoopbackPumpThread* pump,
                                     LoopbackEnetEndpoint* endpoint,
                                     std::chrono::milliseconds sleep_interval) {
    if (!pump || !endpoint || !endpoint->host) {
        return false;
    }
    StopLoopbackEndpointPumpThread(pump);
    pump->keep_running.store(true);
    pump->thread = std::thread([pump, endpoint, sleep_interval]() {
        while (pump->keep_running.load()) {
            PumpLoopbackEndpoint(endpoint);
            std::this_thread::sleep_for(sleep_interval);
        }
    });
    return true;
}

bool RunBoundedProbeLoop(const BoundedProbeLoopOptions& options,
                         const std::function<void()>& send_probe,
                         const std::function<bool()>& poll_once,
                         const std::function<bool()>& done,
                         BoundedProbeLoopDiagnostics* diagnostics) {
    if (!poll_once || !done) {
        return false;
    }

    BoundedProbeLoopDiagnostics local_diag{};
    while (std::chrono::steady_clock::now() < options.deadline) {
        ++local_diag.poll_loops;
        if (send_probe && options.probe_interval_polls > 0 &&
            local_diag.probe_sends < options.probe_max_sends &&
            (local_diag.poll_loops % options.probe_interval_polls) == 0) {
            send_probe();
            ++local_diag.probe_sends;
        }
        if (!poll_once()) {
            if (diagnostics) {
                *diagnostics = local_diag;
            }
            return false;
        }
        if (done()) {
            if (diagnostics) {
                *diagnostics = local_diag;
            }
            return true;
        }
        std::this_thread::sleep_for(options.poll_sleep);
    }

    if (diagnostics) {
        *diagnostics = local_diag;
    }
    return false;
}

BoundedProbePhaseResult RunBoundedProbePhase(const BoundedProbeLoopOptions& options,
                                             const std::function<void()>& send_probe,
                                             const std::function<bool(std::string* out_error)>& poll_once,
                                             const std::function<bool()>& done) {
    BoundedProbePhaseResult result{};
    if (!poll_once || !done) {
        result.failure_message = "invalid bounded probe phase callbacks";
        return result;
    }

    std::string phase_error{};
    result.completed = RunBoundedProbeLoop(
        options,
        send_probe,
        [&]() { return poll_once(&phase_error); },
        done,
        &result.diagnostics);
    result.failure_message = std::move(phase_error);
    return result;
}

} // namespace karma::network::tests
