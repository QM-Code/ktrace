#include "karma/network/server_transport.hpp"
#include "network/tests/loopback_enet_fixture.hpp"
#include "network/tests/loopback_endpoint_alloc.hpp"
#include "network/tests/structured_log_event_sink.hpp"
#include "network/transport_pump_normalizer.hpp"

#include <spdlog/logger.h>
#include <spdlog/spdlog.h>

#include <enet.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using karma::network::ServerTransportEvent;
using karma::network::ServerTransportEventType;

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

std::vector<ServerTransportEvent> NormalizeEvents(std::vector<ServerTransportEvent> staged_events) {
    std::vector<ServerTransportEvent> out_events{};
    karma::network::detail::NormalizePumpEventsPerKey(&out_events,
                                                      &staged_events,
                                                      ServerTransportEventType::Connected,
                                                      ServerTransportEventType::Received,
                                                      ServerTransportEventType::Disconnected,
                                                      [](const ServerTransportEvent& event) {
                                                          return event.peer;
                                                      });
    return out_events;
}

std::vector<ServerTransportEventType> NormalizeTypes(std::vector<ServerTransportEvent> staged_events) {
    const auto out_events = NormalizeEvents(std::move(staged_events));
    std::vector<ServerTransportEventType> out_types{};
    out_types.reserve(out_events.size());
    for (const auto& event : out_events) {
        out_types.push_back(event.type);
    }
    return out_types;
}

ServerTransportEvent MakeEvent(ServerTransportEventType type, uint8_t payload_byte, uint32_t peer = 17) {
    ServerTransportEvent event{};
    event.type = type;
    event.peer = peer;
    event.peer_ip = "127.0.0.1";
    event.peer_port = static_cast<uint16_t>(30000 + peer);
    if (type == ServerTransportEventType::Received) {
        event.payload.push_back(static_cast<std::byte>(payload_byte));
    }
    return event;
}

using LoopbackClientEndpoint = karma::network::tests::LoopbackEnetEndpoint;

int EventPhase(ServerTransportEventType type) {
    switch (type) {
        case ServerTransportEventType::Connected:
            return 0;
        case ServerTransportEventType::Received:
            return 1;
        case ServerTransportEventType::Disconnected:
            return 2;
    }
    return -1;
}

std::unique_ptr<karma::network::ServerTransport> CreateLoopbackServerTransport(uint16_t* out_port) {
    if (!out_port) {
        return nullptr;
    }
    constexpr uint16_t kFirstPort = 32300;
    constexpr uint16_t kLastPort = 32428;
    constexpr uint32_t kPasses = 4;
    std::unique_ptr<karma::network::ServerTransport> transport{};
    uint16_t chosen_port = 0;
    const bool bound = karma::network::tests::BindLoopbackEndpointDeterministic(
        kFirstPort,
        kLastPort,
        kPasses,
        5,
        [&](uint16_t port) {
            karma::network::ServerTransportConfig config{};
            config.listen_port = port;
            config.max_clients = 64;
            config.channel_count = 2;
            auto candidate = karma::network::CreateServerTransport(config);
            if (!(candidate && candidate->isReady())) {
                return false;
            }
            transport = std::move(candidate);
            chosen_port = port;
            return true;
        });
    if (!bound) {
        return nullptr;
    }
    *out_port = chosen_port;
    return transport;
}

std::optional<LoopbackClientEndpoint> CreateLoopbackClient(uint16_t port) {
    return karma::network::tests::CreateLoopbackClientEndpoint(port);
}

std::optional<LoopbackClientEndpoint> CreateLoopbackClientWithRetry(uint16_t port,
                                                                     uint32_t attempts = 24) {
    return karma::network::tests::CreateLoopbackClientEndpointWithRetry(port, attempts);
}

void DestroyLoopbackClient(LoopbackClientEndpoint* endpoint) {
    karma::network::tests::DestroyLoopbackEndpoint(endpoint);
}

