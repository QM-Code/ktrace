#include "karma/network/client_transport.hpp"
#include "network/tests/loopback_enet_fixture.hpp"
#include "network/tests/loopback_endpoint_alloc.hpp"
#include "network/tests/structured_log_event_sink.hpp"
#include "network/transport_pump_normalizer.hpp"

#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include <enet.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

namespace {

using karma::network::ClientTransportEvent;
using karma::network::ClientTransportEventType;

bool Fail(const std::string& message) {
    std::cerr << message << "\n";
    return false;
}

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        return Fail(message);
    }
    return true;
}

std::vector<ClientTransportEventType> Normalize(std::vector<ClientTransportEvent> staged_events) {
    std::vector<ClientTransportEvent> out_events{};
    karma::network::detail::NormalizePumpEvents(&out_events,
                                                &staged_events,
                                                ClientTransportEventType::Connected,
                                                ClientTransportEventType::Received,
                                                ClientTransportEventType::Disconnected);

    std::vector<ClientTransportEventType> out_types{};
    out_types.reserve(out_events.size());
    for (const auto& event : out_events) {
        out_types.push_back(event.type);
    }
    return out_types;
}

bool TestConnectedReceivedDisconnectedOrdering() {
    std::vector<ClientTransportEvent> staged_events{};

    ClientTransportEvent disconnected{};
    disconnected.type = ClientTransportEventType::Disconnected;
    staged_events.push_back(std::move(disconnected));

    ClientTransportEvent received{};
    received.type = ClientTransportEventType::Received;
    received.payload.push_back(std::byte{0x7f});
    staged_events.push_back(std::move(received));

    ClientTransportEvent connected{};
    connected.type = ClientTransportEventType::Connected;
    staged_events.push_back(std::move(connected));

    const auto normalized = Normalize(std::move(staged_events));
    return Expect(normalized.size() == 3, "expected three normalized client events") &&
           Expect(normalized[0] == ClientTransportEventType::Connected,
                  "client event[0] should be Connected") &&
           Expect(normalized[1] == ClientTransportEventType::Received,
                  "client event[1] should be Received") &&
           Expect(normalized[2] == ClientTransportEventType::Disconnected,
                  "client event[2] should be Disconnected");
}

bool TestDisconnectIsTerminalWithinPumpCycle() {
    std::vector<ClientTransportEvent> staged_events{};

    ClientTransportEvent first_received{};
    first_received.type = ClientTransportEventType::Received;
    first_received.payload.push_back(std::byte{0x01});
    staged_events.push_back(std::move(first_received));

    ClientTransportEvent disconnected{};
    disconnected.type = ClientTransportEventType::Disconnected;
    staged_events.push_back(std::move(disconnected));

    ClientTransportEvent second_received{};
    second_received.type = ClientTransportEventType::Received;
    second_received.payload.push_back(std::byte{0x02});
    staged_events.push_back(std::move(second_received));

    const auto normalized = Normalize(std::move(staged_events));
    return Expect(normalized.size() == 3, "expected three normalized client events") &&
           Expect(normalized[0] == ClientTransportEventType::Received,
                  "client event[0] should be Received") &&
           Expect(normalized[1] == ClientTransportEventType::Received,
                  "client event[1] should be Received") &&
           Expect(normalized[2] == ClientTransportEventType::Disconnected,
                  "client disconnect must be terminal in cycle ordering");
}

bool TestReconnectConnectedPrecedesPayloadInSameCycle() {
    std::vector<ClientTransportEvent> staged_events{};

    // Reconnect can append Connected after receive handling in the same poll cycle.
    ClientTransportEvent received{};
    received.type = ClientTransportEventType::Received;
    received.payload.push_back(std::byte{0x33});
    staged_events.push_back(std::move(received));

    ClientTransportEvent reconnected{};
    reconnected.type = ClientTransportEventType::Connected;
    staged_events.push_back(std::move(reconnected));

    const auto normalized = Normalize(std::move(staged_events));
    return Expect(normalized.size() == 2, "expected two normalized client events") &&
           Expect(normalized[0] == ClientTransportEventType::Connected,
                  "reconnect Connected should be emitted before payload events") &&
           Expect(normalized[1] == ClientTransportEventType::Received,
                  "payload event should follow reconnect Connected");
}

