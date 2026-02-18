#include "net/protocol.hpp"
#include "net/protocol_codec.hpp"
#include "server/net/transport_event_source.hpp"

#include "karma/common/config/store.hpp"
#include "network/tests/support/loopback_transport_fixture.hpp"

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
#include <utility>
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

struct ClientEndpoint {
    karma::network::tests::LoopbackTransportEndpoint endpoint{};
    ClientCapture capture{};
    std::string name{};
};

constexpr int kSkipReturnCode = 77;

void PrintSkip(const std::string& message) {
    std::cerr << "SKIP: " << message << "\n";
}

TestResult FailTest(const std::string& message) {
    std::cerr << "FAIL: " << message << "\n";
    return TestResult::Fail;
}

bool Expect(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        return false;
    }
    return true;
}

std::filesystem::path MakeTestConfigPath(const char* suffix) {
    const auto nonce = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() /
           ("bz3-transport-multiclient-" + std::string(suffix) + "-" + std::to_string(nonce) + ".json");
}

void InitEmptyConfig(const char* suffix) {
    karma::common::config::ConfigStore::Initialize({}, MakeTestConfigPath(suffix));
    (void)karma::common::config::ConfigStore::Set("config.SaveIntervalSeconds", 5.0);
    (void)karma::common::config::ConfigStore::Set("config.MergeIntervalSeconds", 5.0);
}

std::optional<ServerFixture> CreateServerFixture() {
    constexpr uint16_t kFirstPort = 32100;
    constexpr uint16_t kLastPort = 32148;
    for (uint16_t port = kFirstPort; port < kLastPort; ++port) {
        auto source = bz3::server::net::CreateServerTransportEventSource(port);
        if (source) {
            return ServerFixture{port, std::move(source)};
        }
    }
    return std::nullopt;
}

std::optional<ClientEndpoint> CreateClientEndpoint(uint16_t port, std::string name) {
    auto loopback_endpoint = karma::network::tests::CreateLoopbackClientTransportEndpoint(port, 2);
    if (!loopback_endpoint.has_value()) {
        return std::nullopt;
    }

    ClientEndpoint client{};
    client.endpoint = std::move(*loopback_endpoint);
    client.name = std::move(name);
    return client;
}

void DestroyClientEndpoint(ClientEndpoint* endpoint) {
    if (!endpoint) {
        return;
    }
    karma::network::tests::DestroyLoopbackTransportEndpoint(&endpoint->endpoint);
}