void PumpLoopbackClient(LoopbackClientEndpoint* endpoint) {
    karma::network::tests::PumpLoopbackEndpoint(endpoint);
}

uint32_t CountActiveClients(const std::vector<LoopbackClientEndpoint>& clients) {
    uint32_t active = 0;
    for (const auto& client : clients) {
        if (client.connected && client.peer) {
            ++active;
        }
    }
    return active;
}

bool SendLoopbackPayload(LoopbackClientEndpoint* endpoint, uint8_t client_index, uint8_t sequence) {
    std::vector<std::byte> payload{};
    payload.push_back(static_cast<std::byte>(client_index));
    payload.push_back(static_cast<std::byte>(sequence));
    payload.push_back(std::byte{0x7f});
    return karma::network::tests::SendLoopbackPayload(endpoint, payload);
}

bool DecodeLoopbackPayload(const ServerTransportEvent& event,
                           uint8_t* out_client_index,
                           uint8_t* out_sequence) {
    if (event.type != ServerTransportEventType::Received || event.payload.size() < 2
        || !out_client_index || !out_sequence) {
        return false;
    }
    return karma::network::tests::DecodeLoopbackPayloadPair(event.payload,
                                                            out_client_index,
                                                            out_sequence);
}

bool ValidateAndAccumulatePerPeerOrdering(
    const std::vector<ServerTransportEvent>& events,
    std::unordered_map<karma::network::PeerToken, int>* global_phase_by_peer,
    std::unordered_map<karma::network::PeerToken, int>* first_phase_by_peer,
    std::unordered_map<karma::network::PeerToken, uint32_t>* connected_count_by_peer,
    std::unordered_map<karma::network::PeerToken, uint32_t>* received_count_by_peer,
    std::unordered_map<karma::network::PeerToken, uint32_t>* disconnected_count_by_peer,
    bool allow_connected_after_terminal_disconnect = false) {
    if (!global_phase_by_peer || !first_phase_by_peer || !connected_count_by_peer
        || !received_count_by_peer || !disconnected_count_by_peer) {
        return false;
    }

    karma::network::PeerToken current_peer = 0;
    int current_phase = -1;
    std::unordered_set<karma::network::PeerToken> completed_peers{};
    for (const auto& event : events) {
        if (event.peer == 0) {
            return Fail("live stress observed event with null peer token");
        }

        const int phase = EventPhase(event.type);
        if (event.peer != current_peer) {
            if (current_peer != 0) {
                completed_peers.insert(current_peer);
            }
            if (completed_peers.find(event.peer) != completed_peers.end()) {
                return Fail("live stress observed non-contiguous per-peer event block within poll");
            }
            current_peer = event.peer;
            current_phase = -1;
        }
        if (phase < current_phase) {
            return Fail("live stress observed per-peer phase regression within poll");
        }
        current_phase = phase;

        auto it_global = global_phase_by_peer->find(event.peer);
        const int previous_phase = (it_global == global_phase_by_peer->end()) ? -1 : it_global->second;
        if (previous_phase == 2) {
            if (!(allow_connected_after_terminal_disconnect
                  && event.type == ServerTransportEventType::Connected)) {
                return Fail("live stress observed event after terminal disconnect for peer");
            }
        }
        if (phase < previous_phase && !(previous_phase == 2 && phase == 0
                                        && allow_connected_after_terminal_disconnect)) {
            return Fail("live stress observed cross-poll per-peer phase regression");
        }
        (*global_phase_by_peer)[event.peer] = phase;
        if (first_phase_by_peer->find(event.peer) == first_phase_by_peer->end()) {
            (*first_phase_by_peer)[event.peer] = phase;
        }

        switch (event.type) {
            case ServerTransportEventType::Connected:
                ++(*connected_count_by_peer)[event.peer];
                break;
            case ServerTransportEventType::Received:
                ++(*received_count_by_peer)[event.peer];
                break;
            case ServerTransportEventType::Disconnected:
                ++(*disconnected_count_by_peer)[event.peer];
                break;
        }
    }
    return true;
}

