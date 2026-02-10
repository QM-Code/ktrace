#include "net/protocol.hpp"
#include "net/protocol_codec.hpp"
#include "server/net/enet_event_source.hpp"

#include "karma/common/config_store.hpp"

#include <enet.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace {

enum class TestResult {
    Pass,
    Skip,
    Fail
};

struct ServerFixture {
    uint16_t port = 0;
    std::unique_ptr<bz3::server::net::ServerEventSource> source{};
};

struct ClientCapture {
    bool connected = false;
    bool disconnected = false;
    std::vector<bz3::net::ServerMessage> messages{};
};

bool Fail(const std::string& message) {
    std::cerr << message << "\n";
    return false;
}

TestResult FailTest(const std::string& message) {
    std::cerr << "FAIL: " << message << "\n";
    return TestResult::Fail;
}

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        return Fail(message);
    }
    return true;
}

void PrintSkip(const std::string& message) {
    std::cerr << "SKIP: " << message << "\n";
}

std::filesystem::path MakeTestConfigPath(const char* suffix) {
    const auto nonce = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() /
           ("bz3-enet-loopback-" + std::string(suffix) + "-" + std::to_string(nonce) + ".json");
}

void InitEmptyConfig(const char* suffix) {
    karma::config::ConfigStore::Initialize({}, MakeTestConfigPath(suffix));
    (void)karma::config::ConfigStore::Set("config.SaveIntervalSeconds", 5.0);
    (void)karma::config::ConfigStore::Set("config.MergeIntervalSeconds", 5.0);
}

std::optional<ServerFixture> CreateServerFixture() {
    constexpr uint16_t kFirstPort = 32000;
    constexpr uint16_t kLastPort = 32032;
    for (uint16_t port = kFirstPort; port < kLastPort; ++port) {
        auto source = bz3::server::net::CreateEnetServerEventSource(port);
        if (source) {
            return ServerFixture{port, std::move(source)};
        }
    }
    return std::nullopt;
}

void PumpClient(ENetHost* client_host, ClientCapture* capture) {
    if (!client_host || !capture) {
        return;
    }
    ENetEvent event{};
    while (enet_host_service(client_host, &event, 0) > 0) {
        switch (event.type) {
            case ENET_EVENT_TYPE_CONNECT:
                capture->connected = true;
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
            case ENET_EVENT_TYPE_DISCONNECT_TIMEOUT:
                capture->disconnected = true;
                break;
            case ENET_EVENT_TYPE_RECEIVE: {
                const auto decoded = bz3::net::DecodeServerMessage(event.packet->data, event.packet->dataLength);
                if (decoded.has_value()) {
                    capture->messages.push_back(std::move(*decoded));
                }
                enet_packet_destroy(event.packet);
                break;
            }
            default:
                break;
        }
    }
}

template <typename StepFn, typename DoneFn>
bool WaitUntil(std::chrono::milliseconds timeout, StepFn&& step, DoneFn&& done) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        step();
        if (done()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    step();
    return done();
}

bool SendPayload(ENetHost* client_host, ENetPeer* peer, const std::vector<std::byte>& payload) {
    if (!client_host || !peer || payload.empty()) {
        return false;
    }
    ENetPacket* packet =
        enet_packet_create(payload.data(), payload.size(), ENET_PACKET_FLAG_RELIABLE);
    if (!packet) {
        return false;
    }
    if (enet_peer_send(peer, 0, packet) != 0) {
        enet_packet_destroy(packet);
        return false;
    }
    enet_host_flush(client_host);
    return true;
}

