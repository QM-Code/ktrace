#include "server/runtime.hpp"
#include "server/domain/world_session.hpp"
#include "server/net/event_source.hpp"
#include "server/server_game.hpp"

#include "karma/app/engine_server_app.hpp"
#include "karma/common/config_validation.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <csignal>
#include <memory>
#include <string>
#include <vector>

namespace bz3::server {

namespace {

std::atomic<bool> g_running{true};

void OnSignal(int signum) {
    KARMA_TRACE("engine.server", "bz3-server: received signal {}, requesting stop", signum);
    g_running.store(false);
}

} // namespace

int RunRuntime(const CLIOptions& options) {
    const auto issues = karma::config::ValidateRequiredKeys(karma::config::ServerRequiredKeys());
    if (!issues.empty()) {
        for (const auto& issue : issues) {
            if (options.strict_config) {
                spdlog::error("config validation: {}: {}", issue.path, issue.message);
            } else {
                spdlog::warn("config validation: {}: {}", issue.path, issue.message);
            }
        }
        if (options.strict_config) {
            return 1;
        }
    }

    const auto world_context = domain::LoadWorldSessionContext(options);
    if (!world_context.has_value()) {
        return 1;
    }
    if (options.host_port_explicit) {
        KARMA_TRACE("engine.server",
                    "CLI option --port set: {}",
                    options.host_port);
    }
    if (options.community_explicit) {
        KARMA_TRACE("engine.server",
                    "CLI option --community parsed (not wired yet): '{}'",
                    options.community);
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    g_running.store(true);
    karma::app::EngineServerConfig engineConfig{};
    ServerGame game{world_context->world_name};
    std::unique_ptr<net::ServerEventSource> event_source = net::CreateServerEventSource(options);
    karma::app::EngineServerApp app{};
    app.start(game, engineConfig);

    while (app.isRunning()) {
        for (const auto& event : event_source->poll()) {
            switch (event.type) {
                case net::ServerInputEvent::Type::ClientJoin: {
                    const bool accepted =
                        game.onClientJoin(event.join.client_id, event.join.player_name);
                    std::vector<net::SessionSnapshotEntry> sessions{};
                    if (accepted) {
                        for (const auto& session : game.activeSessionSnapshot()) {
                            sessions.push_back(net::SessionSnapshotEntry{
                                session.session_id,
                                session.session_name});
                        }
                    }
                    const std::string reason =
                        accepted
                            ? std::string{}
                            : (game.lastJoinRejectReason().empty()
                                   ? std::string("Join rejected by server.")
                                   : game.lastJoinRejectReason());
                    event_source->onJoinResult(event.join.client_id,
                                               accepted,
                                               reason,
                                               world_context->world_name,
                                               sessions);
                    break;
                }
                case net::ServerInputEvent::Type::ClientLeave:
                    game.onClientLeave(event.leave.client_id);
                    break;
            }
        }
        app.tick();
        if (!g_running.load()) {
            app.requestStop();
        }
    }

    return 0;
}

} // namespace bz3::server
