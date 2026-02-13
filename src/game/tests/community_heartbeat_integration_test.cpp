#include "server/cli_options.hpp"
#include "server/community_heartbeat.hpp"
#include "server/heartbeat_client.hpp"

#include "karma/common/json.hpp"

#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

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

class RecordingHeartbeatClient final : public bz3::server::IHeartbeatClient {
 public:
    struct Call {
        std::string community_url{};
        std::string server_address{};
        int players = 0;
        int max_players = 0;
    };

    void requestHeartbeat(const std::string& community_url,
                          const std::string& server_address,
                          int players,
                          int max_players) override {
        calls.push_back(Call{
            .community_url = community_url,
            .server_address = server_address,
            .players = players,
            .max_players = max_players});
    }

    std::vector<Call> calls{};
};

bool TestCommunityOverrideDispatchesHeartbeat() {
    std::vector<std::string> args{
        "bz3-server",
        "--community",
        "localhost:8080"};
    std::vector<char*> argv{};
    argv.reserve(args.size());
    for (auto& arg : args) {
        argv.push_back(arg.data());
    }

    const bz3::server::CLIOptions options =
        bz3::server::ParseCLIOptions(static_cast<int>(argv.size()), argv.data());
    if (!Expect(options.community_explicit, "expected --community to set community_explicit")) {
        return false;
    }
    if (!Expect(options.community == "localhost:8080", "expected --community value to round-trip")) {
        return false;
    }

    const karma::json::Value merged_config{
        {"community",
         karma::json::Value{
             {"enabled", false},
             {"server", "http://ignored.example:9999"},
             {"heartbeatIntervalSeconds", 5}}},
        {"maxPlayers", 20}};

    auto recording_client = std::make_unique<RecordingHeartbeatClient>();
    auto* recorded = recording_client.get();
    bz3::server::CommunityHeartbeat heartbeat(std::move(recording_client));
    heartbeat.configureFromConfig(merged_config,
                                  11899,
                                  options.community_explicit ? options.community : std::string{});

    if (!Expect(heartbeat.enabled(), "community override should force heartbeat enabled")) {
        return false;
    }
    if (!Expect(heartbeat.communityUrl() == "http://localhost:8080",
                "community override should normalize scheme")) {
        return false;
    }
    if (!Expect(heartbeat.intervalSeconds() == 5, "heartbeat interval should come from config")) {
        return false;
    }

    heartbeat.update(static_cast<size_t>(3));
    heartbeat.update(static_cast<size_t>(4));

    if (!Expect(recorded->calls.size() == 1, "expected exactly one heartbeat dispatch")) {
        return false;
    }

    const auto& call = recorded->calls.front();
    return Expect(call.community_url == "http://localhost:8080",
                  "heartbeat should dispatch to --community target")
           && Expect(call.server_address == "11899",
                     "heartbeat should advertise listen port when host is not configured")
           && Expect(call.players == 3, "heartbeat should forward connected player count")
           && Expect(call.max_players == 20, "heartbeat should forward maxPlayers from config");
}

} // namespace

int main() {
    if (!TestCommunityOverrideDispatchesHeartbeat()) {
        return 1;
    }
    return 0;
}