void PumpClient(ClientEndpoint* endpoint) {
    if (!endpoint) {
        return;
    }

    std::vector<std::vector<std::byte>> payloads{};
    karma::network::tests::PumpLoopbackTransportEndpointCapturePayloads(&endpoint->endpoint, &payloads);
    endpoint->capture.connected = endpoint->capture.connected || endpoint->endpoint.connected;
    endpoint->capture.disconnected = endpoint->capture.disconnected || endpoint->endpoint.disconnected;
    for (const auto& payload : payloads) {
        const auto decoded = bz3::net::DecodeServerMessage(payload.data(), payload.size());
        if (decoded.has_value()) {
            endpoint->capture.messages.push_back(std::move(*decoded));
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

bool SendPayload(ClientEndpoint* endpoint, const std::vector<std::byte>& payload) {
    if (!endpoint || payload.empty()) {
        return false;
    }
    return karma::network::tests::SendLoopbackTransportPayload(&endpoint->endpoint, payload);
}

bool HasAcceptedHandshake(const ClientEndpoint& endpoint, uint32_t expected_client_id) {
    bool saw_accept = false;
    bool saw_init = false;
    bool saw_snapshot = false;
    for (const auto& message : endpoint.capture.messages) {
        if (message.type == bz3::net::ServerMessageType::JoinResponse && message.join_accepted) {
            saw_accept = true;
        } else if (message.type == bz3::net::ServerMessageType::Init && message.client_id == expected_client_id) {
            saw_init = true;
        } else if (message.type == bz3::net::ServerMessageType::SessionSnapshot && !message.sessions.empty()) {
            saw_snapshot = true;
        }
    }
    return saw_accept && saw_init && saw_snapshot;
}

bool HasPlayerSpawnFor(const ClientEndpoint& endpoint, uint32_t client_id) {
    return std::any_of(endpoint.capture.messages.begin(),
                       endpoint.capture.messages.end(),
                       [client_id](const bz3::net::ServerMessage& message) {
                           return message.type == bz3::net::ServerMessageType::PlayerSpawn
                                  && message.event_client_id == client_id;
                       });
}

bool HasPlayerDeathFor(const ClientEndpoint& endpoint, uint32_t client_id) {
    return std::any_of(endpoint.capture.messages.begin(),
                       endpoint.capture.messages.end(),
                       [client_id](const bz3::net::ServerMessage& message) {
                           return message.type == bz3::net::ServerMessageType::PlayerDeath
                                  && message.event_client_id == client_id;
                       });
}

bool HasCreateShotFor(const ClientEndpoint& endpoint, uint32_t source_client_id, uint32_t global_shot_id) {
    return std::any_of(endpoint.capture.messages.begin(),
                       endpoint.capture.messages.end(),
                       [source_client_id, global_shot_id](const bz3::net::ServerMessage& message) {
                           return message.type == bz3::net::ServerMessageType::CreateShot
                                  && message.event_client_id == source_client_id
                                  && message.event_shot_id == global_shot_id;
                       });
}

void ClearMessages(ClientEndpoint* endpoint) {
    if (!endpoint) {
        return;
    }
    endpoint->capture.messages.clear();
}

TestResult TestMultiClientBroadcasts() {
    InitEmptyConfig("broadcast");
    auto fixture = CreateServerFixture();
    if (!fixture.has_value()) {
        PrintSkip("unable to create transport server fixture");
        return TestResult::Skip;
    }

    auto client_a_opt = CreateClientEndpoint(fixture->port, "client-a");
    if (!client_a_opt.has_value()) {
        PrintSkip("unable to create first transport client");
        return TestResult::Skip;
    }
    auto client_b_opt = CreateClientEndpoint(fixture->port, "client-b");
    if (!client_b_opt.has_value()) {
        DestroyClientEndpoint(&client_a_opt.value());
        PrintSkip("unable to create second transport client");
        return TestResult::Skip;
    }

    ClientEndpoint client_a = std::move(*client_a_opt);
    ClientEndpoint client_b = std::move(*client_b_opt);
    std::vector<bz3::server::net::ServerInputEvent> server_events{};

    const auto step = [&]() {
        auto polled = fixture->source->poll();
        server_events.insert(server_events.end(), polled.begin(), polled.end());
        PumpClient(&client_a);
        PumpClient(&client_b);
    };

    if (!WaitUntil(std::chrono::milliseconds(1200),
                   step,
                   [&]() { return client_a.capture.connected && client_b.capture.connected; })) {
        DestroyClientEndpoint(&client_a);
        DestroyClientEndpoint(&client_b);
        return FailTest("multiclient: timed out waiting for both clients to connect");
    }

    const auto join_a = bz3::net::EncodeClientJoinRequest(
        "alpha",
        bz3::net::kProtocolVersion,
        "",
        "",
        "",
        "",
        "",
        0);
    const auto join_b = bz3::net::EncodeClientJoinRequest(
        "beta",
        bz3::net::kProtocolVersion,
        "",
        "",
        "",
        "",
        "",
        0);
    if (!SendPayload(&client_a, join_a) || !SendPayload(&client_b, join_b)) {
        DestroyClientEndpoint(&client_a);
        DestroyClientEndpoint(&client_b);
        return FailTest("multiclient: failed sending one or both join payloads");
    }

    if (!WaitUntil(std::chrono::milliseconds(1200), step, [&]() {
            int join_count = 0;
            for (const auto& event : server_events) {
                if (event.type == bz3::server::net::ServerInputEvent::Type::ClientJoin) {
                    ++join_count;
                }
            }
            return join_count >= 2;
        })) {
        DestroyClientEndpoint(&client_a);
        DestroyClientEndpoint(&client_b);
        return FailTest("multiclient: timed out waiting for two server-side ClientJoin events");
    }

    uint32_t alpha_id = 0;
    uint32_t beta_id = 0;
    for (const auto& event : server_events) {
        if (event.type != bz3::server::net::ServerInputEvent::Type::ClientJoin) {
            continue;
        }
        if (event.join.player_name == "alpha") {
            alpha_id = event.join.client_id;
        } else if (event.join.player_name == "beta") {
            beta_id = event.join.client_id;
        }
    }

    if (!Expect(alpha_id != 0, "multiclient: missing alpha join id")
        || !Expect(beta_id != 0, "multiclient: missing beta join id")
        || !Expect(alpha_id != beta_id, "multiclient: alpha/beta ids must differ")) {
        DestroyClientEndpoint(&client_a);
        DestroyClientEndpoint(&client_b);
        return TestResult::Fail;
    }

    const std::vector<bz3::server::net::SessionSnapshotEntry> sessions{
        bz3::server::net::SessionSnapshotEntry{alpha_id, "alpha"},
        bz3::server::net::SessionSnapshotEntry{beta_id, "beta"}};
    const std::vector<bz3::server::net::WorldManifestEntry> world_manifest{};
    const std::vector<std::byte> world_payload{};

    fixture->source->onJoinResult(alpha_id,
                                  true,
                                  "",
                                  "loopback-world",
                                  "world-id",
                                  "world-rev",
                                  "pkg-hash",
                                  "content-hash",
                                  "manifest-hash",
                                  0,
                                  0,
                                  std::filesystem::path{},
                                  sessions,
                                  world_manifest,
                                  world_payload);
    fixture->source->onJoinResult(beta_id,
                                  true,
                                  "",
                                  "loopback-world",
                                  "world-id",
                                  "world-rev",
                                  "pkg-hash",
                                  "content-hash",
                                  "manifest-hash",
                                  0,
                                  0,
                                  std::filesystem::path{},
                                  sessions,
                                  world_manifest,
                                  world_payload);

    if (!WaitUntil(std::chrono::milliseconds(1200), step, [&]() {
            return HasAcceptedHandshake(client_a, alpha_id) && HasAcceptedHandshake(client_b, beta_id);
        })) {
        DestroyClientEndpoint(&client_a);
        DestroyClientEndpoint(&client_b);
        return FailTest("multiclient: timed out waiting for accepted handshakes on both clients");
    }

    ClearMessages(&client_a);
    ClearMessages(&client_b);
    fixture->source->onPlayerSpawn(alpha_id);

    if (!WaitUntil(std::chrono::milliseconds(1200), step, [&]() {
            return HasPlayerSpawnFor(client_a, alpha_id) && HasPlayerSpawnFor(client_b, alpha_id);
        })) {
        DestroyClientEndpoint(&client_a);
        DestroyClientEndpoint(&client_b);
        return FailTest("multiclient: timed out waiting for spawn broadcast to both clients");
    }

    ClearMessages(&client_a);
    ClearMessages(&client_b);
    constexpr uint32_t kGlobalShotId = 9001;
    fixture->source->onCreateShot(beta_id, kGlobalShotId, 1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f);

    if (!WaitUntil(std::chrono::milliseconds(1200), step, [&]() {
            return HasCreateShotFor(client_a, beta_id, kGlobalShotId)
                   && HasCreateShotFor(client_b, beta_id, kGlobalShotId);
        })) {
        DestroyClientEndpoint(&client_a);
        DestroyClientEndpoint(&client_b);
        return FailTest("multiclient: timed out waiting for create_shot broadcast to both clients");
    }

    ClearMessages(&client_a);
    ClearMessages(&client_b);
    fixture->source->onPlayerDeath(alpha_id);
    if (!WaitUntil(std::chrono::milliseconds(1200), step, [&]() {
            return HasPlayerDeathFor(client_a, alpha_id) && HasPlayerDeathFor(client_b, alpha_id);
        })) {
        DestroyClientEndpoint(&client_a);
        DestroyClientEndpoint(&client_b);
        return FailTest("multiclient: timed out waiting for player_death broadcast to both clients");
    }

    static_cast<void>(karma::network::tests::DisconnectLoopbackTransportEndpoint(&client_a.endpoint, 0));
    static_cast<void>(karma::network::tests::DisconnectLoopbackTransportEndpoint(&client_b.endpoint, 0));
    WaitUntil(std::chrono::milliseconds(250),
              step,
              [&]() { return client_a.capture.disconnected && client_b.capture.disconnected; });
    DestroyClientEndpoint(&client_a);
    DestroyClientEndpoint(&client_b);
    return TestResult::Pass;
}

} // namespace

int main() {
    const auto result = TestMultiClientBroadcasts();
    if (result == TestResult::Pass) {
        return 0;
    }
    if (result == TestResult::Skip) {
        return kSkipReturnCode;
    }
    return 1;
}
