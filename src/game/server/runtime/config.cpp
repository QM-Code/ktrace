#include "server/runtime/internal.hpp"

#include "karma/app/shared/backend_resolution.hpp"
#include "karma/common/config/helpers.hpp"
#include "karma/common/config/store.hpp"
#include "karma/common/logging/logging.hpp"
#include "karma/cli/server/runtime_options.hpp"

#include <algorithm>
#include <string>

#include <spdlog/spdlog.h>

namespace bz3::server::runtime_detail {

void BuildRuntimeConfig(const karma::cli::server::AppOptions& options,
                        std::string_view world_name,
                        karma::app::server::EngineConfig* engine_config,
                        uint16_t* listen_port,
                        karma::network::ServerPreAuthConfig* pre_auth_config,
                        karma::network::CommunityHeartbeat* community_heartbeat) {
    if (!engine_config || !listen_port || !pre_auth_config || !community_heartbeat) {
        return;
    }

    engine_config->target_tick_hz =
        karma::common::config::ReadFloatConfig({"simulation.fixedHz"}, engine_config->target_tick_hz);
    engine_config->max_delta_time =
        karma::common::config::ReadFloatConfig({"simulation.maxFrameDeltaTime"},
                                       engine_config->max_delta_time);
    engine_config->max_substeps =
        static_cast<int>(karma::common::config::ReadUInt16Config({"simulation.maxSubsteps"},
                                                          static_cast<uint16_t>(engine_config->max_substeps)));
    engine_config->physics_backend =
        karma::app::shared::ResolvePhysicsBackendFromOption(options.backend_physics, options.backend_physics_explicit);
    engine_config->audio_backend =
        karma::app::shared::ResolveAudioBackendFromOption(options.backend_audio, options.backend_audio_explicit);
    engine_config->enable_audio = options.backend_audio_explicit
        || karma::common::config::ReadBoolConfig({"audio.serverEnabled"}, false);

    *listen_port = karma::cli::server::ResolveListenPort(options.listen_port,
                                                       options.listen_port_explicit,
                                                       static_cast<uint16_t>(11899));

    *pre_auth_config = karma::network::ReadServerPreAuthConfig();

    const std::string community_override = options.community_explicit ? options.community : std::string{};
    community_heartbeat->configureFromConfig(karma::common::config::ConfigStore::Merged(),
                                             *listen_port,
                                             community_override);
    if (community_heartbeat->enabled()) {
        spdlog::info("Community heartbeat enabled: target='{}' advertise='{}' interval={}s max_players={}",
                     community_heartbeat->communityUrl(),
                     community_heartbeat->serverAddress(),
                     community_heartbeat->intervalSeconds(),
                     community_heartbeat->maxPlayers());
    } else {
        spdlog::info("Community heartbeat disabled: target='{}' advertise='{}' interval={}s",
                     community_heartbeat->communityUrl(),
                     community_heartbeat->serverAddress(),
                     community_heartbeat->intervalSeconds());
    }

    pre_auth_config->community_url = community_heartbeat->communityUrl();
    pre_auth_config->world_name = std::string(world_name);

    KARMA_TRACE("engine.server",
                "Server pre-auth {} (community_auth={})",
                pre_auth_config->required_password.empty() ? "disabled" : "enabled",
                pre_auth_config->community_url.empty() ? "disabled" : "enabled");
}

} // namespace bz3::server::runtime_detail
