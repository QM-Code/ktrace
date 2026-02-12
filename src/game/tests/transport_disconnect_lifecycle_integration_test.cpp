#include "net/protocol.hpp"
#include "net/protocol_codec.hpp"
#include "server/net/transport_event_source.hpp"

#include "karma/common/config_store.hpp"
#include "network/tests/loopback_transport_fixture.hpp"

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
};

struct ClientEndpoint {
    karma::network::tests::LoopbackTransportEndpoint endpoint{};
    ClientCapture capture{};
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

std::filesystem::path MakeTestConfigPath() {
    const auto nonce = static_cast<unsigned long long>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return std::filesystem::temp_directory_path() /
           ("bz3-transport-disconnect-" + std::to_string(nonce) + ".json");
}

void InitEmptyConfig() {
    karma::config::ConfigStore::Initialize({}, MakeTestConfigPath());
    (void)karma::config::ConfigStore::Set("config.SaveIntervalSeconds", 5.0);
    (void)karma::config::ConfigStore::Set("config.MergeIntervalSeconds", 5.0);
}

std::optional<ServerFixture> CreateServerFixture() {
    constexpr uint16_t kFirstPort = 32150;
    constexpr uint16_t kLastPort = 32200;
    for (uint16_t port = kFirstPort; port < kLastPort; ++port) {
        auto source = bz3::server::net::CreateServerTransportEventSource(port);
        if (source) {
            return ServerFixture{port, std::move(source)};
        }
    }
    return std::nullopt;
}

std::optional<ClientEndpoint> CreateClientEndpoint(uint16_t port) {
    auto loopback_endpoint = karma::network::tests::CreateLoopbackClientTransportEndpoint(port, 2);
    if (!loopback_endpoint.has_value()) {
        return std::nullopt;
    }

    ClientEndpoint endpoint{};
    endpoint.endpoint = std::move(*loopback_endpoint);
    return endpoint;
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

    karma::network::tests::PumpLoopbackTransportEndpoint(&endpoint->endpoint);
    endpoint->capture.connected = endpoint->capture.connected || endpoint->endpoint.connected;
    endpoint->capture.disconnected = endpoint->capture.disconnected || endpoint->endpoint.disconnected;
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

size_t CountEvents(const std::vector<bz3::server::net::ServerInputEvent>& events,
                   bz3::server::net::ServerInputEvent::Type type,
                   uint32_t client_id) {
    size_t count = 0;
    for (const auto& event : events) {
        if (event.type != type) {
            continue;
        }
        uint32_t event_client_id = 0;
        switch (type) {
            case bz3::server::net::ServerInputEvent::Type::ClientJoin:
                event_client_id = event.join.client_id;
                break;
            case bz3::server::net::ServerInputEvent::Type::ClientLeave:
                event_client_id = event.leave.client_id;
                break;
            case bz3::server::net::ServerInputEvent::Type::ClientRequestSpawn:
                event_client_id = event.request_spawn.client_id;
                break;
            case bz3::server::net::ServerInputEvent::Type::ClientCreateShot:
                event_client_id = event.create_shot.client_id;
                break;
        }
        if (event_client_id == client_id) {
            ++count;
        }
    }
    return count;
}

bool HasJoinBeforeLeaveForClient(const std::vector<bz3::server::net::ServerInputEvent>& events,
                                 uint32_t client_id) {
    std::optional<size_t> join_index{};
    std::optional<size_t> leave_index{};
    for (size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        if (event.type == bz3::server::net::ServerInputEvent::Type::ClientJoin
            && event.join.client_id == client_id && !join_index.has_value()) {
            join_index = index;
            continue;
        }
        if (event.type == bz3::server::net::ServerInputEvent::Type::ClientLeave
            && event.leave.client_id == client_id && !leave_index.has_value()) {
            leave_index = index;
        }
    }
    return join_index.has_value() && leave_index.has_value() && *join_index < *leave_index;
}

std::optional<uint32_t> FindJoinedClientId(const std::vector<bz3::server::net::ServerInputEvent>& events,
                                           const std::string& player_name) {
    for (const auto& event : events) {
        if (event.type == bz3::server::net::ServerInputEvent::Type::ClientJoin
            && event.join.player_name == player_name) {
            return event.join.client_id;
        }
    }
    return std::nullopt;
}

std::optional<uint32_t> FindCreateShotClientId(const std::vector<bz3::server::net::ServerInputEvent>& events,
                                               uint32_t local_shot_id) {
    for (const auto& event : events) {
        if (event.type == bz3::server::net::ServerInputEvent::Type::ClientCreateShot
            && event.create_shot.local_shot_id == local_shot_id) {
            return event.create_shot.client_id;
        }
    }
    return std::nullopt;
}

TestResult TestDisconnectLifecycle() {
    InitEmptyConfig();
    auto fixture = CreateServerFixture();
    if (!fixture.has_value()) {
        PrintSkip("unable to create transport server fixture");
        return TestResult::Skip;
    }

    auto client_a_opt = CreateClientEndpoint(fixture->port);
    if (!client_a_opt.has_value()) {
        PrintSkip("unable to create first transport client");
        return TestResult::Skip;
    }
    ClientEndpoint client_a = std::move(*client_a_opt);
    std::vector<bz3::server::net::ServerInputEvent> server_events{};

    const auto step_a = [&]() {
        auto polled = fixture->source->poll();
        server_events.insert(server_events.end(), polled.begin(), polled.end());
        PumpClient(&client_a);
    };

    if (!WaitUntil(std::chrono::milliseconds(1200), step_a, [&]() { return client_a.capture.connected; })) {
        DestroyClientEndpoint(&client_a);
        return FailTest("disconnect-test: timed out waiting for first client connect");
    }

    if (!SendPayload(&client_a,
                     bz3::net::EncodeClientJoinRequest(
                         "alpha", bz3::net::kProtocolVersion, "", "", "", "", "", 0))) {
        DestroyClientEndpoint(&client_a);
        return FailTest("disconnect-test: failed sending alpha join");
    }

    if (!WaitUntil(std::chrono::milliseconds(1200), step_a, [&]() {
            return FindJoinedClientId(server_events, "alpha").has_value();
        })) {
        DestroyClientEndpoint(&client_a);
        return FailTest("disconnect-test: timed out waiting for alpha ClientJoin");
    }

    const auto first_id_opt = FindJoinedClientId(server_events, "alpha");
    if (!first_id_opt.has_value()) {
        DestroyClientEndpoint(&client_a);
        return FailTest("disconnect-test: missing alpha client id after join");
    }
    const uint32_t first_id = *first_id_opt;

    server_events.clear();
    static_cast<void>(karma::network::tests::DisconnectLoopbackTransportEndpoint(&client_a.endpoint, 0));
    static_cast<void>(karma::network::tests::DisconnectLoopbackTransportEndpoint(&client_a.endpoint, 0));
    if (!WaitUntil(std::chrono::milliseconds(1200), step_a, [&]() {
            return client_a.capture.disconnected
                   && CountEvents(
                          server_events, bz3::server::net::ServerInputEvent::Type::ClientLeave, first_id)
                          >= 1;
        })) {
        DestroyClientEndpoint(&client_a);
        return FailTest("disconnect-test: timed out waiting for leave event from disconnect");
    }

    for (int i = 0; i < 60; ++i) {
        step_a();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!Expect(
            CountEvents(server_events, bz3::server::net::ServerInputEvent::Type::ClientLeave, first_id) == 1,
            "disconnect-test: expected exactly one leave event for first client")) {
        DestroyClientEndpoint(&client_a);
        return TestResult::Fail;
    }
    DestroyClientEndpoint(&client_a);

    server_events.clear();
    auto client_b_opt = CreateClientEndpoint(fixture->port);
    if (!client_b_opt.has_value()) {
        PrintSkip("unable to create second transport client");
        return TestResult::Skip;
    }
    ClientEndpoint client_b = std::move(*client_b_opt);
    const auto step_b = [&]() {
        auto polled = fixture->source->poll();
        server_events.insert(server_events.end(), polled.begin(), polled.end());
        PumpClient(&client_b);
    };

    if (!WaitUntil(std::chrono::milliseconds(1200), step_b, [&]() { return client_b.capture.connected; })) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: timed out waiting for second client connect");
    }

    if (!SendPayload(&client_b,
                     bz3::net::EncodeClientJoinRequest(
                         "beta", bz3::net::kProtocolVersion, "", "", "", "", "", 0))) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: failed sending beta join");
    }

