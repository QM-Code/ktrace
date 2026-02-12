#include "net/protocol.hpp"
#include "net/protocol_codec.hpp"
#include "server/net/transport_event_source.hpp"

#include "karma/common/config_store.hpp"
#include "network/tests/loopback_transport_fixture.hpp"

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
           ("bz3-transport-loopback-" + std::string(suffix) + "-" + std::to_string(nonce) + ".json");
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
        auto source = bz3::server::net::CreateServerTransportEventSource(port);
        if (source) {
            return ServerFixture{port, std::move(source)};
        }
    }
    return std::nullopt;
}

void PumpClient(karma::network::tests::LoopbackTransportEndpoint* client_endpoint,
                ClientCapture* capture) {
    if (!client_endpoint || !capture) {
        return;
    }

    std::vector<std::vector<std::byte>> payloads{};
    karma::network::tests::PumpLoopbackTransportEndpointCapturePayloads(client_endpoint, &payloads);
    capture->connected = capture->connected || client_endpoint->connected;
    capture->disconnected = capture->disconnected || client_endpoint->disconnected;
    for (const auto& payload : payloads) {
        const auto decoded = bz3::net::DecodeServerMessage(payload.data(), payload.size());
        if (decoded.has_value()) {
            capture->messages.push_back(std::move(*decoded));
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

bool SendPayload(karma::network::tests::LoopbackTransportEndpoint* client_endpoint,
                 const std::vector<std::byte>& payload) {
    if (!client_endpoint || payload.empty()) {
        return false;
    }
    return karma::network::tests::SendLoopbackTransportPayload(client_endpoint, payload);
}

TestResult TestAcceptedJoinAndGameplayEvents() {
    InitEmptyConfig("accepted");
    auto fixture = CreateServerFixture();
    if (!fixture.has_value()) {
        PrintSkip("unable to create transport server fixture (socket bind unavailable)");
        return TestResult::Skip;
    }

    auto client_endpoint_opt = karma::network::tests::CreateLoopbackClientTransportEndpoint(fixture->port, 2);
    if (!client_endpoint_opt.has_value()) {
        PrintSkip("unable to create transport client host");
        return TestResult::Skip;
    }
    auto client_endpoint = std::move(*client_endpoint_opt);

    ClientCapture capture{};
    std::vector<bz3::server::net::ServerInputEvent> server_events{};
    const auto step = [&]() {
        auto polled = fixture->source->poll();
        server_events.insert(server_events.end(), polled.begin(), polled.end());
        PumpClient(&client_endpoint, &capture);
    };

    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() { return capture.connected; })) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return FailTest("accepted-test: timed out waiting for client connect event");
    }

    if (!SendPayload(&client_endpoint, bz3::net::EncodeClientRequestPlayerSpawn(9999))
        || !SendPayload(&client_endpoint,
                        bz3::net::EncodeClientCreateShot(9999,
                                                         88,
                                                         bz3::net::Vec3{1.0f, 2.0f, 3.0f},
                                                         bz3::net::Vec3{4.0f, 5.0f, 6.0f}))
        || !SendPayload(&client_endpoint, bz3::net::EncodeClientLeave(9999))) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return FailTest("accepted-test: failed to send one or more pre-join payloads");
    }

    for (int i = 0; i < 120; ++i) {
        step();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!Expect(server_events.empty(),
                "accepted-test: pre-join spawn/shot/leave should not emit server-side events")) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return TestResult::Fail;
    }
    server_events.clear();

    const auto join_payload = bz3::net::EncodeClientJoinRequest(
        "loopback-player",
        bz3::net::kProtocolVersion,
        "cached-hash",
        "cached-world",
        "cached-rev",
        "cached-content",
        "cached-manifest",
        3);
    if (!SendPayload(&client_endpoint, join_payload)) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return FailTest("accepted-test: failed to send join payload");
    }

    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() {
            return std::any_of(server_events.begin(),
                               server_events.end(),
                               [](const bz3::server::net::ServerInputEvent& event) {
                                   return event.type == bz3::server::net::ServerInputEvent::Type::ClientJoin;
                               });
        })) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
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
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return TestResult::Fail;
    }
    if (!Expect(joined_name == "loopback-player", "joined player name mismatch")) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
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
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return FailTest(
            "accepted-test: timed out waiting for JoinResponse/Init/SessionSnapshot after onJoinResult");
    }

    server_events.clear();
    const auto spawn_payload = bz3::net::EncodeClientRequestPlayerSpawn(joined_client_id + 1);
    if (!SendPayload(&client_endpoint, spawn_payload)) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return FailTest("accepted-test: failed to send request_spawn payload");
    }
    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() {
            return std::any_of(server_events.begin(),
                               server_events.end(),
                               [joined_client_id](const bz3::server::net::ServerInputEvent& event) {
                                   return event.type ==
                                              bz3::server::net::ServerInputEvent::Type::ClientRequestSpawn
                                          && event.request_spawn.client_id == joined_client_id;
                               });
        })) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return FailTest("accepted-test: timed out waiting for ClientRequestSpawn server event");
    }

    server_events.clear();
    const auto shot_payload = bz3::net::EncodeClientCreateShot(joined_client_id + 1,
                                                               77,
                                                               bz3::net::Vec3{1.0f, 2.0f, 3.0f},
                                                               bz3::net::Vec3{4.0f, 5.0f, 6.0f});
    if (!SendPayload(&client_endpoint, shot_payload)) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
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
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return FailTest("accepted-test: timed out waiting for ClientCreateShot server event");
    }

    static_cast<void>(karma::network::tests::DisconnectLoopbackTransportEndpoint(&client_endpoint, 0));
    WaitUntil(std::chrono::milliseconds(300), step, [&]() { return capture.disconnected; });
    karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
    return TestResult::Pass;
}

TestResult TestProtocolMismatchRejected() {
    InitEmptyConfig("mismatch");
    auto fixture = CreateServerFixture();
    if (!fixture.has_value()) {
        PrintSkip("unable to create transport server fixture for mismatch test");
        return TestResult::Skip;
    }

    auto client_endpoint_opt = karma::network::tests::CreateLoopbackClientTransportEndpoint(fixture->port, 2);
    if (!client_endpoint_opt.has_value()) {
        PrintSkip("unable to create transport client host for mismatch test");
        return TestResult::Skip;
    }
    auto client_endpoint = std::move(*client_endpoint_opt);

    ClientCapture capture{};
    std::vector<bz3::server::net::ServerInputEvent> server_events{};
    const auto step = [&]() {
        auto polled = fixture->source->poll();
        server_events.insert(server_events.end(), polled.begin(), polled.end());
        PumpClient(&client_endpoint, &capture);
    };

    if (!WaitUntil(std::chrono::milliseconds(1000), step, [&]() { return capture.connected; })) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return FailTest("mismatch-test: timed out waiting for client connect event");
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
    if (!SendPayload(&client_endpoint, bad_join_payload)) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
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
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return FailTest("mismatch-test: timed out waiting for rejected JoinResponse or disconnect");
    }

    if (!saw_reject && !saw_disconnect) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return FailTest("mismatch-test: neither reject response nor disconnect observed");
    }

    if (!Expect(std::none_of(server_events.begin(),
                             server_events.end(),
                             [](const bz3::server::net::ServerInputEvent& event) {
                                 return event.type == bz3::server::net::ServerInputEvent::Type::ClientJoin;
                             }),
                "server emitted ClientJoin event for protocol-mismatch request")) {
        karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
        return TestResult::Fail;
    }

    karma::network::tests::DestroyLoopbackTransportEndpoint(&client_endpoint);
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