TestResult TestAcceptedJoinAndGameplayEvents() {
    InitEmptyConfig("accepted");
    auto fixture = CreateServerFixture();
    if (!fixture.has_value()) {
        PrintSkip("unable to create ENet server fixture (socket bind unavailable)");
        return TestResult::Skip;
    }

    ENetHost* client_host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!client_host) {
        PrintSkip("unable to create ENet client host");
        return TestResult::Skip;
    }

    ENetAddress address{};
    if (enet_address_set_host(&address, "127.0.0.1") != 0) {
        enet_host_destroy(client_host);
        return FailTest("accepted-test: failed to resolve 127.0.0.1");
    }
    address.port = fixture->port;
    ENetPeer* peer = enet_host_connect(client_host, &address, 2, 0);
    if (!peer) {
        enet_host_destroy(client_host);
        return FailTest("accepted-test: enet_host_connect returned null");
    }

    ClientCapture capture{};
    std::vector<bz3::server::net::ServerInputEvent> server_events{};
    const auto step = [&]() {
        auto polled = fixture->source->poll();
        server_events.insert(server_events.end(), polled.begin(), polled.end());
        PumpClient(client_host, &capture);
    };

    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() { return capture.connected; })) {
        enet_host_destroy(client_host);
        return FailTest("accepted-test: timed out waiting for ENET_EVENT_TYPE_CONNECT");
    }

    const auto join_payload = bz3::net::EncodeClientJoinRequest(
        "loopback-player",
        bz3::net::kProtocolVersion,
        "cached-hash",
        "cached-world",
        "cached-rev",
        "cached-content",
        "cached-manifest",
        3);
    if (!SendPayload(client_host, peer, join_payload)) {
        enet_host_destroy(client_host);
        return FailTest("accepted-test: failed to send join payload");
    }

    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() {
            return std::any_of(server_events.begin(),
                               server_events.end(),
                               [](const bz3::server::net::ServerInputEvent& event) {
                                   return event.type == bz3::server::net::ServerInputEvent::Type::ClientJoin;
                               });
        })) {
        enet_host_destroy(client_host);
        return FailTest("accepted-test: timed out waiting for server ClientJoin event");
    }

    uint32_t joined_client_id = 0;
    std::string joined_name{};
    for (const auto& event : server_events) {
        if (event.type == bz3::server::net::ServerInputEvent::Type::ClientJoin) {
            joined_client_id = event.join.client_id;
            joined_name = event.join.player_name;
            break;
        }
    }
    if (!Expect(joined_client_id != 0, "joined client_id not captured")) {
        enet_host_destroy(client_host);
        return TestResult::Fail;
    }
    if (!Expect(joined_name == "loopback-player", "joined player name mismatch")) {
        enet_host_destroy(client_host);
        return TestResult::Fail;
    }

    const std::vector<bz3::server::net::SessionSnapshotEntry> sessions{
        bz3::server::net::SessionSnapshotEntry{joined_client_id, "loopback-player"}};
    const std::vector<bz3::server::net::WorldManifestEntry> world_manifest{
        bz3::server::net::WorldManifestEntry{"config.json", 100, "h1"}};
    const std::vector<std::byte> world_payload{std::byte{0x10}, std::byte{0x20}, std::byte{0x30}};
    fixture->source->onJoinResult(joined_client_id,
                                  true,
                                  "",
                                  "loopback-world",
                                  "world-id",
                                  "world-rev",
                                  "pkg-hash",
                                  "content-hash",
                                  "manifest-hash",
                                  static_cast<uint32_t>(world_manifest.size()),
                                  world_payload.size(),
                                  std::filesystem::path{},
                                  sessions,
                                  world_manifest,
                                  world_payload);

    bool saw_join_accept = false;
    bool saw_init = false;
    bool saw_snapshot = false;
    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() {
            for (const auto& message : capture.messages) {
                if (message.type == bz3::net::ServerMessageType::JoinResponse && message.join_accepted) {
                    saw_join_accept = true;
                } else if (message.type == bz3::net::ServerMessageType::Init
                           && message.client_id == joined_client_id
                           && message.world_id == "world-id") {
                    saw_init = true;
                } else if (message.type == bz3::net::ServerMessageType::SessionSnapshot
                           && !message.sessions.empty()) {
                    saw_snapshot = true;
                }
            }
            return saw_join_accept && saw_init && saw_snapshot;
        })) {
        enet_host_destroy(client_host);
        return FailTest(
            "accepted-test: timed out waiting for JoinResponse/Init/SessionSnapshot after onJoinResult");
    }

    server_events.clear();
    const auto spawn_payload = bz3::net::EncodeClientRequestPlayerSpawn(joined_client_id);
    if (!SendPayload(client_host, peer, spawn_payload)) {
        enet_host_destroy(client_host);
        return FailTest("accepted-test: failed to send request_spawn payload");
    }
    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() {
            return std::any_of(server_events.begin(),
                               server_events.end(),
                               [](const bz3::server::net::ServerInputEvent& event) {
                                   return event.type ==
                                          bz3::server::net::ServerInputEvent::Type::ClientRequestSpawn;
                               });
        })) {
        enet_host_destroy(client_host);
        return FailTest("accepted-test: timed out waiting for ClientRequestSpawn server event");
    }

    server_events.clear();
    const auto shot_payload = bz3::net::EncodeClientCreateShot(joined_client_id,
                                                               77,
                                                               bz3::net::Vec3{1.0f, 2.0f, 3.0f},
                                                               bz3::net::Vec3{4.0f, 5.0f, 6.0f});
    if (!SendPayload(client_host, peer, shot_payload)) {
        enet_host_destroy(client_host);
        return FailTest("accepted-test: failed to send create_shot payload");
    }
    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() {
            return std::any_of(server_events.begin(),
                               server_events.end(),
                               [joined_client_id](const bz3::server::net::ServerInputEvent& event) {
                                   return event.type ==
                                              bz3::server::net::ServerInputEvent::Type::ClientCreateShot
                                          && event.create_shot.client_id == joined_client_id
                                          && event.create_shot.local_shot_id == 77;
                               });
        })) {
        enet_host_destroy(client_host);
        return FailTest("accepted-test: timed out waiting for ClientCreateShot server event");
    }

    enet_peer_disconnect(peer, 0);
    WaitUntil(std::chrono::milliseconds(300), step, [&]() { return capture.disconnected; });
    enet_host_destroy(client_host);
    return TestResult::Pass;
}