bool TestConnectedReceivedDisconnectedOrdering() {
    std::vector<ServerTransportEvent> staged_events{};
    staged_events.push_back(MakeEvent(ServerTransportEventType::Disconnected, 0));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Received, 0x10));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Connected, 0));

    const auto normalized = NormalizeTypes(std::move(staged_events));
    return Expect(normalized.size() == 3, "expected three normalized server events") &&
           Expect(normalized[0] == ServerTransportEventType::Connected,
                  "server event[0] should be Connected") &&
           Expect(normalized[1] == ServerTransportEventType::Received,
                  "server event[1] should be Received") &&
           Expect(normalized[2] == ServerTransportEventType::Disconnected,
                  "server event[2] should be Disconnected");
}

bool TestDisconnectIsTerminalWithinPumpCycle() {
    std::vector<ServerTransportEvent> staged_events{};
    staged_events.push_back(MakeEvent(ServerTransportEventType::Received, 0x01));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Disconnected, 0));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Received, 0x02));

    const auto normalized = NormalizeTypes(std::move(staged_events));
    return Expect(normalized.size() == 3, "expected three normalized server events") &&
           Expect(normalized[0] == ServerTransportEventType::Received,
                  "server event[0] should be Received") &&
           Expect(normalized[1] == ServerTransportEventType::Received,
                  "server event[1] should be Received") &&
           Expect(normalized[2] == ServerTransportEventType::Disconnected,
                  "server disconnect must be terminal in cycle ordering");
}

bool TestPerPeerOrderingAndDisconnectEdges() {
    std::vector<ServerTransportEvent> staged_events{};

    // Peer 2 appears first; peer ordering should follow first appearance.
    staged_events.push_back(MakeEvent(ServerTransportEventType::Received, 0x21, 2));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Disconnected, 0x00, 1));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Connected, 0x00, 2));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Received, 0x11, 1));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Disconnected, 0x00, 2));
    staged_events.push_back(MakeEvent(ServerTransportEventType::Connected, 0x00, 1));

    const auto normalized = NormalizeEvents(std::move(staged_events));
    return Expect(normalized.size() == 6, "expected six normalized per-peer server events") &&
           Expect(normalized[0].peer == 2 &&
                      normalized[0].type == ServerTransportEventType::Connected,
                  "peer-2 event[0] should be Connected") &&
           Expect(normalized[1].peer == 2 &&
                      normalized[1].type == ServerTransportEventType::Received,
                  "peer-2 event[1] should be Received") &&
           Expect(normalized[2].peer == 2 &&
                      normalized[2].type == ServerTransportEventType::Disconnected,
                  "peer-2 event[2] should be Disconnected terminally") &&
           Expect(normalized[3].peer == 1 &&
                      normalized[3].type == ServerTransportEventType::Connected,
                  "peer-1 event[3] should be Connected") &&
           Expect(normalized[4].peer == 1 &&
                      normalized[4].type == ServerTransportEventType::Received,
                  "peer-1 event[4] should be Received") &&
           Expect(normalized[5].peer == 1 &&
                      normalized[5].type == ServerTransportEventType::Disconnected,
                  "peer-1 event[5] should be Disconnected terminally");
}