bool TestSinglePeerLifecycleEdgesRemainNormalized() {
    std::vector<ClientTransportEvent> staged_events{};

    ClientTransportEvent disconnected_first{};
    disconnected_first.type = ClientTransportEventType::Disconnected;
    staged_events.push_back(std::move(disconnected_first));

    ClientTransportEvent connected_first{};
    connected_first.type = ClientTransportEventType::Connected;
    staged_events.push_back(std::move(connected_first));

    ClientTransportEvent received{};
    received.type = ClientTransportEventType::Received;
    received.payload.push_back(std::byte{0x55});
    staged_events.push_back(std::move(received));

    ClientTransportEvent connected_second{};
    connected_second.type = ClientTransportEventType::Connected;
    staged_events.push_back(std::move(connected_second));

    ClientTransportEvent disconnected_second{};
    disconnected_second.type = ClientTransportEventType::Disconnected;
    staged_events.push_back(std::move(disconnected_second));

    const auto normalized = Normalize(std::move(staged_events));
    return Expect(normalized.size() == 5, "expected five normalized client lifecycle-edge events") &&
           Expect(normalized[0] == ClientTransportEventType::Connected,
                  "client lifecycle-edge event[0] should be Connected") &&
           Expect(normalized[1] == ClientTransportEventType::Connected,
                  "client lifecycle-edge event[1] should be Connected") &&
           Expect(normalized[2] == ClientTransportEventType::Received,
                  "client lifecycle-edge event[2] should be Received") &&
           Expect(normalized[3] == ClientTransportEventType::Disconnected,
                  "client lifecycle-edge event[3] should be Disconnected") &&
           Expect(normalized[4] == ClientTransportEventType::Disconnected,
                  "client lifecycle-edge event[4] should be Disconnected");
}

bool TestUnregisteredBackendWarnsAndFails() {
    constexpr std::string_view kLoggerName = "client_transport_contract_logger";
    auto sink = std::make_shared<karma::network::tests::StructuredLogEventSink>();
    auto logger = std::make_shared<spdlog::logger>(std::string{kLoggerName}, sink);
    logger->set_level(spdlog::level::trace);

    auto previous_logger = spdlog::default_logger();
    spdlog::set_default_logger(logger);

    karma::network::ClientTransportConfig config{};
    config.backend_name = "unregistered-client-backend-contract";
    auto transport = karma::network::CreateClientTransport(config);

    spdlog::set_default_logger(previous_logger);

    const size_t warning_count = sink->CountLevelForLogger(spdlog::level::warn, kLoggerName);
    const size_t error_count = sink->CountLevelForLogger(spdlog::level::err, kLoggerName);

    return Expect(!transport, "unregistered client backend should fail transport creation") &&
           Expect(warning_count == 1,
                  "unregistered client backend should emit exactly one warning log event") &&
           Expect(error_count == 0,
                  "unregistered client backend should not emit error-level log events");
}

using LoopbackServerEndpoint = karma::network::tests::LoopbackEnetEndpoint;

std::optional<LoopbackServerEndpoint> CreateLoopbackServerEndpoint(uint16_t* out_port) {
    if (!out_port) {
        return std::nullopt;
    }
    constexpr uint16_t kFirstPort = 32400;
    constexpr uint16_t kLastPort = 32496;
    constexpr uint32_t kPasses = 4;
    LoopbackServerEndpoint endpoint{};
    uint16_t chosen_port = 0;
    const bool bound = karma::network::tests::BindLoopbackEndpointDeterministic(
        kFirstPort,
        kLastPort,
        kPasses,
        5,
        [&](uint16_t port) {
            auto candidate = karma::network::tests::CreateLoopbackServerEndpointAtPort(port);
            if (!candidate.has_value()) {
                return false;
            }
            endpoint = std::move(*candidate);
            chosen_port = port;
            return true;
        });
    if (!bound) {
        return std::nullopt;
    }
    *out_port = chosen_port;
    return endpoint;
}

