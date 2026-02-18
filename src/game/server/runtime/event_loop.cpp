#include "server/runtime/internal.hpp"

#include "server/runtime_event_rules.hpp"

#include "karma/common/logging/logging.hpp"
#include "karma/network/server/session/hooks.hpp"
#include "karma/network/server/session/leave_runtime.hpp"

#include <string>
#include <vector>

namespace bz3::server::runtime_detail {

void RunEventLoop(karma::app::server::Engine* app,
                  ServerGame* game,
                  net::ServerEventSource* event_source,
                  domain::ShotSystem* shot_system,
                  float shot_step_dt,
                  const karma::network::ServerPreAuthConfig& pre_auth_config,
                  const karma::network::ServerJoinWorldState& world_state,
                  karma::network::CommunityHeartbeat* community_heartbeat) {
    if (!app || !game || !event_source || !shot_system || !community_heartbeat) {
        return;
    }

    uint32_t next_global_shot_id = 1;

    karma::network::ServerSessionHooks session_hooks{};
    session_hooks.on_join = [game](const karma::network::ServerPreAuthRequest& request) {
        return karma::network::ServerSessionJoinDecision{
            .accepted = game->onClientJoin(request.client_id, request.player_name),
            .reject_reason = std::string{}};
    };
    session_hooks.has_client = [game](uint32_t client_id) {
        return game->hasClient(client_id);
    };
    session_hooks.on_leave = [game](uint32_t client_id) {
        return game->onClientLeave(client_id);
    };
    session_hooks.last_join_reject_reason = [game]() {
        return game->lastJoinRejectReason();
    };

    RuntimeEventRuleHandlers runtime_handlers{};
    runtime_handlers.has_client = session_hooks.has_client;
    runtime_handlers.on_player_death = [event_source](uint32_t client_id) {
        event_source->onPlayerDeath(client_id);
    };
    runtime_handlers.on_player_spawn = [event_source](uint32_t client_id) {
        event_source->onPlayerSpawn(client_id);
    };
    runtime_handlers.on_create_shot = [event_source, shot_system](uint32_t source_client_id,
                                                                  uint32_t global_shot_id,
                                                                  float pos_x,
                                                                  float pos_y,
                                                                  float pos_z,
                                                                  float vel_x,
                                                                  float vel_y,
                                                                  float vel_z) {
        shot_system->addShot(source_client_id,
                             global_shot_id,
                             pos_x,
                             pos_y,
                             pos_z,
                             vel_x,
                             vel_y,
                             vel_z);
        event_source->onCreateShot(source_client_id,
                                   global_shot_id,
                                   pos_x,
                                   pos_y,
                                   pos_z,
                                   vel_x,
                                   vel_y,
                                   vel_z);
    };

    while (app->isRunning()) {
        for (const auto& event : event_source->poll()) {
            switch (event.type) {
                case net::ServerInputEvent::Type::ClientJoin: {
                    const auto join_decision = karma::network::ResolveServerSessionJoinDecision(
                        pre_auth_config,
                        karma::network::ServerPreAuthRequest{
                            event.join.client_id,
                            event.join.player_name,
                            event.join.auth_payload,
                            event.join.peer_ip,
                            event.join.peer_port},
                        session_hooks);
                    const bool accepted = join_decision.accepted;
                    const std::string reason = join_decision.reject_reason;
                    if (!accepted) {
                        KARMA_TRACE("engine.server",
                                    "Server join rejected client_id={} name='{}' auth_payload_present={} ip={} port={} reason='{}'",
                                    event.join.client_id,
                                    event.join.player_name,
                                    event.join.auth_payload.empty() ? 0 : 1,
                                    event.join.peer_ip,
                                    event.join.peer_port,
                                    reason);
                    }
                    const auto payload = karma::network::BuildServerJoinResultPayload(
                        event.join.client_id,
                        join_decision,
                        world_state,
                        [game]() {
                            std::vector<karma::network::ServerSessionSnapshotEntry> sessions{};
                            for (const auto& session : game->activeSessionSnapshot()) {
                                sessions.push_back(karma::network::ServerSessionSnapshotEntry{
                                    session.session_id,
                                    session.session_name});
                            }
                            return sessions;
                        });
                    karma::network::EmitServerJoinResult(
                        payload,
                        [event_source](const karma::network::ServerJoinResultPayload& emitted) {
                            EmitJoinResultToEventSource(*event_source, emitted);
                        });
                    break;
                }
                case net::ServerInputEvent::Type::ClientLeave: {
                    const auto leave_event_result = karma::network::ApplyServerSessionLeaveEvent(
                        event.leave.client_id,
                        session_hooks,
                        [event_source](uint32_t client_id) {
                            event_source->onPlayerDeath(client_id);
                        });
                    if (leave_event_result != karma::network::ServerSessionLeaveEventResult::Applied) {
                        KARMA_TRACE("net.server",
                                    "RunRuntime: leave {} client_id={}",
                                    karma::network::DescribeServerSessionLeaveEventResult(leave_event_result),
                                    event.leave.client_id);
                    }
                    break;
                }
                case net::ServerInputEvent::Type::ClientRequestSpawn:
                case net::ServerInputEvent::Type::ClientCreateShot: {
                    const auto rule_result =
                        ApplyRuntimeEventRules(event, runtime_handlers, &next_global_shot_id);
                    if (rule_result == RuntimeEventRuleResult::Applied) {
                        break;
                    }

                    if (rule_result == RuntimeEventRuleResult::IgnoredUnknownClient) {
                        if (event.type == net::ServerInputEvent::Type::ClientRequestSpawn) {
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

                    KARMA_TRACE("net.server",
                                "RunRuntime: runtime event rule not applied type={} result={}",
                                static_cast<int>(event.type),
                                static_cast<int>(rule_result));
                    break;
                }
            }
        }

        app->tick();

        const auto expired_shots = shot_system->update(domain::ShotSystem::Clock::now(), shot_step_dt);
        if (!expired_shots.empty()) {
            for (const auto& expired : expired_shots) {
                event_source->onRemoveShot(expired.global_shot_id, true);
            }
            KARMA_TRACE("net.server",
                        "RunRuntime: expired shots removed count={} remaining={}",
                        expired_shots.size(),
                        shot_system->activeShotCount());
        }

        community_heartbeat->update(game->connectedClientCount());
        if (!ShouldKeepRunning()) {
            app->requestStop();
        }
    }
}

} // namespace bz3::server::runtime_detail