bool TestHighVolumeMultiPeerOrderingStress() {
    constexpr uint32_t kPeerCount = 128;
    constexpr uint32_t kReceivedPerPeer = 32;
    constexpr uint32_t kPeerStride = 37; // Co-prime to 128 for deterministic full permutation.

    std::vector<uint32_t> peer_order{};
    peer_order.reserve(kPeerCount);
    for (uint32_t i = 0; i < kPeerCount; ++i) {
        peer_order.push_back(((i * kPeerStride) % kPeerCount) + 1);
    }

    std::vector<ServerTransportEvent> staged_events{};
    staged_events.reserve(static_cast<size_t>(kPeerCount) * static_cast<size_t>(kReceivedPerPeer + 2));

    // Stage intentionally scrambled type order to stress normalization:
    // all first received events, then all disconnects, then all connects, then remaining receives.
    for (const auto peer : peer_order) {
        staged_events.push_back(MakeEvent(ServerTransportEventType::Received,
                                          static_cast<uint8_t>(peer % 251),
                                          peer));
    }
    for (const auto peer : peer_order) {
        staged_events.push_back(MakeEvent(ServerTransportEventType::Disconnected, 0x00, peer));
    }
    for (const auto peer : peer_order) {
        staged_events.push_back(MakeEvent(ServerTransportEventType::Connected, 0x00, peer));
    }
    for (uint32_t receive_idx = 1; receive_idx < kReceivedPerPeer; ++receive_idx) {
        for (const auto peer : peer_order) {
            const auto payload_byte = static_cast<uint8_t>((peer + receive_idx) % 251);
            staged_events.push_back(
                MakeEvent(ServerTransportEventType::Received, payload_byte, peer));
        }
    }

    const auto normalized = NormalizeEvents(std::move(staged_events));
    const size_t expected_count =
        static_cast<size_t>(kPeerCount) * static_cast<size_t>(kReceivedPerPeer + 2);
    if (!Expect(normalized.size() == expected_count,
                "high-volume stress expected normalized event count mismatch")) {
        return false;
    }

    const size_t events_per_peer = static_cast<size_t>(kReceivedPerPeer + 2);
    for (size_t peer_idx = 0; peer_idx < peer_order.size(); ++peer_idx) {
        const auto expected_peer = peer_order[peer_idx];
        const size_t base = peer_idx * events_per_peer;
        if (!Expect(normalized[base].peer == expected_peer &&
                        normalized[base].type == ServerTransportEventType::Connected,
                    "high-volume stress connected ordering mismatch")) {
            return false;
        }
        for (size_t offset = 1; offset <= static_cast<size_t>(kReceivedPerPeer); ++offset) {
            if (!Expect(normalized[base + offset].peer == expected_peer &&
                            normalized[base + offset].type == ServerTransportEventType::Received,
                        "high-volume stress received ordering mismatch")) {
                return false;
            }
        }
        if (!Expect(normalized[base + events_per_peer - 1].peer == expected_peer &&
                        normalized[base + events_per_peer - 1].type ==
                            ServerTransportEventType::Disconnected,
                    "high-volume stress disconnect terminal ordering mismatch")) {
            return false;
        }
    }

    return true;
}