std::optional<LoopbackServerEndpoint> CreateLoopbackServerEndpointAtPort(uint16_t port) {
    return karma::network::tests::CreateLoopbackServerEndpointAtPortWithRetry(port);
}

void DestroyLoopbackServerEndpoint(LoopbackServerEndpoint* endpoint) {
    karma::network::tests::DestroyLoopbackEndpoint(endpoint);
}

void PumpLoopbackServerEndpoint(LoopbackServerEndpoint* endpoint) {
    karma::network::tests::PumpLoopbackEndpoint(endpoint);
}

bool SendLoopbackServerPayloadBurst(LoopbackServerEndpoint* endpoint,
                                    uint8_t cycle_index,
                                    uint32_t payload_count) {
    if (!endpoint || !endpoint->host || !endpoint->peer) {
        return false;
    }
    for (uint32_t sequence = 0; sequence < payload_count; ++sequence) {
        std::vector<std::byte> payload{};
        payload.push_back(static_cast<std::byte>(cycle_index));
        payload.push_back(static_cast<std::byte>(sequence));
        payload.push_back(std::byte{0x5a});
        if (!karma::network::tests::SendLoopbackPayload(endpoint, payload)) {
            return false;
        }
    }
    return true;
}

bool PollClientTransport(std::unique_ptr<karma::network::ClientTransport>* transport,
                         std::vector<ClientTransportEvent>* out_events) {
    if (!transport || !(*transport) || !out_events) {
        return false;
    }
    out_events->clear();
    (*transport)->poll(karma::network::ClientTransportPollOptions{}, out_events);
    return true;
}

