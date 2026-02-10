#include "server/runtime.hpp"
#include "server/domain/world_session.hpp"
#include "server/net/event_source.hpp"
#include "server/server_game.hpp"
#include "server/runtime_event_rules.hpp"

#include "karma/app/engine_server_app.hpp"
#include "karma/audio/backend.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_validation.hpp"
#include "karma/common/logging.hpp"
#include "karma/physics/backend.hpp"

#include <spdlog/spdlog.h>

#include <atomic>
#include <algorithm>
#include <cstddef>
#include <csignal>
#include <memory>
#include <optional>
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

karma::physics_backend::BackendKind ResolvePhysicsBackendFromOptions(const CLIOptions& options) {
    const std::string configured = options.backend_physics_explicit
        ? options.backend_physics
        : karma::config::ReadStringConfig("physics.backend", "auto");
    const auto parsed = karma::physics_backend::ParseBackendKind(configured);
    if (!parsed) {
        const char* source =
            options.backend_physics_explicit ? "--backend-physics" : "config 'physics.backend'";
        throw std::runtime_error(std::string("Invalid value for ") + source + ": '" + configured +
                                 "' (expected: auto|jolt|physx)");
    }
    if (*parsed != karma::physics_backend::BackendKind::Auto) {
        const auto compiled = karma::physics_backend::CompiledBackends();
        const bool supported = std::any_of(
            compiled.begin(),
            compiled.end(),
            [parsed](karma::physics_backend::BackendKind kind) { return kind == *parsed; });
        if (!supported) {
            throw std::runtime_error(
                std::string("Configured physics backend '") + configured + "' is not compiled into this binary.");
        }
    }
    return *parsed;
}

karma::audio_backend::BackendKind ResolveAudioBackendFromOptions(const CLIOptions& options) {
    const std::string configured = options.backend_audio_explicit
        ? options.backend_audio
        : karma::config::ReadStringConfig("audio.backend", "auto");
    const auto parsed = karma::audio_backend::ParseBackendKind(configured);
    if (!parsed) {
        const char* source =
            options.backend_audio_explicit ? "--backend-audio" : "config 'audio.backend'";
        throw std::runtime_error(std::string("Invalid value for ") + source + ": '" + configured +
                                 "' (expected: auto|sdl3audio|miniaudio)");
    }
    if (*parsed != karma::audio_backend::BackendKind::Auto) {
        const auto compiled = karma::audio_backend::CompiledBackends();
        const bool supported = std::any_of(
            compiled.begin(),
            compiled.end(),
            [parsed](karma::audio_backend::BackendKind kind) { return kind == *parsed; });
        if (!supported) {
            throw std::runtime_error(
                std::string("Configured audio backend '") + configured + "' is not compiled into this binary.");
        }
    }
    return *parsed;
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
    engineConfig.physics_backend = ResolvePhysicsBackendFromOptions(options);
    engineConfig.audio_backend = ResolveAudioBackendFromOptions(options);
    engineConfig.enable_audio = options.backend_audio_explicit
        || karma::config::ReadBoolConfig({"audio.serverEnabled"}, false);
    ServerGame game{world_context->world_name};
    std::unique_ptr<net::ServerEventSource> event_source = net::CreateServerEventSource(options);
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
        [event_source_ptr = event_source.get()](uint32_t source_client_id,
                                                uint32_t global_shot_id,
                                                float pos_x,
                                                float pos_y,
                                                float pos_z,
                                                float vel_x,
                                                float vel_y,
                                                float vel_z) {
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
        if (!g_running.load()) {
            app.requestStop();
        }
    }

    return 0;
}

} // namespace bz3::server
