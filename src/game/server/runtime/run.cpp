#include "server/runtime.hpp"

#include "server/domain/world_session.hpp"
#include "server/net/event_source.hpp"
#include "server/runtime/internal.hpp"
#include "server/server_game.hpp"

#include "karma/app/bootstrap_scaffold.hpp"
#include "karma/app/engine_server_app.hpp"
#include "karma/common/config_helpers.hpp"
#include "karma/common/config_validation.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace bz3::server {

int RunRuntime(const karma::cli::ServerAppOptions& options) {
    const auto issues = karma::config::ValidateRequiredKeys(karma::config::ServerRequiredKeys());
    if (!karma::app::ReportRequiredConfigIssues(issues, options.strict_config)) {
        return 1;
    }

    const auto world_context = domain::LoadWorldSessionContext(options);
    if (!world_context.has_value()) {
        return 1;
    }

    runtime_detail::InstallSignalHandlers(options.app_name.empty() ? std::string("server") : options.app_name);
    karma::app::EngineServerConfig engine_config{};
    uint16_t listen_port = 0;
    karma::network::ServerPreAuthConfig pre_auth_config{};
    karma::network::CommunityHeartbeat community_heartbeat{};
    runtime_detail::BuildRuntimeConfig(options,
                                       world_context->world_name,
                                       &engine_config,
                                       &listen_port,
                                       &pre_auth_config,
                                       &community_heartbeat);

    ServerGame game{world_context->world_name};
    std::unique_ptr<net::ServerEventSource> event_source =
        net::CreateServerEventSource(options, listen_port);

    domain::ShotSystem shot_system{};
    const float shot_lifetime_seconds =
        std::max(0.1f, karma::config::ReadFloatConfig({"gameplay.shotLifetimeSeconds"}, 5.0f));
    shot_system.setLifetime(std::chrono::milliseconds(static_cast<int64_t>(shot_lifetime_seconds * 1000.0f)));
    const float shot_step_dt =
        (engine_config.target_tick_hz > 1e-6f)
            ? (1.0f / engine_config.target_tick_hz)
            : (1.0f / 60.0f);

    karma::app::EngineServerApp app{};
    app.start(game, engine_config);

    std::vector<karma::network::ServerWorldManifestEntry> world_manifest{};
    world_manifest.reserve(world_context->world_manifest.size());
    for (const auto& entry : world_context->world_manifest) {
        world_manifest.push_back(karma::network::ServerWorldManifestEntry{
            .path = entry.path,
            .size = entry.size,
            .hash = entry.hash});
    }

    static const std::vector<std::byte> empty_world_package{};
    const auto* world_package = world_context->world_package_enabled
        ? &world_context->world_package
        : &empty_world_package;
    const karma::network::ServerJoinWorldState world_state{
        .world_name = world_context->world_name,
        .world_id = world_context->world_id,
        .world_revision = world_context->world_revision,
        .world_package_hash = world_context->world_package_hash,
        .world_content_hash = world_context->world_content_hash,
        .world_manifest_hash = world_context->world_manifest_hash,
        .world_manifest_file_count = world_context->world_manifest_file_count,
        .world_package_size = world_context->world_package_size,
        .world_dir = world_context->world_dir,
        .world_manifest = &world_manifest,
        .world_package = world_package};

    runtime_detail::RunEventLoop(&app,
                                 &game,
                                 event_source.get(),
                                 &shot_system,
                                 shot_step_dt,
                                 pre_auth_config,
                                 world_state,
                                 &community_heartbeat);

    return 0;
}

} // namespace bz3::server