bool TestLiveLoopbackMultiPeerOrderingStress() {
    constexpr uint32_t kPeerCount = 16;
    constexpr uint32_t kBurstMessagesPerPeer = 48;

    uint16_t port = 0;
    auto transport = CreateLoopbackServerTransport(&port);
    if (!Expect(transport && transport->isReady(), "live stress failed to create loopback server transport")) {
        return false;
    }

    std::vector<LoopbackClientEndpoint> clients{};
    clients.reserve(kPeerCount);
    for (uint32_t i = 0; i < kPeerCount; ++i) {
        auto endpoint = CreateLoopbackClientWithRetry(port);
        if (!Expect(endpoint.has_value(), "live stress failed to create loopback client endpoint")) {
            for (auto& client : clients) {
                DestroyLoopbackClient(&client);
            }
            return false;
        }
        clients.push_back(std::move(*endpoint));
    }

    auto cleanup_clients = [&]() {
        for (auto& client : clients) {
            DestroyLoopbackClient(&client);
        }
    };

    std::unordered_map<karma::network::PeerToken, int> global_phase_by_peer{};
    std::unordered_map<karma::network::PeerToken, int> first_phase_by_peer{};
    std::unordered_map<karma::network::PeerToken, uint32_t> connected_count_by_peer{};
    std::unordered_map<karma::network::PeerToken, uint32_t> received_count_by_peer{};
    std::unordered_map<karma::network::PeerToken, uint32_t> disconnected_count_by_peer{};

    const auto pump_step = [&]() -> bool {
        for (auto& client : clients) {
            PumpLoopbackClient(&client);
        }
        std::vector<ServerTransportEvent> events{};
        transport->poll(karma::network::ServerTransportPollOptions{}, &events);
        return ValidateAndAccumulatePerPeerOrdering(events,
                                                    &global_phase_by_peer,
                                                    &first_phase_by_peer,
                                                    &connected_count_by_peer,
                                                    &received_count_by_peer,
                                                    &disconnected_count_by_peer);
    };

    const auto connect_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < connect_deadline) {
        if (!pump_step()) {
            cleanup_clients();
            return false;
        }
        const bool all_clients_connected = std::all_of(clients.begin(),
                                                       clients.end(),
                                                       [](const LoopbackClientEndpoint& client) {
                                                           return client.connected;
                                                       });
        if (all_clients_connected && connected_count_by_peer.size() == kPeerCount) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!Expect(connected_count_by_peer.size() == kPeerCount,
                "live stress timed out waiting for all peers to connect")) {
        cleanup_clients();
        return false;
    }

    for (uint32_t sequence = 0; sequence < kBurstMessagesPerPeer; ++sequence) {
        for (uint32_t client_index = 0; client_index < kPeerCount; ++client_index) {
            if (!Expect(SendLoopbackPayload(&clients[client_index],
                                            static_cast<uint8_t>(client_index),
                                            static_cast<uint8_t>(sequence)),
                        "live stress failed to send burst payload")) {
                cleanup_clients();
                return false;
            }
        }
    }

    const auto has_full_received_burst = [&]() {
        if (received_count_by_peer.size() != kPeerCount) {
            return false;
        }
        for (const auto& [peer, count] : received_count_by_peer) {
            (void)peer;
            if (count != kBurstMessagesPerPeer) {
                return false;
            }
        }
        return true;
    };

    const auto receive_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(7);
    while (std::chrono::steady_clock::now() < receive_deadline) {
        if (!pump_step()) {
            cleanup_clients();
            return false;
        }
        if (has_full_received_burst()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!Expect(has_full_received_burst(),
                "live stress timed out waiting for full per-peer burst delivery")) {
        cleanup_clients();
        return false;
    }

    for (auto& client : clients) {
        if (client.peer) {
            enet_peer_disconnect(client.peer, 0);
        }
    }

    const auto disconnect_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(7);
    while (std::chrono::steady_clock::now() < disconnect_deadline) {
        if (!pump_step()) {
            cleanup_clients();
            return false;
        }
        if (disconnected_count_by_peer.size() == kPeerCount) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    for (int i = 0; i < 40; ++i) {
        if (!pump_step()) {
            cleanup_clients();
            return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (!Expect(first_phase_by_peer.size() == kPeerCount,
                "live stress expected first-phase entry for each peer")) {
        cleanup_clients();
        return false;
    }
    if (!Expect(connected_count_by_peer.size() == kPeerCount,
                "live stress expected connected count entry for each peer")) {
        cleanup_clients();
        return false;
    }
    if (!Expect(disconnected_count_by_peer.size() == kPeerCount,
                "live stress expected disconnected count entry for each peer")) {
        cleanup_clients();
        return false;
    }
    if (!Expect(received_count_by_peer.size() == kPeerCount,
                "live stress expected received count entry for each peer")) {
        cleanup_clients();
        return false;
    }

    for (const auto& [peer, first_phase] : first_phase_by_peer) {
        if (!Expect(first_phase == 0, "live stress first event for peer must be Connected")) {
            cleanup_clients();
            return false;
        }
        if (!Expect(connected_count_by_peer[peer] == 1, "live stress expected exactly one connect per peer")) {
            cleanup_clients();
            return false;
        }
        if (!Expect(received_count_by_peer[peer] == kBurstMessagesPerPeer,
                    "live stress expected full received burst count per peer")) {
            cleanup_clients();
            return false;
        }
        if (!Expect(disconnected_count_by_peer[peer] == 1,
                    "live stress expected exactly one disconnect per peer")) {
            cleanup_clients();
            return false;
        }
        if (!Expect(global_phase_by_peer[peer] == 2,
                    "live stress expected peer terminal phase to be Disconnected")) {
            cleanup_clients();
            return false;
        }
    }

    cleanup_clients();
    return true;
}

bool TestLiveLoopbackMultiPeerFairnessUnderReconnectChurn() {
    constexpr uint32_t kPeerCount = 20;
    constexpr uint32_t kRounds = 24;
    constexpr uint32_t kChurnPerRound = 4;
    constexpr uint32_t kMinRoundActivePeers = 10;

    uint16_t port = 0;
    auto transport = CreateLoopbackServerTransport(&port);
    if (!Expect(transport && transport->isReady(),
                "live fairness stress failed to create loopback server transport")) {
        return false;
    }

    std::vector<LoopbackClientEndpoint> clients{};
    clients.reserve(kPeerCount);
    for (uint32_t i = 0; i < kPeerCount; ++i) {
        auto endpoint = CreateLoopbackClientWithRetry(port);
        if (!Expect(endpoint.has_value(),
                    "live fairness stress failed to create loopback client endpoint")) {
            for (auto& client : clients) {
                DestroyLoopbackClient(&client);
            }
            return false;
        }
        clients.push_back(std::move(*endpoint));
    }

    auto cleanup_clients = [&]() {
        for (auto& client : clients) {
            DestroyLoopbackClient(&client);
        }
    };

    std::unordered_map<karma::network::PeerToken, int> global_phase_by_peer{};
    std::unordered_map<karma::network::PeerToken, int> first_phase_by_peer{};
    std::unordered_map<karma::network::PeerToken, uint32_t> connected_count_by_peer{};
    std::unordered_map<karma::network::PeerToken, uint32_t> received_count_by_peer{};
    std::unordered_map<karma::network::PeerToken, uint32_t> disconnected_count_by_peer{};
    std::vector<uint32_t> attempted_by_client(kPeerCount, 0);
    std::vector<uint32_t> received_by_client(kPeerCount, 0);
    std::unordered_set<uint32_t> delivered_round_keys{};

    const auto pump_step = [&]() -> bool {
        for (auto& client : clients) {
            PumpLoopbackClient(&client);
        }
        std::vector<ServerTransportEvent> events{};
        transport->poll(karma::network::ServerTransportPollOptions{}, &events);
        if (!ValidateAndAccumulatePerPeerOrdering(events,
                                                  &global_phase_by_peer,
                                                  &first_phase_by_peer,
                                                  &connected_count_by_peer,
                                                  &received_count_by_peer,
                                                  &disconnected_count_by_peer,
                                                  true)) {
            return false;
        }
        for (const auto& event : events) {
            uint8_t client_index = 0;
            uint8_t sequence = 0;
            if (!DecodeLoopbackPayload(event, &client_index, &sequence)) {
                continue;
            }
            if (client_index >= kPeerCount) {
                return Fail("live fairness stress observed payload with invalid client index");
            }
            ++received_by_client[client_index];
            const uint32_t round_key =
                (static_cast<uint32_t>(client_index) << 8U) | static_cast<uint32_t>(sequence);
            delivered_round_keys.insert(round_key);
        }
        return true;
    };

    const auto connect_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(6);
    while (std::chrono::steady_clock::now() < connect_deadline) {
        if (!pump_step()) {
            cleanup_clients();
            return false;
        }
        const bool all_clients_connected = std::all_of(clients.begin(),
                                                       clients.end(),
                                                       [](const LoopbackClientEndpoint& client) {
                                                           return client.connected;
                                                       });
        if (all_clients_connected && connected_count_by_peer.size() >= kPeerCount) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!std::all_of(clients.begin(),
                     clients.end(),
                     [](const LoopbackClientEndpoint& client) { return client.connected; })) {
        for (uint32_t idx = 0; idx < kPeerCount; ++idx) {
            if (clients[idx].connected && clients[idx].peer) {
                continue;
            }
            DestroyLoopbackClient(&clients[idx]);
            auto endpoint = CreateLoopbackClientWithRetry(port);
            if (!Expect(endpoint.has_value(),
                        "live fairness stress failed to recover initially-unconnected client")) {
                cleanup_clients();
                return false;
            }
            clients[idx] = std::move(*endpoint);
        }

        const auto reconnect_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        while (std::chrono::steady_clock::now() < reconnect_deadline) {
            if (!pump_step()) {
                cleanup_clients();
                return false;
            }
            if (std::all_of(clients.begin(),
                            clients.end(),
                            [](const LoopbackClientEndpoint& client) {
                                return client.connected;
                            })) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    if (!Expect(std::all_of(clients.begin(),
                            clients.end(),
                            [](const LoopbackClientEndpoint& client) { return client.connected; }),
                "live fairness stress timed out waiting for all clients to connect")) {
        cleanup_clients();
        return false;
    }

    uint32_t total_active_peers = 0;
    uint32_t min_active_peers = kPeerCount;
    for (uint32_t round = 0; round < kRounds; ++round) {
        std::unordered_set<uint32_t> churn_indices{};
        for (uint32_t slot = 0; slot < kChurnPerRound; ++slot) {
            const uint32_t idx = (round * 5 + slot * 7) % kPeerCount;
            churn_indices.insert(idx);
        }

        for (const auto idx : churn_indices) {
            if (clients[idx].peer) {
                enet_peer_disconnect_now(clients[idx].peer, 0);
            }
            DestroyLoopbackClient(&clients[idx]);
            auto endpoint = CreateLoopbackClientWithRetry(port);
            if (!Expect(endpoint.has_value(),
                        "live fairness stress failed to recreate churned client endpoint")) {
                cleanup_clients();
                return false;
            }
            clients[idx] = std::move(*endpoint);
        }

        const auto settle_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (std::chrono::steady_clock::now() < settle_deadline) {
            if (!pump_step()) {
                cleanup_clients();
                return false;
            }
            if (CountActiveClients(clients) >= kMinRoundActivePeers) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        std::vector<uint32_t> active_indices{};
        active_indices.reserve(kPeerCount);
        for (uint32_t idx = 0; idx < kPeerCount; ++idx) {
            if (clients[idx].connected && clients[idx].peer) {
                active_indices.push_back(idx);
            }
        }
        if (!Expect(static_cast<uint32_t>(active_indices.size()) >= kMinRoundActivePeers,
                    "live fairness stress active peer count dropped below churn-resilient minimum")) {
            cleanup_clients();
            return false;
        }
        total_active_peers += static_cast<uint32_t>(active_indices.size());
        min_active_peers = std::min<uint32_t>(min_active_peers, static_cast<uint32_t>(active_indices.size()));

        std::unordered_set<uint32_t> expected_round_keys{};
        for (const auto idx : active_indices) {
            const uint8_t client_index = static_cast<uint8_t>(idx);
            const uint8_t sequence = static_cast<uint8_t>(round);
            if (!Expect(SendLoopbackPayload(&clients[idx], client_index, sequence),
                        "live fairness stress failed to send round payload")) {
                cleanup_clients();
                return false;
            }
            ++attempted_by_client[idx];
            const uint32_t round_key =
                (static_cast<uint32_t>(client_index) << 8U) | static_cast<uint32_t>(sequence);
            expected_round_keys.insert(round_key);
        }

        const auto round_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < round_deadline) {
            if (!pump_step()) {
                cleanup_clients();
                return false;
            }

            bool round_complete = true;
            for (const auto key : expected_round_keys) {
                if (delivered_round_keys.find(key) == delivered_round_keys.end()) {
                    round_complete = false;
                    break;
                }
            }
            if (round_complete) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        bool round_complete = true;
        for (const auto key : expected_round_keys) {
            if (delivered_round_keys.find(key) == delivered_round_keys.end()) {
                round_complete = false;
                break;
            }
        }
        if (!Expect(round_complete,
                    "live fairness stress timed out waiting for active-peer round delivery")) {
            cleanup_clients();
            return false;
        }
    }

    for (auto& client : clients) {
        if (client.peer) {
            enet_peer_disconnect(client.peer, 0);
        }
    }

    const auto disconnect_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(7);
    while (std::chrono::steady_clock::now() < disconnect_deadline) {
        if (!pump_step()) {
            cleanup_clients();
            return false;
        }
        bool all_disconnected = true;
        for (const auto& client : clients) {
            if (client.connected) {
                all_disconnected = false;
                break;
            }
        }
        if (all_disconnected) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    const double average_active_peers =
        static_cast<double>(total_active_peers) / static_cast<double>(kRounds);
    if (!Expect(average_active_peers >= (static_cast<double>(kPeerCount) * 0.60),
                "live fairness stress active peer average dropped below fairness threshold")) {
        cleanup_clients();
        return false;
    }
    if (!Expect(min_active_peers >= kMinRoundActivePeers,
                "live fairness stress observed too few active peers in a churn round")) {
        cleanup_clients();
        return false;
    }

    for (uint32_t idx = 0; idx < kPeerCount; ++idx) {
        if (!Expect(attempted_by_client[idx] > 0,
                    "live fairness stress expected each client to contribute active payloads")) {
            cleanup_clients();
            return false;
        }
        if (!Expect(received_by_client[idx] == attempted_by_client[idx],
                    "live fairness stress observed payload delivery mismatch for client")) {
            cleanup_clients();
            return false;
        }
    }

    cleanup_clients();
    return true;
}

bool TestUnregisteredBackendWarnsAndFails() {
    constexpr std::string_view kLoggerName = "server_transport_contract_logger";
    auto sink = std::make_shared<karma::network::tests::StructuredLogEventSink>();
    auto logger = std::make_shared<spdlog::logger>(std::string{kLoggerName}, sink);
    logger->set_level(spdlog::level::trace);

    auto previous_logger = spdlog::default_logger();
    spdlog::set_default_logger(logger);

    karma::network::ServerTransportConfig config{};
    config.backend_name = "unregistered-server-backend-contract";
    auto transport = karma::network::CreateServerTransport(config);

    spdlog::set_default_logger(previous_logger);

    const size_t warning_count = sink->CountLevelForLogger(spdlog::level::warn, kLoggerName);
    const size_t error_count = sink->CountLevelForLogger(spdlog::level::err, kLoggerName);

    return Expect(!transport, "unregistered server backend should fail transport creation") &&
           Expect(warning_count == 1,
                  "unregistered server backend should emit exactly one warning log event") &&
           Expect(error_count == 0,
                  "unregistered server backend should not emit error-level log events");
}

} // namespace

int main() {
    if (!TestConnectedReceivedDisconnectedOrdering()) {
        return 1;
    }
    if (!TestDisconnectIsTerminalWithinPumpCycle()) {
        return 1;
    }
    if (!TestPerPeerOrderingAndDisconnectEdges()) {
        return 1;
    }
    if (!TestHighVolumeMultiPeerOrderingStress()) {
        return 1;
    }
    if (!TestLiveLoopbackMultiPeerOrderingStress()) {
        return 1;
    }
    if (!TestLiveLoopbackMultiPeerFairnessUnderReconnectChurn()) {
        return 1;
    }
    if (!TestUnregisteredBackendWarnsAndFails()) {
        return 1;
    }
    return 0;
}
