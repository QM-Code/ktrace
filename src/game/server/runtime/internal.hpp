#pragma once

#include "server/domain/shot_system.hpp"
#include "server/net/event_source.hpp"
#include "server/server_game.hpp"

#include "karma/app/engine_server_app.hpp"
#include "karma/cli/server_app_options.hpp"
#include "karma/network/community/heartbeat.hpp"
#include "karma/network/server/auth/preauth.hpp"
#include "karma/network/server/session/join_runtime.hpp"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bz3::server::runtime_detail {

void InstallSignalHandlers(std::string app_name);
bool ShouldKeepRunning();

void BuildRuntimeConfig(const karma::cli::ServerAppOptions& options,
                        std::string_view world_name,
                        karma::app::EngineServerConfig* engine_config,
                        uint16_t* listen_port,
                        karma::network::ServerPreAuthConfig* pre_auth_config,
                        karma::network::CommunityHeartbeat* community_heartbeat);

std::vector<net::SessionSnapshotEntry> ToNetSessionSnapshot(
    const std::vector<karma::network::ServerSessionSnapshotEntry>& sessions);
std::vector<net::WorldManifestEntry> ToNetWorldManifest(
    const std::vector<karma::network::ServerWorldManifestEntry>& manifest);
void EmitJoinResultToEventSource(net::ServerEventSource& event_source,
                                 const karma::network::ServerJoinResultPayload& payload);

void RunEventLoop(karma::app::EngineServerApp* app,
                  ServerGame* game,
                  net::ServerEventSource* event_source,
                  domain::ShotSystem* shot_system,
                  float shot_step_dt,
                  const karma::network::ServerPreAuthConfig& pre_auth_config,
                  const karma::network::ServerJoinWorldState& world_state,
                  karma::network::CommunityHeartbeat* community_heartbeat);

} // namespace bz3::server::runtime_detail
