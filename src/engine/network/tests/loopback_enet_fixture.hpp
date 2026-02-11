#pragma once

#include <enet.h>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace karma::network::tests {

struct LoopbackEnetEndpoint {
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;
    bool connected = false;
    bool disconnected = false;
    uint32_t connect_events = 0;
    uint32_t disconnect_events = 0;
};

std::optional<LoopbackEnetEndpoint> CreateLoopbackServerEndpointAtPort(
    uint16_t port,
    size_t max_peers = 64,
    size_t channel_count = 2);

std::optional<LoopbackEnetEndpoint> CreateLoopbackServerEndpointAtPortWithRetry(
    uint16_t port,
    uint32_t attempts = 40,
    uint32_t retry_sleep_ms = 5,
    size_t max_peers = 64,
    size_t channel_count = 2);

std::optional<LoopbackEnetEndpoint> CreateLoopbackClientEndpoint(uint16_t port,
                                                                 size_t channel_count = 2);

std::optional<LoopbackEnetEndpoint> CreateLoopbackClientEndpointWithRetry(
    uint16_t port,
    uint32_t attempts = 24,
    uint32_t retry_sleep_ms = 2,
    size_t channel_count = 2);

void DestroyLoopbackEndpoint(LoopbackEnetEndpoint* endpoint);

void PumpLoopbackEndpoint(LoopbackEnetEndpoint* endpoint);

bool SendLoopbackPayload(LoopbackEnetEndpoint* endpoint,
                         const std::vector<std::byte>& payload);

bool DecodeLoopbackPayloadPair(const std::vector<std::byte>& payload,
                               uint8_t* out_first,
                               uint8_t* out_second);

} // namespace karma::network::tests