bool PollClientTransportWithServerPump(std::unique_ptr<karma::network::ClientTransport>* transport,
                                       LoopbackServerEndpoint* server,
                                       std::vector<ClientTransportEvent>* out_events) {
    if (!transport || !(*transport) || !server || !out_events) {
        return false;
    }
    std::atomic<bool> keep_pumping{true};
    std::thread pump_thread([&]() {
        while (keep_pumping.load()) {
            PumpLoopbackServerEndpoint(server);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    const bool ok = PollClientTransport(transport, out_events);
    keep_pumping.store(false);
    pump_thread.join();
    return ok;
}

bool TestLiveReconnectPayloadInterleaveStress() {
    karma::network::ClientTransportConfig config{};
    auto transport = karma::network::CreateClientTransport(config);
    if (!Expect(transport && transport->isReady(),
                "live reconnect stress failed to create ready client transport")) {
        return false;
    }

    uint16_t listen_port = 0;
    auto server_opt = CreateLoopbackServerEndpoint(&listen_port);
    if (!Expect(server_opt.has_value(), "live reconnect stress failed to create loopback server")) {
        return false;
    }
    LoopbackServerEndpoint server = std::move(*server_opt);
    auto cleanup_server = [&]() { DestroyLoopbackServerEndpoint(&server); };

    karma::network::ClientTransportConnectOptions options{};
    options.host = "127.0.0.1";
    options.port = listen_port;
    options.timeout_ms = 300;
    options.reconnect_max_attempts = 24;
    options.reconnect_backoff_initial_ms = 0;
    options.reconnect_backoff_max_ms = 0;
    options.reconnect_timeout_ms = 120;
    std::atomic<bool> pump_connect{true};
    std::thread connect_pump_thread([&]() {
        while (pump_connect.load()) {
            PumpLoopbackServerEndpoint(&server);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    const bool initial_connected = transport->connect(options);
    pump_connect.store(false);
    connect_pump_thread.join();
    if (!Expect(initial_connected,
                "live reconnect stress initial connect failed")) {
        cleanup_server();
        return false;
    }

    std::vector<ClientTransportEvent> client_events{};
    const auto initial_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (std::chrono::steady_clock::now() < initial_deadline) {
        PollClientTransportWithServerPump(&transport, &server, &client_events);
        if (server.connect_events > 0 && server.peer != nullptr) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!Expect(server.connect_events > 0 && server.peer != nullptr,
                "live reconnect stress timed out waiting for initial server-side connect")) {
        cleanup_server();
        return false;
    }

    constexpr uint32_t kReconnectCycles = 8;
    constexpr uint32_t kPayloadsPerCycle = 24;

    for (uint32_t cycle = 0; cycle < kReconnectCycles; ++cycle) {
        if (!Expect(server.peer != nullptr, "live reconnect stress expected connected server peer")) {
            cleanup_server();
            return false;
        }
        ENetPeer* prior_peer = server.peer;
        enet_peer_disconnect_now(prior_peer, 0);
        if (server.peer == prior_peer) {
            server.peer = nullptr;
        }

        bool saw_reconnect_connected = false;
        bool sent_post_reconnect_burst = false;
        uint32_t received_after_connected = 0;

        const auto cycle_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
        while (std::chrono::steady_clock::now() < cycle_deadline) {
            if (!sent_post_reconnect_burst && server.peer != nullptr) {
                if (!Expect(SendLoopbackServerPayloadBurst(&server,
                                                           static_cast<uint8_t>(cycle),
                                                           kPayloadsPerCycle),
                            "live reconnect stress failed to send post-reconnect payload burst")) {
                    cleanup_server();
                    return false;
                }
                sent_post_reconnect_burst = true;
            }

            if (!PollClientTransportWithServerPump(&transport, &server, &client_events)) {
                cleanup_server();
                return Fail("live reconnect stress failed polling client transport");
            }
            for (const auto& event : client_events) {
                if (event.type == ClientTransportEventType::Connected) {
                    saw_reconnect_connected = true;
                } else if (event.type == ClientTransportEventType::Received) {
                    if (!saw_reconnect_connected) {
                        cleanup_server();
                        return Fail("live reconnect stress observed payload before reconnect Connected");
                    }
                    ++received_after_connected;
                } else if (event.type == ClientTransportEventType::Disconnected) {
                    cleanup_server();
                    return Fail("live reconnect stress observed terminal Disconnected during reconnect cycle");
                }
            }

            if (saw_reconnect_connected && sent_post_reconnect_burst &&
                received_after_connected >= kPayloadsPerCycle) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        if (!Expect(saw_reconnect_connected,
                    "live reconnect stress timed out waiting for reconnect Connected event")) {
            cleanup_server();
            return false;
        }
        if (!Expect(sent_post_reconnect_burst,
                    "live reconnect stress did not observe post-reconnect server peer")) {
            cleanup_server();
            return false;
        }
        if (!Expect(received_after_connected == kPayloadsPerCycle,
                    "live reconnect stress did not receive full post-reconnect payload burst")) {
            cleanup_server();
            return false;
        }
    }

    transport->disconnect(0);
    cleanup_server();
    return true;
}

bool TestLiveTimeoutDisconnectRaceTerminalOrdering() {
    karma::network::ClientTransportConfig config{};
    auto transport = karma::network::CreateClientTransport(config);
    if (!Expect(transport && transport->isReady(),
                "live timeout-race stress failed to create ready client transport")) {
        return false;
    }

    uint16_t listen_port = 0;
    auto server_opt = CreateLoopbackServerEndpoint(&listen_port);
    if (!Expect(server_opt.has_value(), "live timeout-race stress failed to create loopback server")) {
        return false;
    }
    constexpr auto kPollSleep = std::chrono::milliseconds(1);
    constexpr auto kInitialPeerDeadline = std::chrono::seconds(3);
    constexpr auto kReconnectReadinessDeadline = std::chrono::seconds(11);
    constexpr auto kReconnectDeliveryDeadline = std::chrono::seconds(5);
    constexpr auto kTerminalDisconnectDeadline = std::chrono::seconds(15);
    constexpr int kPostTerminalSettlePolls = 140;
    constexpr uint32_t kReconnectProbeSendIntervalPolls = 4;
    constexpr uint32_t kReconnectProbeMaxSends = 3000;
    constexpr uint32_t kReconnectReadyStablePolls = 2;
    constexpr uint32_t kTerminalProbeSendIntervalPolls = 5;
    constexpr uint32_t kTerminalProbeMaxSends = 3000;

    LoopbackServerEndpoint server = std::move(*server_opt);
    karma::network::tests::LoopbackPumpThread server_pump{};
    auto stop_server_pump = [&]() { karma::network::tests::StopLoopbackEndpointPumpThread(&server_pump); };
    auto start_server_pump = [&]() {
        return karma::network::tests::StartLoopbackEndpointPumpThread(&server_pump, &server, kPollSleep);
    };
    auto cleanup_server = [&]() {
        stop_server_pump();
        DestroyLoopbackServerEndpoint(&server);
    };

    karma::network::ClientTransportConnectOptions options{};
    options.host = "127.0.0.1";
    options.port = listen_port;
    options.timeout_ms = 400;
    options.reconnect_max_attempts = 32;
    options.reconnect_backoff_initial_ms = 0;
    options.reconnect_backoff_max_ms = 0;
    options.reconnect_timeout_ms = 180;

    std::atomic<bool> pump_connect{true};
    std::thread connect_pump_thread([&]() {
        while (pump_connect.load()) {
            PumpLoopbackServerEndpoint(&server);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    const bool initial_connected = transport->connect(options);
    pump_connect.store(false);
    connect_pump_thread.join();
    if (!Expect(initial_connected,
                "live timeout-race stress initial connect failed")) {
        cleanup_server();
        return false;
    }

    std::vector<ClientTransportEvent> client_events{};
    if (!Expect(start_server_pump(),
                "live timeout-race stress failed to start loopback server pump")) {
        cleanup_server();
        return false;
    }
    const auto initial_deadline = std::chrono::steady_clock::now() + kInitialPeerDeadline;
    while (std::chrono::steady_clock::now() < initial_deadline) {
        PollClientTransport(&transport, &client_events);
        if (server.peer != nullptr) {
            break;
        }
        std::this_thread::sleep_for(kPollSleep);
    }
    if (!Expect(server.peer != nullptr,
                "live timeout-race stress timed out waiting for initial server peer")) {
        cleanup_server();
        return false;
    }

    // Phase 1: timeout/disconnect race with successful reconnect.
    // Force aggressive timeout parameters before hard-drop.
    stop_server_pump();
    enet_peer_timeout(server.peer, 1, 1, 1);
    DestroyLoopbackServerEndpoint(&server);
    const std::vector<std::byte> timeout_probe{std::byte{0x77}, std::byte{0x42}};
    (void)transport->sendReliable(timeout_probe);

    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    auto restarted_server = karma::network::tests::CreateLoopbackServerEndpointAtPortWithRetry(
        listen_port,
        120,
        5);
    if (!Expect(restarted_server.has_value(),
                "live timeout-race stress failed to restart loopback server")) {
        return false;
    }
    server = std::move(*restarted_server);
    if (!Expect(start_server_pump(),
                "live timeout-race stress failed to restart loopback server pump")) {
        cleanup_server();
        return false;
    }

    bool saw_reconnect_connected = false;
    bool sent_reconnect_burst = false;
    uint32_t received_after_reconnect = 0;
    uint32_t reconnect_poll_loops = 0;
    uint32_t reconnect_probe_sends = 0;
    uint32_t reconnect_stable_ready_polls = 0;
    constexpr uint32_t kReconnectBurstCount = 10;
    const auto make_reconnect_probe_options = [&](std::chrono::steady_clock::duration timeout) {
        karma::network::tests::BoundedProbeLoopOptions options{};
        options.deadline = std::chrono::steady_clock::now() + timeout;
        options.poll_sleep = kPollSleep;
        options.probe_interval_polls = kReconnectProbeSendIntervalPolls;
        options.probe_max_sends = kReconnectProbeMaxSends;
        return options;
    };
    const auto poll_reconnect_phase = [&](const char* poll_failure,
                                          const char* disconnect_failure,
                                          bool track_stable_readiness,
                                          std::string* out_error) {
        if (!PollClientTransport(&transport, &client_events)) {
            if (out_error) {
                *out_error = poll_failure;
            }
            return false;
        }
        for (const auto& event : client_events) {
            if (event.type == ClientTransportEventType::Connected) {
                saw_reconnect_connected = true;
            } else if (event.type == ClientTransportEventType::Received) {
                if (!saw_reconnect_connected) {
                    if (out_error) {
                        *out_error = "live timeout-race stress saw payload before reconnect Connected";
                    }
                    return false;
                }
                ++received_after_reconnect;
            } else if (event.type == ClientTransportEventType::Disconnected) {
                if (out_error) {
                    *out_error = disconnect_failure;
                }
                return false;
            }
        }
        if (track_stable_readiness) {
            if (saw_reconnect_connected && server.peer != nullptr) {
                ++reconnect_stable_ready_polls;
            } else {
                reconnect_stable_ready_polls = 0;
            }
        }
        return true;
    };
    const auto reconnect_readiness_options =
        make_reconnect_probe_options(kReconnectReadinessDeadline);
    const auto reconnect_readiness_phase = karma::network::tests::RunBoundedProbePhase(
        reconnect_readiness_options,
        [&]() { (void)transport->sendReliable(timeout_probe); },
        [&](std::string* out_error) {
            return poll_reconnect_phase("live timeout-race stress failed polling reconnect phase",
                                        "live timeout-race stress emitted terminal Disconnected before "
                                        "reconnect recovered",
                                        true,
                                        out_error);
        },
        [&]() { return reconnect_stable_ready_polls >= kReconnectReadyStablePolls; });
    reconnect_poll_loops = reconnect_readiness_phase.diagnostics.poll_loops;
    reconnect_probe_sends = reconnect_readiness_phase.diagnostics.probe_sends;
    if (!reconnect_readiness_phase.completed && !reconnect_readiness_phase.failure_message.empty()) {
        cleanup_server();
        return Fail(reconnect_readiness_phase.failure_message);
    }
    if (!Expect(reconnect_stable_ready_polls >= kReconnectReadyStablePolls,
                "live timeout-race stress timed out waiting for deterministic reconnect readiness "
                "(stable-ready polls=" +
                    std::to_string(reconnect_stable_ready_polls) + ", polls=" +
                    std::to_string(reconnect_poll_loops) + ", probe sends=" +
                    std::to_string(reconnect_probe_sends) + ")")) {
        cleanup_server();
        return false;
    }
    if (!Expect(SendLoopbackServerPayloadBurst(&server, 0x41, kReconnectBurstCount),
                "live timeout-race stress failed sending reconnect burst after readiness gating")) {
        cleanup_server();
        return false;
    }
    sent_reconnect_burst = true;

    const auto reconnect_delivery_options =
        make_reconnect_probe_options(kReconnectDeliveryDeadline);
    const auto reconnect_delivery_phase = karma::network::tests::RunBoundedProbePhase(
        reconnect_delivery_options,
        [&]() { (void)transport->sendReliable(timeout_probe); },
        [&](std::string* out_error) {
            return poll_reconnect_phase(
                "live timeout-race stress failed polling reconnect delivery phase",
                "live timeout-race stress emitted terminal Disconnected before reconnect delivery completed",
                false,
                out_error);
        },
        [&]() { return received_after_reconnect >= kReconnectBurstCount; });
    reconnect_poll_loops += reconnect_delivery_phase.diagnostics.poll_loops;
    reconnect_probe_sends += reconnect_delivery_phase.diagnostics.probe_sends;
    if (!reconnect_delivery_phase.completed && !reconnect_delivery_phase.failure_message.empty()) {
        cleanup_server();
        return Fail(reconnect_delivery_phase.failure_message);
    }
    if (!Expect(sent_reconnect_burst && received_after_reconnect == kReconnectBurstCount,
                "live timeout-race stress did not fully receive reconnect burst (received=" +
                    std::to_string(received_after_reconnect) + "/" +
                    std::to_string(kReconnectBurstCount) + ", polls=" +
                    std::to_string(reconnect_poll_loops) + ", probe sends=" +
                    std::to_string(reconnect_probe_sends) + ")")) {
        cleanup_server();
        return false;
    }

    // Phase 2: hard-drop again and keep server offline to force reconnect exhaustion.
    stop_server_pump();
    if (server.peer != nullptr) {
        enet_peer_timeout(server.peer, 1, 1, 1);
    }
    DestroyLoopbackServerEndpoint(&server);
    (void)transport->sendReliable(timeout_probe);

    bool saw_terminal_disconnected = false;
    uint32_t terminal_disconnect_count = 0;
    bool saw_connected_after_terminal = false;
    bool saw_received_after_terminal = false;
    uint32_t terminal_poll_loops = 0;
    uint32_t terminal_probe_sends = 0;
    karma::network::tests::BoundedProbeLoopOptions terminal_options{};
    terminal_options.deadline = std::chrono::steady_clock::now() + kTerminalDisconnectDeadline;
    terminal_options.poll_sleep = kPollSleep;
    terminal_options.probe_interval_polls = kTerminalProbeSendIntervalPolls;
    terminal_options.probe_max_sends = kTerminalProbeMaxSends;
    const auto terminal_phase = karma::network::tests::RunBoundedProbePhase(
        terminal_options,
        [&]() { (void)transport->sendReliable(timeout_probe); },
        [&](std::string* out_error) {
            if (!PollClientTransport(&transport, &client_events)) {
                if (out_error) {
                    *out_error = "live timeout-race stress failed polling terminal phase";
                }
                return false;
            }
            for (const auto& event : client_events) {
                if (event.type == ClientTransportEventType::Disconnected) {
                    saw_terminal_disconnected = true;
                    ++terminal_disconnect_count;
                } else if (event.type == ClientTransportEventType::Connected) {
                    if (saw_terminal_disconnected) {
                        saw_connected_after_terminal = true;
                    }
                } else if (event.type == ClientTransportEventType::Received) {
                    if (saw_terminal_disconnected) {
                        saw_received_after_terminal = true;
                    }
                }
            }
            return true;
        },
        [&]() { return saw_terminal_disconnected; });
    terminal_poll_loops = terminal_phase.diagnostics.poll_loops;
    terminal_probe_sends = terminal_phase.diagnostics.probe_sends;
    if (!terminal_phase.completed && !terminal_phase.failure_message.empty()) {
        return Fail(terminal_phase.failure_message);
    }
    if (!Expect(saw_terminal_disconnected,
                "live timeout-race stress timed out waiting for terminal Disconnected (polls=" +
                    std::to_string(terminal_poll_loops) + ", probe sends=" +
                    std::to_string(terminal_probe_sends) + ")")) {
        return false;
    }

    for (int i = 0; i < kPostTerminalSettlePolls; ++i) {
        if (!PollClientTransport(&transport, &client_events)) {
            return Fail("live timeout-race stress failed polling post-terminal settle");
        }
        for (const auto& event : client_events) {
            if (event.type == ClientTransportEventType::Disconnected) {
                ++terminal_disconnect_count;
            } else if (event.type == ClientTransportEventType::Connected) {
                saw_connected_after_terminal = true;
            } else if (event.type == ClientTransportEventType::Received) {
                saw_received_after_terminal = true;
            }
        }
        std::this_thread::sleep_for(kPollSleep);
    }

    if (!Expect(terminal_disconnect_count == 1,
                "live timeout-race stress expected exactly one terminal Disconnected")) {
        return false;
    }
    if (!Expect(!saw_connected_after_terminal,
                "live timeout-race stress observed Connected after terminal Disconnected")) {
        return false;
    }
    if (!Expect(!saw_received_after_terminal,
                "live timeout-race stress observed Received after terminal Disconnected")) {
        return false;
    }

    transport->close();
    return true;
}

} // namespace

int main() {
    if (!TestConnectedReceivedDisconnectedOrdering()) {
        return 1;
    }
    if (!TestDisconnectIsTerminalWithinPumpCycle()) {
        return 1;
    }
    if (!TestReconnectConnectedPrecedesPayloadInSameCycle()) {
        return 1;
    }
    if (!TestSinglePeerLifecycleEdgesRemainNormalized()) {
        return 1;
    }
    if (!TestLiveReconnectPayloadInterleaveStress()) {
        return 1;
    }
    if (!TestLiveTimeoutDisconnectRaceTerminalOrdering()) {
        return 1;
    }
    if (!TestUnregisteredBackendWarnsAndFails()) {
        return 1;
    }
    return 0;
}