TestResult TestProtocolMismatchRejected() {
    InitEmptyConfig("mismatch");
    auto fixture = CreateServerFixture();
    if (!fixture.has_value()) {
        PrintSkip("unable to create ENet server fixture for mismatch test");
        return TestResult::Skip;
    }

    ENetHost* client_host = enet_host_create(nullptr, 1, 2, 0, 0);
    if (!client_host) {
        PrintSkip("unable to create ENet client host for mismatch test");
        return TestResult::Skip;
    }

    ENetAddress address{};
    if (enet_address_set_host(&address, "127.0.0.1") != 0) {
        enet_host_destroy(client_host);
        return FailTest("mismatch-test: failed to resolve 127.0.0.1");
    }
    address.port = fixture->port;
    ENetPeer* peer = enet_host_connect(client_host, &address, 2, 0);
    if (!peer) {
        enet_host_destroy(client_host);
        return FailTest("mismatch-test: enet_host_connect returned null");
    }

    ClientCapture capture{};
    std::vector<bz3::server::net::ServerInputEvent> server_events{};
    const auto step = [&]() {
        auto polled = fixture->source->poll();
        server_events.insert(server_events.end(), polled.begin(), polled.end());
        PumpClient(client_host, &capture);
    };

    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() { return capture.connected; })) {
        enet_host_destroy(client_host);
        return FailTest("mismatch-test: timed out waiting for ENET_EVENT_TYPE_CONNECT");
    }

    const auto bad_join_payload = bz3::net::EncodeClientJoinRequest(
        "loopback-player",
        bz3::net::kProtocolVersion + 99,
        "",
        "",
        "",
        "",
        "",
        0);
    if (!SendPayload(client_host, peer, bad_join_payload)) {
        enet_host_destroy(client_host);
        return FailTest("mismatch-test: failed to send bad join payload");
    }

    bool saw_reject = false;
    bool saw_disconnect = false;
    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() {
            for (const auto& message : capture.messages) {
                if (message.type == bz3::net::ServerMessageType::JoinResponse && !message.join_accepted) {
                    saw_reject = true;
                    return true;
                }
            }
            saw_disconnect = capture.disconnected;
            return saw_disconnect;
        })) {
        enet_host_destroy(client_host);
        return FailTest("mismatch-test: timed out waiting for rejected JoinResponse or disconnect");
    }

    if (!saw_reject && !saw_disconnect) {
        enet_host_destroy(client_host);
        return FailTest("mismatch-test: neither reject response nor disconnect observed");
    }

    if (!Expect(std::none_of(server_events.begin(),
                             server_events.end(),
                             [](const bz3::server::net::ServerInputEvent& event) {
                                 return event.type == bz3::server::net::ServerInputEvent::Type::ClientJoin;
                             }),
                "server emitted ClientJoin event for protocol-mismatch request")) {
        enet_host_destroy(client_host);
        return TestResult::Fail;
    }

    enet_host_destroy(client_host);
    return TestResult::Pass;
}

} // namespace

int main() {
    const auto accepted_result = TestAcceptedJoinAndGameplayEvents();
    if (accepted_result == TestResult::Fail) {
        return 1;
    }
    const auto mismatch_result = TestProtocolMismatchRejected();
    if (mismatch_result == TestResult::Fail) {
        return 1;
    }
    if (accepted_result == TestResult::Skip && mismatch_result == TestResult::Skip) {
        return 77;
    }
    return 0;
}
