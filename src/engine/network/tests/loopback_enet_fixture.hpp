#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <thread>
#include <vector>

typedef struct _ENetHost ENetHost;
typedef struct _ENetPeer ENetPeer;

namespace karma::network::tests {

struct LoopbackEnetEndpoint {
    ENetHost* host = nullptr;
    ENetPeer* peer = nullptr;
    bool connected = false;
    bool disconnected = false;
    uint32_t connect_events = 0;
    uint32_t disconnect_events = 0;
};

bool InitializeLoopbackEnet(std::string* out_error = nullptr);

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

void PumpLoopbackEndpointCapturePayloads(LoopbackEnetEndpoint* endpoint,
                                         std::vector<std::vector<std::byte>>* out_payloads);

bool SendLoopbackPayload(LoopbackEnetEndpoint* endpoint,
                         const std::vector<std::byte>& payload);

bool DisconnectLoopbackEndpoint(LoopbackEnetEndpoint* endpoint, uint32_t data = 0);

uint16_t GetLoopbackEndpointBoundPort(const LoopbackEnetEndpoint* endpoint);

bool LoopbackEndpointHasPeer(const LoopbackEnetEndpoint* endpoint);

bool DecodeLoopbackPayloadPair(const std::vector<std::byte>& payload,
                               uint8_t* out_first,
                               uint8_t* out_second);

struct LoopbackPumpThread {
    std::atomic<bool> keep_running{false};
    std::thread thread{};
};

bool StartLoopbackEndpointPumpThread(LoopbackPumpThread* pump,
                                     LoopbackEnetEndpoint* endpoint,
                                     std::chrono::milliseconds sleep_interval =
                                         std::chrono::milliseconds(1));

void StopLoopbackEndpointPumpThread(LoopbackPumpThread* pump);

struct BoundedProbeLoopOptions {
    std::chrono::steady_clock::time_point deadline{};
    std::chrono::milliseconds poll_sleep = std::chrono::milliseconds(1);
    uint32_t probe_interval_polls = 0;
    uint32_t probe_max_sends = 0;
};

struct BoundedProbeLoopDiagnostics {
    uint32_t poll_loops = 0;
    uint32_t probe_sends = 0;
};

bool RunBoundedProbeLoop(const BoundedProbeLoopOptions& options,
                         const std::function<void()>& send_probe,
                         const std::function<bool()>& poll_once,
                         const std::function<bool()>& done,
                         BoundedProbeLoopDiagnostics* diagnostics);

struct BoundedProbePhaseResult {
    bool completed = false;
    BoundedProbeLoopDiagnostics diagnostics{};
    std::string failure_message{};
};

BoundedProbePhaseResult RunBoundedProbePhase(const BoundedProbeLoopOptions& options,
                                             const std::function<void()>& send_probe,
                                             const std::function<bool(std::string* out_error)>& poll_once,
                                             const std::function<bool()>& done);

} // namespace karma::network::tests
