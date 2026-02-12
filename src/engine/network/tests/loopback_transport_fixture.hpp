#pragma once

#include "network/tests/loopback_enet_fixture.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace karma::network::tests {

using LoopbackTransportEndpoint = LoopbackEnetEndpoint;
using LoopbackTransportPumpThread = LoopbackPumpThread;
using BoundedTransportProbeLoopOptions = BoundedProbeLoopOptions;
using BoundedTransportProbeLoopDiagnostics = BoundedProbeLoopDiagnostics;
using BoundedTransportProbePhaseResult = BoundedProbePhaseResult;

inline bool InitializeLoopbackTransport(std::string* out_error = nullptr) {
    return InitializeLoopbackEnet(out_error);
}

inline std::optional<LoopbackTransportEndpoint> CreateLoopbackServerTransportEndpointAtPort(
    uint16_t port,
    size_t max_peers = 64,
    size_t channel_count = 2) {
    return CreateLoopbackServerEndpointAtPort(port, max_peers, channel_count);
}

inline std::optional<LoopbackTransportEndpoint> CreateLoopbackServerTransportEndpointAtPortWithRetry(
    uint16_t port,
    uint32_t attempts = 40,
    uint32_t retry_sleep_ms = 5,
    size_t max_peers = 64,
    size_t channel_count = 2) {
    return CreateLoopbackServerEndpointAtPortWithRetry(
        port, attempts, retry_sleep_ms, max_peers, channel_count);
}

inline std::optional<LoopbackTransportEndpoint> CreateLoopbackClientTransportEndpoint(
    uint16_t port,
    size_t channel_count = 2) {
    return CreateLoopbackClientEndpoint(port, channel_count);
}

inline std::optional<LoopbackTransportEndpoint> CreateLoopbackClientTransportEndpointWithRetry(
    uint16_t port,
    uint32_t attempts = 24,
    uint32_t retry_sleep_ms = 2,
    size_t channel_count = 2) {
    return CreateLoopbackClientEndpointWithRetry(port, attempts, retry_sleep_ms, channel_count);
}

inline void DestroyLoopbackTransportEndpoint(LoopbackTransportEndpoint* endpoint) {
    DestroyLoopbackEndpoint(endpoint);
}

inline void PumpLoopbackTransportEndpoint(LoopbackTransportEndpoint* endpoint) {
    PumpLoopbackEndpoint(endpoint);
}

inline void PumpLoopbackTransportEndpointCapturePayloads(
    LoopbackTransportEndpoint* endpoint,
    std::vector<std::vector<std::byte>>* out_payloads) {
    PumpLoopbackEndpointCapturePayloads(endpoint, out_payloads);
}

inline bool SendLoopbackTransportPayload(LoopbackTransportEndpoint* endpoint,
                                         const std::vector<std::byte>& payload) {
    return SendLoopbackPayload(endpoint, payload);
}

inline bool DisconnectLoopbackTransportEndpoint(LoopbackTransportEndpoint* endpoint, uint32_t data = 0) {
    return DisconnectLoopbackEndpoint(endpoint, data);
}

inline uint16_t GetLoopbackTransportEndpointBoundPort(const LoopbackTransportEndpoint* endpoint) {
    return GetLoopbackEndpointBoundPort(endpoint);
}

inline bool LoopbackTransportEndpointHasPeer(const LoopbackTransportEndpoint* endpoint) {
    return LoopbackEndpointHasPeer(endpoint);
}

inline bool DecodeLoopbackTransportPayloadPair(const std::vector<std::byte>& payload,
                                               uint8_t* out_first,
                                               uint8_t* out_second) {
    return DecodeLoopbackPayloadPair(payload, out_first, out_second);
}

inline bool StartLoopbackTransportEndpointPumpThread(
    LoopbackTransportPumpThread* pump,
    LoopbackTransportEndpoint* endpoint,
    std::chrono::milliseconds sleep_interval = std::chrono::milliseconds(1)) {
    return StartLoopbackEndpointPumpThread(pump, endpoint, sleep_interval);
}

inline void StopLoopbackTransportEndpointPumpThread(LoopbackTransportPumpThread* pump) {
    StopLoopbackEndpointPumpThread(pump);
}

inline bool RunBoundedTransportProbeLoop(const BoundedTransportProbeLoopOptions& options,
                                         const std::function<void()>& send_probe,
                                         const std::function<bool()>& poll_once,
                                         const std::function<bool()>& done,
                                         BoundedTransportProbeLoopDiagnostics* diagnostics) {
    return RunBoundedProbeLoop(options, send_probe, poll_once, done, diagnostics);
}

inline BoundedTransportProbePhaseResult RunBoundedTransportProbePhase(
    const BoundedTransportProbeLoopOptions& options,
    const std::function<void()>& send_probe,
    const std::function<bool(std::string* out_error)>& poll_once,
    const std::function<bool()>& done) {
    return RunBoundedProbePhase(options, send_probe, poll_once, done);
}

} // namespace karma::network::tests