    if (!WaitUntil(std::chrono::milliseconds(1200), step_b, [&]() {
            return FindJoinedClientId(server_events, "beta").has_value();
        })) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: timed out waiting for beta ClientJoin");
    }

    const auto second_id_opt = FindJoinedClientId(server_events, "beta");
    if (!second_id_opt.has_value()) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: missing beta client id after join");
    }
    const uint32_t second_id = *second_id_opt;
    if (!Expect(first_id != second_id, "disconnect-test: reconnect reused stale client id")) {
        DestroyClientEndpoint(&client_b);
        return TestResult::Fail;
    }

    server_events.clear();
    constexpr uint32_t kLocalShotId = 4242;
    if (!SendPayload(&client_b,
                     bz3::net::EncodeClientCreateShot(first_id,
                                                      kLocalShotId,
                                                      bz3::net::Vec3{1.0f, 2.0f, 3.0f},
                                                      bz3::net::Vec3{4.0f, 5.0f, 6.0f}))) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: failed sending create_shot from second client");
    }

    if (!WaitUntil(std::chrono::milliseconds(1200), step_b, [&]() {
            return FindCreateShotClientId(server_events, kLocalShotId).has_value();
        })) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: timed out waiting for create_shot after reconnect");
    }

    const auto shot_source_opt = FindCreateShotClientId(server_events, kLocalShotId);
    if (!shot_source_opt.has_value()) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: missing create_shot event source");
    }
    if (!Expect(*shot_source_opt == second_id,
                "disconnect-test: create_shot used stale client id instead of current connection id")) {
        DestroyClientEndpoint(&client_b);
        return TestResult::Fail;
    }

    server_events.clear();
    if (!SendPayload(&client_b, bz3::net::EncodeClientLeave(first_id))) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: failed sending explicit leave");
    }

    if (!WaitUntil(std::chrono::milliseconds(1200), step_b, [&]() {
            return CountEvents(server_events,
                               bz3::server::net::ServerInputEvent::Type::ClientLeave,
                               second_id)
                   >= 1;
        })) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: timed out waiting for explicit leave event");
    }

    if (!SendPayload(&client_b, bz3::net::EncodeClientRequestPlayerSpawn(second_id + 1))) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: failed sending request_spawn after explicit leave");
    }
    if (!SendPayload(&client_b,
                     bz3::net::EncodeClientCreateShot(second_id + 1,
                                                      4343,
                                                      bz3::net::Vec3{1.0f, 2.0f, 3.0f},
                                                      bz3::net::Vec3{4.0f, 5.0f, 6.0f}))) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: failed sending create_shot after explicit leave");
    }

    for (int i = 0; i < 80; ++i) {
        step_b();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!Expect(CountEvents(server_events, bz3::server::net::ServerInputEvent::Type::ClientRequestSpawn, second_id)
                    == 0,
                "disconnect-test: post-leave request_spawn should be ignored")) {
        DestroyClientEndpoint(&client_b);
        return TestResult::Fail;
    }
    if (!Expect(CountEvents(server_events, bz3::server::net::ServerInputEvent::Type::ClientCreateShot, second_id)
                    == 0,
                "disconnect-test: post-leave create_shot should be ignored")) {
        DestroyClientEndpoint(&client_b);
        return TestResult::Fail;
    }

    static_cast<void>(karma::network::tests::DisconnectLoopbackTransportEndpoint(&client_b.endpoint, 0));
    if (!WaitUntil(std::chrono::milliseconds(600), step_b, [&]() { return client_b.capture.disconnected; })) {
        DestroyClientEndpoint(&client_b);
        return FailTest("disconnect-test: timed out waiting for disconnect after explicit leave");
    }

    for (int i = 0; i < 60; ++i) {
        step_b();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!Expect(
            CountEvents(server_events, bz3::server::net::ServerInputEvent::Type::ClientLeave, second_id) == 1,
            "disconnect-test: explicit leave + disconnect emitted duplicate leave event")) {
        DestroyClientEndpoint(&client_b);
        return TestResult::Fail;
    }

    DestroyClientEndpoint(&client_b);

    std::optional<uint32_t> previous_churn_id{};
    for (int cycle = 0; cycle < 2; ++cycle) {
        server_events.clear();
        auto churn_client_opt = CreateClientEndpoint(fixture->port);
        if (!churn_client_opt.has_value()) {
            PrintSkip("unable to create churn transport client");
            return TestResult::Skip;
        }
        ClientEndpoint churn_client = std::move(*churn_client_opt);
        const auto churn_step = [&]() {
            auto polled = fixture->source->poll();
            server_events.insert(server_events.end(), polled.begin(), polled.end());
            PumpClient(&churn_client);
        };

        if (!WaitUntil(std::chrono::milliseconds(1200),
                       churn_step,
                       [&]() { return churn_client.capture.connected; })) {
            DestroyClientEndpoint(&churn_client);
            return FailTest("disconnect-test: timed out waiting for churn client connect");
        }

        const std::string churn_name = "churn-" + std::to_string(cycle);
        if (!SendPayload(&churn_client,
                         bz3::net::EncodeClientJoinRequest(
                             churn_name, bz3::net::kProtocolVersion, "", "", "", "", "", 0))) {
            DestroyClientEndpoint(&churn_client);
            return FailTest("disconnect-test: failed sending churn join");
        }

        if (!WaitUntil(std::chrono::milliseconds(1200), churn_step, [&]() {
                return FindJoinedClientId(server_events, churn_name).has_value();
            })) {
            DestroyClientEndpoint(&churn_client);
            return FailTest("disconnect-test: timed out waiting for churn ClientJoin");
        }

        const auto churn_id_opt = FindJoinedClientId(server_events, churn_name);
        if (!churn_id_opt.has_value()) {
            DestroyClientEndpoint(&churn_client);
            return FailTest("disconnect-test: missing churn client id after join");
        }
        const uint32_t churn_id = *churn_id_opt;
        if (!Expect(!previous_churn_id.has_value() || *previous_churn_id != churn_id,
                    "disconnect-test: rapid reconnect reused stale client id")) {
            DestroyClientEndpoint(&churn_client);
            return TestResult::Fail;
        }

        const uint32_t leave_payload_id = previous_churn_id.has_value() ? *previous_churn_id : churn_id;
        if (!SendPayload(&churn_client, bz3::net::EncodeClientLeave(leave_payload_id))) {
            DestroyClientEndpoint(&churn_client);
            return FailTest("disconnect-test: failed sending churn leave");
        }
        static_cast<void>(karma::network::tests::DisconnectLoopbackTransportEndpoint(&churn_client.endpoint, 0));
        static_cast<void>(karma::network::tests::DisconnectLoopbackTransportEndpoint(&churn_client.endpoint, 0));

        if (!WaitUntil(std::chrono::milliseconds(1200), churn_step, [&]() {
                return churn_client.capture.disconnected
                       && CountEvents(server_events,
                                      bz3::server::net::ServerInputEvent::Type::ClientLeave,
                                      churn_id)
                              >= 1;
            })) {
            DestroyClientEndpoint(&churn_client);
            return FailTest("disconnect-test: timed out waiting for churn leave/disconnect");
        }

        for (int i = 0; i < 60; ++i) {
            churn_step();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (!Expect(CountEvents(server_events, bz3::server::net::ServerInputEvent::Type::ClientJoin, churn_id)
                        == 1,
                    "disconnect-test: churn cycle expected exactly one join event")) {
            DestroyClientEndpoint(&churn_client);
            return TestResult::Fail;
        }
        if (!Expect(CountEvents(server_events, bz3::server::net::ServerInputEvent::Type::ClientLeave, churn_id)
                        == 1,
                    "disconnect-test: churn cycle emitted duplicate leave event")) {
            DestroyClientEndpoint(&churn_client);
            return TestResult::Fail;
        }
        if (!Expect(HasJoinBeforeLeaveForClient(server_events, churn_id),
                    "disconnect-test: churn cycle emitted leave before join for same client")) {
            DestroyClientEndpoint(&churn_client);
            return TestResult::Fail;
        }

        previous_churn_id = churn_id;
        DestroyClientEndpoint(&churn_client);
    }

    server_events.clear();
    auto draining_client_opt = CreateClientEndpoint(fixture->port);
    if (!draining_client_opt.has_value()) {
        PrintSkip("unable to create draining transport client");
        return TestResult::Skip;
    }
    ClientEndpoint draining_client = std::move(*draining_client_opt);
    const auto draining_step = [&]() {
        auto polled = fixture->source->poll();
        server_events.insert(server_events.end(), polled.begin(), polled.end());
        PumpClient(&draining_client);
    };
    if (!WaitUntil(std::chrono::milliseconds(1200),
                   draining_step,
                   [&]() { return draining_client.capture.connected; })) {
        DestroyClientEndpoint(&draining_client);
        return FailTest("disconnect-test: timed out waiting for draining client connect");
    }

    if (!SendPayload(&draining_client,
                     bz3::net::EncodeClientJoinRequest(
                         "drain-old", bz3::net::kProtocolVersion, "", "", "", "", "", 0))) {
        DestroyClientEndpoint(&draining_client);
        return FailTest("disconnect-test: failed sending drain-old join");
    }
    if (!WaitUntil(std::chrono::milliseconds(1200), draining_step, [&]() {
            return FindJoinedClientId(server_events, "drain-old").has_value();
        })) {
        DestroyClientEndpoint(&draining_client);
        return FailTest("disconnect-test: timed out waiting for drain-old ClientJoin");
    }
    const auto draining_id_opt = FindJoinedClientId(server_events, "drain-old");
    if (!draining_id_opt.has_value()) {
        DestroyClientEndpoint(&draining_client);
        return FailTest("disconnect-test: missing drain-old client id after join");
    }
    const uint32_t draining_id = *draining_id_opt;

    server_events.clear();
    static_cast<void>(karma::network::tests::DisconnectLoopbackTransportEndpoint(&draining_client.endpoint, 0));
    static_cast<void>(karma::network::tests::DisconnectLoopbackTransportEndpoint(&draining_client.endpoint, 0));

    auto takeover_client_opt = CreateClientEndpoint(fixture->port);
    if (!takeover_client_opt.has_value()) {
        DestroyClientEndpoint(&draining_client);
        PrintSkip("unable to create takeover transport client");
        return TestResult::Skip;
    }
    ClientEndpoint takeover_client = std::move(*takeover_client_opt);
    const auto overlap_step = [&]() {
        auto polled = fixture->source->poll();
        server_events.insert(server_events.end(), polled.begin(), polled.end());
        PumpClient(&draining_client);
        PumpClient(&takeover_client);
    };
    if (!WaitUntil(std::chrono::milliseconds(1200),
                   overlap_step,
                   [&]() { return takeover_client.capture.connected; })) {
        DestroyClientEndpoint(&takeover_client);
        DestroyClientEndpoint(&draining_client);
        return FailTest("disconnect-test: timed out waiting for takeover client connect");
    }
    if (!SendPayload(&takeover_client,
                     bz3::net::EncodeClientJoinRequest(
                         "drain-new", bz3::net::kProtocolVersion, "", "", "", "", "", 0))) {
        DestroyClientEndpoint(&takeover_client);
        DestroyClientEndpoint(&draining_client);
        return FailTest("disconnect-test: failed sending drain-new join");
    }
    if (!WaitUntil(std::chrono::milliseconds(1200), overlap_step, [&]() {
            return FindJoinedClientId(server_events, "drain-new").has_value()
                   && CountEvents(
                          server_events, bz3::server::net::ServerInputEvent::Type::ClientLeave, draining_id)
                          >= 1;
        })) {
        DestroyClientEndpoint(&takeover_client);
        DestroyClientEndpoint(&draining_client);
        return FailTest("disconnect-test: timed out waiting for overlap join/leave race coverage");
    }

    const auto takeover_id_opt = FindJoinedClientId(server_events, "drain-new");
    if (!takeover_id_opt.has_value()) {
        DestroyClientEndpoint(&takeover_client);
        DestroyClientEndpoint(&draining_client);
        return FailTest("disconnect-test: missing drain-new client id after overlap join");
    }
    const uint32_t takeover_id = *takeover_id_opt;
    if (!Expect(takeover_id != draining_id,
                "disconnect-test: overlap join reused prior draining client id")) {
        DestroyClientEndpoint(&takeover_client);
        DestroyClientEndpoint(&draining_client);
        return TestResult::Fail;
    }

    for (int i = 0; i < 80; ++i) {
        overlap_step();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    if (!Expect(
            CountEvents(server_events, bz3::server::net::ServerInputEvent::Type::ClientLeave, draining_id) == 1,
            "disconnect-test: overlap drain emitted duplicate leave for prior client")) {
        DestroyClientEndpoint(&takeover_client);
        DestroyClientEndpoint(&draining_client);
        return TestResult::Fail;
    }
    if (!Expect(
            CountEvents(server_events, bz3::server::net::ServerInputEvent::Type::ClientJoin, takeover_id) == 1,
            "disconnect-test: overlap join emitted duplicate join for takeover client")) {
        DestroyClientEndpoint(&takeover_client);
        DestroyClientEndpoint(&draining_client);
        return TestResult::Fail;
    }
    if (!Expect(
            CountEvents(server_events, bz3::server::net::ServerInputEvent::Type::ClientLeave, takeover_id) == 0,
            "disconnect-test: overlap drain leaked leave event onto takeover client")) {
        DestroyClientEndpoint(&takeover_client);
        DestroyClientEndpoint(&draining_client);
        return TestResult::Fail;
    }

    DestroyClientEndpoint(&takeover_client);
    DestroyClientEndpoint(&draining_client);

    return TestResult::Pass;
}

} // namespace

int main() {
    const auto result = TestDisconnectLifecycle();
    if (result == TestResult::Pass) {
        return 0;
    }
    if (result == TestResult::Skip) {
        return kSkipReturnCode;
    }
    return 1;
}
