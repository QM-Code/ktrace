#include "server/runtime.hpp"
#include "server/community_heartbeat.hpp"
#include "server/domain/shot_system.hpp"
#include "server/domain/world_session.hpp"
#include "server/net/event_source.hpp"
#include "server/server_game.hpp"
#include "server/runtime_event_rules.hpp"

#include "karma/app/backend_resolution.hpp"
#include "karma/app/bootstrap_scaffold.hpp"
#include "karma/app/engine_server_app.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_store.hpp"
#include "karma/common/config_validation.hpp"
#include "karma/common/logging.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <csignal>
#include <memory>
#include <stdexcept>
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
    if (!karma::app::ReportRequiredConfigIssues(issues, options.strict_config)) {
        return 1;
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
                    "CLI option --community set: '{}'",
                    options.community);
    }
    if (options.backend_physics_explicit) {
        KARMA_TRACE("engine.server",
                    "CLI option --backend-physics set: '{}'",
                    options.backend_physics);
    }
    if (options.backend_audio_explicit) {
        KARMA_TRACE("engine.server",
                    "CLI option --backend-audio set: '{}'",
                    options.backend_audio);
    }

    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);

    g_running.store(true);
    karma::app::EngineServerConfig engineConfig{};
    engineConfig.target_tick_hz = karma::config::ReadFloatConfig({"simulation.fixedHz"}, engineConfig.target_tick_hz);
    engineConfig.max_delta_time = karma::config::ReadFloatConfig({"simulation.maxFrameDeltaTime"},
                                                                  engineConfig.max_delta_time);
    engineConfig.max_substeps =
        static_cast<int>(karma::config::ReadUInt16Config({"simulation.maxSubsteps"},
                                                          static_cast<uint16_t>(engineConfig.max_substeps)));
    engineConfig.physics_backend =
        karma::app::ResolvePhysicsBackendFromOption(options.backend_physics, options.backend_physics_explicit);
    engineConfig.audio_backend =
        karma::app::ResolveAudioBackendFromOption(options.backend_audio, options.backend_audio_explicit);
    engineConfig.enable_audio = options.backend_audio_explicit
        || karma::config::ReadBoolConfig({"audio.serverEnabled"}, false);
    const uint16_t listen_port = options.host_port_explicit
        ? options.host_port
        : karma::config::ReadUInt16Config({"network.ServerPort"}, static_cast<uint16_t>(11899));

    CommunityHeartbeat community_heartbeat{};
    const std::string community_override = options.community_explicit ? options.community : std::string{};
    community_heartbeat.configureFromConfig(karma::config::ConfigStore::Merged(),
                                            listen_port,
                                            community_override);
    if (community_heartbeat.enabled()) {
        spdlog::info("Community heartbeat enabled: target='{}' advertise='{}' interval={}s max_players={}",
                     community_heartbeat.communityUrl(),
                     community_heartbeat.serverAddress(),
                     community_heartbeat.intervalSeconds(),
                     community_heartbeat.maxPlayers());
    } else {
        spdlog::info("Community heartbeat disabled: target='{}' advertise='{}' interval={}s",
                     community_heartbeat.communityUrl(),
                     community_heartbeat.serverAddress(),
                     community_heartbeat.intervalSeconds());
    }
    ServerGame game{world_context->world_name};
    std::unique_ptr<net::ServerEventSource> event_source = net::CreateServerEventSource(options);
    domain::ShotSystem shot_system{};
    const float shot_lifetime_seconds =
        std::max(0.1f, karma::config::ReadFloatConfig({"gameplay.shotLifetimeSeconds"}, 5.0f));
    shot_system.setLifetime(std::chrono::milliseconds(static_cast<int64_t>(shot_lifetime_seconds * 1000.0f)));
    const float shot_step_dt =
        (engineConfig.target_tick_hz > 1e-6f) ? (1.0f / engineConfig.target_tick_hz) : (1.0f / 60.0f);
    uint32_t next_global_shot_id = 1;
    RuntimeEventRuleHandlers runtime_handlers{};
    runtime_handlers.has_client = [&game](uint32_t client_id) {
        return game.hasClient(client_id);
    };
    runtime_handlers.on_client_leave = [&game](uint32_t client_id) {
        return game.onClientLeave(client_id);
    };
    runtime_handlers.on_player_death = [event_source_ptr = event_source.get()](uint32_t client_id) {
        event_source_ptr->onPlayerDeath(client_id);
    };
    runtime_handlers.on_player_spawn = [event_source_ptr = event_source.get()](uint32_t client_id) {
        event_source_ptr->onPlayerSpawn(client_id);
    };
    runtime_handlers.on_create_shot =
        [event_source_ptr = event_source.get(), &shot_system](uint32_t source_client_id,
                                                              uint32_t global_shot_id,
                                                              float pos_x,
                                                              float pos_y,
                                                              float pos_z,
                                                              float vel_x,
                                                              float vel_y,
                                                              float vel_z) {
            shot_system.addShot(source_client_id,
                                global_shot_id,
                                pos_x,
                                pos_y,
                                pos_z,
                                vel_x,
                                vel_y,
                                vel_z);
            event_source_ptr->onCreateShot(source_client_id,
                                           global_shot_id,
                                           pos_x,
                                           pos_y,
                                           pos_z,
                                           vel_x,
                                           vel_y,
                                           vel_z);
        };
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
                    static const std::vector<std::byte> empty_world_package{};
                    const auto& world_package =
                        world_context->world_package_enabled ? world_context->world_package : empty_world_package;
                    std::vector<net::WorldManifestEntry> world_manifest{};
                    world_manifest.reserve(world_context->world_manifest.size());
                    for (const auto& entry : world_context->world_manifest) {
                        world_manifest.push_back(net::WorldManifestEntry{
                            .path = entry.path,
                            .size = entry.size,
                            .hash = entry.hash});
                    }
                    event_source->onJoinResult(event.join.client_id,
                                               accepted,
                                               reason,
                                               world_context->world_name,
                                               world_context->world_id,
                                               world_context->world_revision,
                                               world_context->world_package_hash,
                                               world_context->world_content_hash,
                                               world_context->world_manifest_hash,
                                               world_context->world_manifest_file_count,
                                               world_context->world_package_size,
                                               world_context->world_dir,
                                               sessions,
                                               world_manifest,
                                               world_package);
                    break;
                }
                case net::ServerInputEvent::Type::ClientLeave:
                case net::ServerInputEvent::Type::ClientRequestSpawn:
                case net::ServerInputEvent::Type::ClientCreateShot: {
                    const auto rule_result =
                        ApplyRuntimeEventRules(event, runtime_handlers, &next_global_shot_id);
                    if (rule_result == RuntimeEventRuleResult::Applied) {
                        break;
                    }

                    if (rule_result == RuntimeEventRuleResult::IgnoredUnknownClient) {
                        if (event.type == net::ServerInputEvent::Type::ClientLeave) {
                            KARMA_TRACE("net.server",
                                        "RunRuntime: ignoring leave for unknown client_id={}",
                                        event.leave.client_id);
                        } else if (event.type == net::ServerInputEvent::Type::ClientRequestSpawn) {
                            KARMA_TRACE("net.server",
                                        "RunRuntime: ignoring spawn request for unknown client_id={}",
                                        event.request_spawn.client_id);
                        } else {
                            KARMA_TRACE("net.server",
                                        "RunRuntime: ignoring create_shot for unknown client_id={} local_shot_id={}",
                                        event.create_shot.client_id,
                                        event.create_shot.local_shot_id);
                        }
                        break;
                    }

                    if (rule_result == RuntimeEventRuleResult::IgnoredLeaveFailed
                        && event.type == net::ServerInputEvent::Type::ClientLeave) {
                        KARMA_TRACE("net.server",
                                    "RunRuntime: leave ignored because client_id={} disconnect failed",
                                    event.leave.client_id);
                        break;
                    }

                    KARMA_TRACE("net.server",
                                "RunRuntime: runtime event rule not applied type={} result={}",
                                static_cast<int>(event.type),
                                static_cast<int>(rule_result));
                    break;
                }
            }
        }
        app.tick();
        const auto expired_shots = shot_system.update(domain::ShotSystem::Clock::now(), shot_step_dt);
        if (!expired_shots.empty()) {
            for (const auto& expired : expired_shots) {
                event_source->onRemoveShot(expired.global_shot_id, true);
            }
            KARMA_TRACE("net.server",
                        "RunRuntime: expired shots removed count={} remaining={}",
                        expired_shots.size(),
                        shot_system.activeShotCount());
        }
        community_heartbeat.update(game);
        if (!g_running.load()) {
            app.requestStop();
        }
    }

    return 0;
}

} // namespace bz3::server
