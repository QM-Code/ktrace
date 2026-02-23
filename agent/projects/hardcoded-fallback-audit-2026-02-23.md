# Hardcoded Fallback Audit (m-karma/src + m-bz3/src)

- Generated: 2026-02-23
- Scope: runtime/source code under `m-karma/src/` and `m-bz3/src/` (excluding helper declarations/definitions in `m-karma/src/common/config/helpers.*` for callsite counting).
- Direct fallback config callsites found: **151** (`ReadBoolConfig|ReadUInt16Config|ReadFloatConfig|ReadStringConfig` with explicit default arguments).

## 1) Direct `Read*Config(..., default)` Callsite Inventory

### `m-bz3/src/client/game/lifecycle.cpp` (1 callsites)

- `m-bz3/src/client/game/lifecycle.cpp:29` `ReadBoolConfig({"gameplay.tank.enabled"}, false)`

### `m-bz3/src/client/game/tank_collision_step_stats.cpp` (2 callsites)

- `m-bz3/src/client/game/tank_collision_step_stats.cpp:45` `ReadBoolConfig({"gameplay.tank.collisionTrace.enabled"}, false)`
- `m-bz3/src/client/game/tank_collision_step_stats.cpp:47` `ReadFloatConfig({"gameplay.tank.collisionTrace.logEverySteps"}, 120.0f)`

### `m-bz3/src/client/game/tank_entity.cpp` (24 callsites)

- `m-bz3/src/client/game/tank_entity.cpp:62` `ReadFloatConfig({"gameplay.tank.forwardSpeed"}, 8.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:64` `ReadFloatConfig({"gameplay.tank.reverseSpeed"}, 5.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:66` `ReadFloatConfig({"gameplay.tank.turnSpeed"}, 2.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:68` `ReadFloatConfig({"gameplay.tank.jumpImpulse"}, 8.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:70` `ReadFloatConfig({"gameplay.tank.gravity"}, -20.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:72` `ReadFloatConfig({"gameplay.tank.maxFallSpeed"}, 40.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:74` `ReadFloatConfig({"gameplay.tank.landingEpsilon"}, 1e-3f)`
- `m-bz3/src/client/game/tank_entity.cpp:79` `ReadFloatConfig({"gameplay.tank.startX"}, 0.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:80` `ReadFloatConfig({"gameplay.tank.startY"}, 0.6f)`
- `m-bz3/src/client/game/tank_entity.cpp:81` `ReadFloatConfig({"gameplay.tank.startZ"}, 0.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:83` `ReadFloatConfig({"gameplay.tank.startYawDegrees"}, 0.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:90` `ReadFloatConfig({"gameplay.tank.modelScale"}, 1.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:92` `ReadFloatConfig({"gameplay.tank.modelYawOffsetDegrees"}, 0.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:95` `ReadFloatConfig({"gameplay.tank.cameraYawOffsetDegrees"}, 0.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:99` `ReadStringConfig("gameplay.tank.cameraMode", std::string("fps"))`
- `m-bz3/src/client/game/tank_entity.cpp:110` `ReadFloatConfig({"gameplay.tank.cameraHeight"}, (tank_camera_mode_ == TankCameraMode::Fps) ? 1.2f : 4.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:115` `ReadFloatConfig({"gameplay.tank.cameraDistance"}, (tank_camera_mode_ == TankCameraMode::Fps) ? 0.0f : 10.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:119` `ReadFloatConfig({"gameplay.tank.cameraForwardOffset"}, 0.7f)`
- `m-bz3/src/client/game/tank_entity.cpp:121` `ReadFloatConfig({"gameplay.tank.cameraLookAhead"}, 8.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:123` `ReadFloatConfig({"gameplay.tank.cameraSmoothRate"}, 18.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:125` `ReadFloatConfig({"gameplay.tank.visualSmoothRate"}, 22.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:127` `ReadFloatConfig({"gameplay.tank.simMaxStepSeconds"}, 1.0f / 120.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:131` `ReadFloatConfig({"gameplay.tank.collisionRadius"}, 1.0f)`
- `m-bz3/src/client/game/tank_entity.cpp:132` `ReadBoolConfig({"gameplay.tank.physicsAuthorityPilot.enabled"}, false)`

### `m-bz3/src/client/game/tank_motion_authority_state_machine.cpp` (4 callsites)

- `m-bz3/src/client/game/tank_motion_authority_state_machine.cpp:86` `ReadFloatConfig({"gameplay.tank.physicsAuthorityPilot.min_ready_steps"}, static_cast<float>(config.min_ready_steps))`
- `m-bz3/src/client/game/tank_motion_authority_state_machine.cpp:92` `ReadFloatConfig({"gameplay.tank.physicsAuthorityPilot.fallback_cooldown_steps"}, static_cast<float>(config.fallback_cooldown_steps))`
- `m-bz3/src/client/game/tank_motion_authority_state_machine.cpp:101` `ReadBoolConfig({"gameplay.tank.physicsAuthorityPilot.trace.enabled"}, false)`
- `m-bz3/src/client/game/tank_motion_authority_state_machine.cpp:102` `ReadFloatConfig({"gameplay.tank.physicsAuthorityPilot.trace.log_every_steps"}, static_cast<float>(config.trace_log_every_steps))`

### `m-bz3/src/client/net/connection/core.cpp` (1 callsites)

- `m-bz3/src/client/net/connection/core.cpp:74` `ReadUInt16Config({"network.ConnectTimeoutMs"}, static_cast<uint16_t>(2000))`

### `m-bz3/src/client/runtime/engine_config.cpp` (29 callsites)

- `m-bz3/src/client/runtime/engine_config.cpp:51` `ReadFloatConfig({"roamingMode.graphics.lighting.unlit"}, 0.0f)`
- `m-bz3/src/client/runtime/engine_config.cpp:52` `ReadBoolConfig({"roamingMode.graphics.lighting.shadows.enabled"}, config.default_light.shadow.enabled)`
- `m-bz3/src/client/runtime/engine_config.cpp:55` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.strength"}, config.default_light.shadow.strength)`
- `m-bz3/src/client/runtime/engine_config.cpp:58` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.bias"}, config.default_light.shadow.bias)`
- `m-bz3/src/client/runtime/engine_config.cpp:61` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.receiverBiasScale"}, config.default_light.shadow.receiver_bias_scale)`
- `m-bz3/src/client/runtime/engine_config.cpp:64` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.normalBiasScale"}, config.default_light.shadow.normal_bias_scale)`
- `m-bz3/src/client/runtime/engine_config.cpp:67` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.rasterDepthBias"}, config.default_light.shadow.raster_depth_bias)`
- `m-bz3/src/client/runtime/engine_config.cpp:70` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.rasterSlopeBias"}, config.default_light.shadow.raster_slope_bias)`
- `m-bz3/src/client/runtime/engine_config.cpp:73` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.extent"}, config.default_light.shadow.extent)`
- `m-bz3/src/client/runtime/engine_config.cpp:76` `ReadUInt16Config({"roamingMode.graphics.lighting.shadows.mapSize"}, static_cast<uint16_t>(config.default_light.shadow.map_size))`
- `m-bz3/src/client/runtime/engine_config.cpp:79` `ReadUInt16Config({"roamingMode.graphics.lighting.shadows.pcfRadius"}, static_cast<uint16_t>(config.default_light.shadow.pcf_radius))`
- `m-bz3/src/client/runtime/engine_config.cpp:82` `ReadUInt16Config({"roamingMode.graphics.lighting.shadows.triangleBudget"}, static_cast<uint16_t>(config.default_light.shadow.triangle_budget))`
- `m-bz3/src/client/runtime/engine_config.cpp:85` `ReadUInt16Config({"roamingMode.graphics.lighting.shadows.updateEveryFrames"}, static_cast<uint16_t>(config.default_light.shadow.update_every_frames))`
- `m-bz3/src/client/runtime/engine_config.cpp:88` `ReadUInt16Config({"roamingMode.graphics.lighting.shadows.pointMapSize"}, static_cast<uint16_t>(config.default_light.shadow.point_map_size))`
- `m-bz3/src/client/runtime/engine_config.cpp:91` `ReadUInt16Config({"roamingMode.graphics.lighting.shadows.pointMaxShadowLights"}, static_cast<uint16_t>(config.default_light.shadow.point_max_shadow_lights))`
- `m-bz3/src/client/runtime/engine_config.cpp:94` `ReadUInt16Config({"roamingMode.graphics.lighting.shadows.pointFacesPerFrameBudget"}, static_cast<uint16_t>(config.default_light.shadow.point_faces_per_frame_budget))`
- `m-bz3/src/client/runtime/engine_config.cpp:97` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.pointConstantBias"}, config.default_light.shadow.point_constant_bias)`
- `m-bz3/src/client/runtime/engine_config.cpp:100` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.pointSlopeBiasScale"}, config.default_light.shadow.point_slope_bias_scale)`
- `m-bz3/src/client/runtime/engine_config.cpp:103` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.pointNormalBiasScale"}, config.default_light.shadow.point_normal_bias_scale)`
- `m-bz3/src/client/runtime/engine_config.cpp:106` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.pointReceiverBiasScale"}, config.default_light.shadow.point_receiver_bias_scale)`
- `m-bz3/src/client/runtime/engine_config.cpp:109` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.localLightDistanceDamping"}, config.default_light.shadow.local_light_distance_damping)`
- `m-bz3/src/client/runtime/engine_config.cpp:112` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.localLightRangeFalloffExponent"}, config.default_light.shadow.local_light_range_falloff_exponent)`
- `m-bz3/src/client/runtime/engine_config.cpp:115` `ReadBoolConfig({"roamingMode.graphics.lighting.shadows.aoAffectsLocalLights"}, config.default_light.shadow.ao_affects_local_lights)`
- `m-bz3/src/client/runtime/engine_config.cpp:118` `ReadFloatConfig({"roamingMode.graphics.lighting.shadows.localLightDirectionalShadowLiftStrength"}, config.default_light.shadow.local_light_directional_shadow_lift_strength)`
- `m-bz3/src/client/runtime/engine_config.cpp:121` `ReadStringConfig({"roamingMode.graphics.lighting.shadows.executionMode"}, karma::renderer::DirectionalLightData::ShadowExecutionModeToken(             config.default_light.shadow.execution_mode))`
- `m-bz3/src/client/runtime/engine_config.cpp:145` `ReadBoolConfig({"audio.enabled"}, true)`
- `m-bz3/src/client/runtime/engine_config.cpp:146` `ReadFloatConfig({"simulation.fixedHz"}, 60.0f)`
- `m-bz3/src/client/runtime/engine_config.cpp:147` `ReadFloatConfig({"simulation.maxFrameDeltaTime"}, 0.25f)`
- `m-bz3/src/client/runtime/engine_config.cpp:149` `ReadUInt16Config({"simulation.maxSubsteps"}, 4)`

### `m-bz3/src/client/runtime/startup_options.cpp` (1 callsites)

- `m-bz3/src/client/runtime/startup_options.cpp:27` `ReadStringConfig("userDefaults.username", "Player")`

### `m-bz3/src/server/domain/world_session.cpp` (4 callsites)

- `m-bz3/src/server/domain/world_session.cpp:73` `ReadStringConfig("worldName", default_world_name)`
- `m-bz3/src/server/domain/world_session.cpp:117` `ReadStringConfig("worldId", context.world_name)`
- `m-bz3/src/server/domain/world_session.cpp:122` `ReadStringConfig("worldRevision", default_revision)`
- `m-bz3/src/server/domain/world_session.cpp:125` `ReadStringConfig("worldRevision", "bundled")`

### `m-bz3/src/server/runtime/config.cpp` (4 callsites)

- `m-bz3/src/server/runtime/config.cpp:28` `ReadFloatConfig({"simulation.fixedHz"}, engine_config->target_tick_hz)`
- `m-bz3/src/server/runtime/config.cpp:30` `ReadFloatConfig({"simulation.maxFrameDeltaTime"}, engine_config->max_delta_time)`
- `m-bz3/src/server/runtime/config.cpp:33` `ReadUInt16Config({"simulation.maxSubsteps"}, static_cast<uint16_t>(engine_config->max_substeps))`
- `m-bz3/src/server/runtime/config.cpp:40` `ReadBoolConfig({"audio.serverEnabled"}, false)`

### `m-bz3/src/server/runtime/run.cpp` (30 callsites)

- `m-bz3/src/server/runtime/run.cpp:62` `ReadFloatConfig({"gameplay.shotLifetimeSeconds"}, 5.0f)`
- `m-bz3/src/server/runtime/run.cpp:69` `ReadBoolConfig({"gameplay.shots.physicsHitPilot.enabled"}, false)`
- `m-bz3/src/server/runtime/run.cpp:74` `ReadFloatConfig({"gameplay.shots.physicsHitPilot.epsilon"}, 1e-3f)`
- `m-bz3/src/server/runtime/run.cpp:77` `ReadFloatConfig({"gameplay.shots.hitDamage"}, 25.0f)`
- `m-bz3/src/server/runtime/run.cpp:80` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.maxRecastIterations"}, 8u)`
- `m-bz3/src/server/runtime/run.cpp:82` `ReadBoolConfig({"gameplay.shots.ricochet.enabled"}, true)`
- `m-bz3/src/server/runtime/run.cpp:87` `ReadUInt16Config({"gameplay.shots.ricochet.maxBounces"}, 1u)`
- `m-bz3/src/server/runtime/run.cpp:90` `ReadFloatConfig({"gameplay.shots.ricochet.normalEpsilon"}, 1e-4f)`
- `m-bz3/src/server/runtime/run.cpp:93` `ReadFloatConfig({"gameplay.shots.ricochet.positionBias"}, 1e-3f)`
- `m-bz3/src/server/runtime/run.cpp:96` `ReadFloatConfig({"gameplay.shots.ricochet.minSpeed"}, 1e-3f)`
- `m-bz3/src/server/runtime/run.cpp:97` `ReadBoolConfig({"gameplay.shots.physicsHitPilot.ignoreTriggers"}, true)`
- `m-bz3/src/server/runtime/run.cpp:100` `ReadBoolConfig({"gameplay.shots.physicsHitPilot.trace.enabled"}, false)`
- `m-bz3/src/server/runtime/run.cpp:105` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.trace.logEverySteps"}, 120u)`
- `m-bz3/src/server/runtime/run.cpp:107` `ReadBoolConfig({"gameplay.shots.physicsHitPilot.guard.enabled"}, false)`
- `m-bz3/src/server/runtime/run.cpp:112` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.guard.windowSteps"}, 180u)`
- `m-bz3/src/server/runtime/run.cpp:116` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.guard.minPhysicsHitsPerWindow"}, 8u)`
- `m-bz3/src/server/runtime/run.cpp:120` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.guard.minQueryCallsPerWindow"}, 16u)`
- `m-bz3/src/server/runtime/run.cpp:123` `ReadFloatConfig({"gameplay.shots.physicsHitPilot.guard.maxConservativeRatio"}, 0.20f)`
- `m-bz3/src/server/runtime/run.cpp:128` `ReadFloatConfig({"gameplay.shots.physicsHitPilot.guard.maxAmbiguousRatio"}, 0.05f)`
- `m-bz3/src/server/runtime/run.cpp:133` `ReadFloatConfig({"gameplay.shots.physicsHitPilot.guard.maxNonActorRatio"}, 0.80f)`
- `m-bz3/src/server/runtime/run.cpp:138` `ReadFloatConfig({"gameplay.shots.physicsHitPilot.guard.maxInvalidQueryRatio"}, 0.05f)`
- `m-bz3/src/server/runtime/run.cpp:144` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.guard.suppressCooldownSteps"}, 300u)`
- `m-bz3/src/server/runtime/run.cpp:148` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.guard.reactivateWarmupSteps"}, 60u)`
- `m-bz3/src/server/runtime/run.cpp:150` `ReadBoolConfig({"gameplay.shots.physicsHitPilot.recastAutoTune.enabled"}, false)`
- `m-bz3/src/server/runtime/run.cpp:155` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.recastAutoTune.minIterations"}, 2u)`
- `m-bz3/src/server/runtime/run.cpp:159` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.recastAutoTune.maxIterations"}, 12u)`
- `m-bz3/src/server/runtime/run.cpp:163` `ReadFloatConfig({"gameplay.shots.physicsHitPilot.recastAutoTune.increaseOnConservativeRatio"}, 0.15f)`
- `m-bz3/src/server/runtime/run.cpp:169` `ReadFloatConfig({"gameplay.shots.physicsHitPilot.recastAutoTune.decreaseOnConservativeRatio"}, 0.02f)`
- `m-bz3/src/server/runtime/run.cpp:177` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.recastAutoTune.decreaseRequiresHealthyWindows"}, 3u)`
- `m-bz3/src/server/runtime/run.cpp:182` `ReadUInt16Config({"gameplay.shots.physicsHitPilot.recastAutoTune.adjustmentCooldownSteps"}, 120u)`

### `m-bz3/src/server/runtime/server_game.cpp` (5 callsites)

- `m-bz3/src/server/runtime/server_game.cpp:867` `ReadFloatConfig({"playerParameters.jumpSpeed"}, kDefaultJumpImpulse)`
- `m-bz3/src/server/runtime/server_game.cpp:868` `ReadFloatConfig({"playerParameters.gravity"}, kDefaultGravity)`
- `m-bz3/src/server/runtime/server_game.cpp:877` `ReadFloatConfig({"gameplay.tank.maxFallSpeed"}, kDefaultMaxFallSpeed)`
- `m-bz3/src/server/runtime/server_game.cpp:879` `ReadFloatConfig({"gameplay.tank.authoritativeInputStepSeconds"}, kDefaultInputStepSeconds)`
- `m-bz3/src/server/runtime/server_game.cpp:886` `ReadFloatConfig({"gameplay.tank.groundedEpsilon"}, kDefaultGroundedEpsilon)`

### `m-karma/src/app/client/backend_resolution.cpp` (1 callsites)

- `m-karma/src/app/client/backend_resolution.cpp:17` `ReadStringConfig("render.backend", "auto")`

### `m-karma/src/app/shared/backend_resolution.cpp` (2 callsites)

- `m-karma/src/app/shared/backend_resolution.cpp:14` `ReadStringConfig("physics.backend", "auto")`
- `m-karma/src/app/shared/backend_resolution.cpp:40` `ReadStringConfig("audio.backend", "auto")`

### `m-karma/src/app/shared/bootstrap.cpp` (1 callsites)

- `m-karma/src/app/shared/bootstrap.cpp:94` `ReadStringConfig("app.name", fallback)`

### `m-karma/src/demo/client/runtime.cpp` (4 callsites)

- `m-karma/src/demo/client/runtime.cpp:35` `ReadStringConfig(path, std::string{})`
- `m-karma/src/demo/client/runtime.cpp:209` `ReadUInt16Config({"demo.runtime.channelCount"}, static_cast<uint16_t>(transport_config.channel_count))`
- `m-karma/src/demo/client/runtime.cpp:221` `ReadUInt16Config({"demo.runtime.clientConnectTimeoutMs", "network.ClientConnectTimeoutMs"}, 2000)`
- `m-karma/src/demo/client/runtime.cpp:248` `ReadUInt16Config({"demo.runtime.clientPollTimeoutMs"}, 20)`

### `m-karma/src/demo/server/runtime.cpp` (4 callsites)

- `m-karma/src/demo/server/runtime.cpp:44` `ReadStringConfig(path, std::string{})`
- `m-karma/src/demo/server/runtime.cpp:143` `ReadUInt16Config({"demo.runtime.maxClients", "maxPlayers"}, 32)`
- `m-karma/src/demo/server/runtime.cpp:146` `ReadUInt16Config({"demo.runtime.channelCount"}, 2)`
- `m-karma/src/demo/server/runtime.cpp:214` `ReadUInt16Config({"demo.runtime.serverPollTimeoutMs"}, 20)`

### `m-karma/src/network/community/heartbeat.cpp` (1 callsites)

- `m-karma/src/network/community/heartbeat.cpp:30` `ReadStringConfig("community.advertise", "")`

### `m-karma/src/network/config/reconnect_policy.cpp` (4 callsites)

- `m-karma/src/network/config/reconnect_policy.cpp:9` `ReadUInt16Config({"network.ClientReconnectMaxAttempts", "network.ReconnectMaxAttempts"}, static_cast<uint16_t>(0))`
- `m-karma/src/network/config/reconnect_policy.cpp:12` `ReadUInt16Config({"network.ClientReconnectBackoffInitialMs", "network.ReconnectBackoffInitialMs"}, static_cast<uint16_t>(250))`
- `m-karma/src/network/config/reconnect_policy.cpp:15` `ReadUInt16Config({"network.ClientReconnectBackoffMaxMs", "network.ReconnectBackoffMaxMs"}, static_cast<uint16_t>(2000))`
- `m-karma/src/network/config/reconnect_policy.cpp:18` `ReadUInt16Config({"network.ClientReconnectTimeoutMs", "network.ReconnectTimeoutMs"}, static_cast<uint16_t>(1000))`

### `m-karma/src/network/config/transport_mapping.cpp` (2 callsites)

- `m-karma/src/network/config/transport_mapping.cpp:10` `ReadStringConfig({"network.ClientTransportBackend"}, std::string("auto"))`
- `m-karma/src/network/config/transport_mapping.cpp:34` `ReadStringConfig({"network.ServerTransportBackend"}, std::string("auto"))`

### `m-karma/src/network/content/transfer_sender.cpp` (2 callsites)

- `m-karma/src/network/content/transfer_sender.cpp:16` `ReadUInt16Config({"network.WorldTransferChunkBytes"}, default_chunk_size)`
- `m-karma/src/network/content/transfer_sender.cpp:19` `ReadUInt16Config({"network.WorldTransferRetryAttempts"}, default_retry_attempts)`

### `m-karma/src/network/server/auth/preauth.cpp` (2 callsites)

- `m-karma/src/network/server/auth/preauth.cpp:268` `ReadStringConfig({"network.PreAuthPassword"}, std::string{})`
- `m-karma/src/network/server/auth/preauth.cpp:269` `ReadStringConfig({"network.PreAuthRejectReason"}, std::string("Authentication failed."))`

### `m-karma/src/ui/backends/imgui/backend.cpp` (2 callsites)

- `m-karma/src/ui/backends/imgui/backend.cpp:37` `ReadUInt16Config({"ui.imgui.SoftwareBridge.Width"}, 1024u)`
- `m-karma/src/ui/backends/imgui/backend.cpp:38` `ReadUInt16Config({"ui.imgui.SoftwareBridge.Height"}, 576u)`

### `m-karma/src/ui/backends/rmlui/adapter.cpp` (6 callsites)

- `m-karma/src/ui/backends/rmlui/adapter.cpp:28` `ReadUInt16Config({"ui.rmlui.SoftwareBridge.Width"}, 1024u)`
- `m-karma/src/ui/backends/rmlui/adapter.cpp:30` `ReadUInt16Config({"ui.rmlui.SoftwareBridge.Height"}, 576u)`
- `m-karma/src/ui/backends/rmlui/adapter.cpp:31` `ReadFloatConfig({"ui.rmlui.SoftwareBridge.Distance"}, 0.75f)`
- `m-karma/src/ui/backends/rmlui/adapter.cpp:32` `ReadFloatConfig({"ui.rmlui.SoftwareBridge.WidthMeters"}, 0.95f)`
- `m-karma/src/ui/backends/rmlui/adapter.cpp:33` `ReadFloatConfig({"ui.rmlui.SoftwareBridge.HeightMeters"}, 0.95f)`
- `m-karma/src/ui/backends/rmlui/adapter.cpp:34` `ReadBoolConfig({"ui.rmlui.SoftwareBridge.AllowFallback"}, false)`

### `m-karma/src/ui/backends/rmlui/stub.cpp` (5 callsites)

- `m-karma/src/ui/backends/rmlui/stub.cpp:39` `ReadBoolConfig({"ui.rmlui.stub.DebugOverlay"}, false)`
- `m-karma/src/ui/backends/rmlui/stub.cpp:40` `ReadBoolConfig({"ui.rmlui.stub.AllowFallback"}, false)`
- `m-karma/src/ui/backends/rmlui/stub.cpp:41` `ReadFloatConfig({"ui.rmlui.stub.Distance"}, 0.75f)`
- `m-karma/src/ui/backends/rmlui/stub.cpp:42` `ReadFloatConfig({"ui.rmlui.stub.Width"}, 1.1f)`
- `m-karma/src/ui/backends/rmlui/stub.cpp:43` `ReadFloatConfig({"ui.rmlui.stub.Height"}, 0.62f)`

### `m-karma/src/ui/backends/software/backend.cpp` (4 callsites)

- `m-karma/src/ui/backends/software/backend.cpp:48` `ReadBoolConfig({"ui.overlayTest.Enabled"}, true)`
- `m-karma/src/ui/backends/software/backend.cpp:49` `ReadFloatConfig({"ui.overlayTest.Distance"}, 0.75f)`
- `m-karma/src/ui/backends/software/backend.cpp:50` `ReadFloatConfig({"ui.overlayTest.Width"}, 1.2f)`
- `m-karma/src/ui/backends/software/backend.cpp:51` `ReadFloatConfig({"ui.overlayTest.Height"}, 0.7f)`

### `m-karma/src/ui/system.cpp` (6 callsites)

- `m-karma/src/ui/system.cpp:72` `ReadStringConfig("ui.backend", current_name)`
- `m-karma/src/ui/system.cpp:233` `ReadBoolConfig({"ui.captureInput"}, false)`
- `m-karma/src/ui/system.cpp:234` `ReadBoolConfig({"ui.overlayFallback.Enabled"}, true)`
- `m-karma/src/ui/system.cpp:235` `ReadFloatConfig({"ui.overlayFallback.Distance", "ui.overlayTest.Distance"}, 0.75f)`
- `m-karma/src/ui/system.cpp:236` `ReadFloatConfig({"ui.overlayFallback.Width", "ui.overlayTest.Width"}, 1.2f)`
- `m-karma/src/ui/system.cpp:237` `ReadFloatConfig({"ui.overlayFallback.Height", "ui.overlayTest.Height"}, 0.7f)`

## 2) Additional Hardcoded Fallback Patterns (Non-Read*Config)

- `m-karma/src/network/server/auth/preauth.cpp:232` response_json.value("ok", false) JSON fallback default
- `m-karma/src/network/server/auth/preauth.cpp:233` response_json.value("error", std::string{}) JSON fallback default
- `m-karma/src/ui/backends/rmlui/adapter.cpp:51` ResolveConfiguredAsset(..., "client/fonts/GoogleSans.ttf") hardcoded asset fallback path
- `m-bz3/src/ui/frontends/imgui/console/console.hpp:191` serverPortInput = 11899 hardcoded local-server UI default
- `m-bz3/src/ui/frontends/rmlui/console/panels/panel_start_server.hpp:117` serverPortValue = 11899 hardcoded local-server UI default
- `m-karma/src/common/config/store.cpp:282` readIntervalSeconds(..., 0.0) hardcoded fallback for SaveIntervalSeconds
- `m-karma/src/common/config/store.cpp:283` readIntervalSeconds(..., 0.0) hardcoded fallback for MergeIntervalSeconds
- `m-karma/src/common/config/store.cpp:286` saveIntervalSeconds = 0.0 when user config disabled
- `m-karma/src/common/config/store.cpp:287` mergeIntervalSeconds = 0.0 when user config disabled

## 3) Risk Flags for Mandatory-JSON Migration

- `m-bz3/src/client/runtime/engine_config.cpp` Many fallback reads are consumed during startup; converting to required reads without adding keys to validation will hard-fail startup. Not a segfault risk, but a process-terminating runtime_error path.
- `m-bz3/src/server/runtime/run.cpp` 30 fallback reads in runtime init. Without a pre-validation table (ServerRequiredKeys is currently empty), missing keys will throw during runtime startup. Hard fail is desired, but error quality/order may degrade.
- `m-bz3/src/server/runtime/config.cpp` Uses engine_config field values as fallback defaults. Replacing with required keys requires explicit JSON contract for simulation.* and audio.serverEnabled.
- `m-bz3/src/server/domain/world_session.cpp` worldName/worldId/worldRevision currently derive from package metadata fallback. Making all mandatory requires generated overlays to populate them to avoid startup failure.
- `m-karma/src/network/server/auth/preauth.cpp` reject_reason fallback protects against empty reject strings. Mandatory config conversion must preserve non-empty invariant or join rejection UX regresses.
- `m-karma/src/ui/backends/imgui/backend.cpp` Software bridge dimensions have clamp guards; removing defaults without required-key validation can throw before UI init.
- `m-karma/src/ui/backends/rmlui/adapter.cpp` Uses hardcoded default font path and bridge dimensions. Removing defaults needs required asset key + dimension keys; otherwise UI backend init can fail early.
- `m-bz3/src/ui/frontends/imgui/console/console.hpp` Hardcoded default local-server port (11899) is not config-driven. Converting to mandatory JSON requires UI config input + validation.
- `m-bz3/src/ui/frontends/rmlui/console/panels/panel_start_server.hpp` Hardcoded default local-server port (11899) is not config-driven. Converting to mandatory JSON requires UI config input + validation.

## 4) Migration Plan (Hard-Fail / Mandatory JSON)

1. Define mandatory key contracts in one place.
   - Expand `ClientRequiredKeys()` and `ServerRequiredKeys()` in `m-karma/src/common/config/validation.cpp` to include every key currently read through fallback APIs.
   - Split contracts by runtime surface (client, server, demo-client, demo-server, UI console) so failures are scoped and actionable.
2. Add preflight validation before runtime initialization.
   - Keep startup behavior as hard-fail, but fail with aggregated validation issues before any subsystem starts.
   - Ensure both `m-karma` and `m-bz3` entrypoints run validation (client + server).
3. Replace fallback reads with required reads in phases.
   - Phase A: keys that already have stable JSON defaults in checked-in configs.
   - Phase B: keys currently falling back to in-memory struct defaults (e.g., shadow settings, simulation settings). Add keys to JSON first, then switch to `ReadRequired*Config`.
   - Phase C: dynamic fallback keys (worldName/worldId/worldRevision, app.name, backend auto selectors) with explicit policy decisions (mandatory vs computed runtime defaults).
4. Preserve post-read safety guards.
   - Keep `std::max`, `std::clamp`, finite checks, and normalization after converting to required reads; mandatory presence does not guarantee valid ranges.
5. Eliminate non-config hardcoded defaults.
   - Move UI local-server default port (`11899`) into mandatory config key(s).
   - Replace JSON `.value(..., default)` in pre-auth with explicit required parsing/validation or hard-fail response parsing policy.
6. Add migration tests and config fixture updates.
   - Update `data/client/config.json` and `data/server/config.json` templates with all mandatory keys.
   - Add tests that verify startup fails with clear errors when any mandatory key is missing or invalid.
7. Rollout with compatibility gate (optional short transition).
   - Temporary mode: warn on fallback path usage with a strict toggle that turns warnings into hard failures.
   - Remove compatibility mode once all shipped configs satisfy mandatory contracts.

## 5) Multi-Candidate Key Fallback Tracker (`Read*Config({"new", "legacy"}, default)`)

- Objective: remove legacy alias keys and keep one canonical key per setting.
- Current inventory: 9 callsites (all in `m-karma/src`; none in `m-bz3/src`).
- Assumption for migration planning: first key is canonical target, second key is legacy alias pending confirmation.
- Status (`2026-02-23`): completed in source.
  - Replaced all 9 multi-candidate `Read*Config({"new","legacy"}, default)` callsites with single-key required reads.
  - Canonical keys now in use:
    - `client.network.reconnect.maxAttempts`
    - `client.network.reconnect.backoffInitialMs`
    - `client.network.reconnect.backoffMaxMs`
    - `client.network.reconnect.timeoutMs`
    - `client.network.connect.timeoutMs`
    - `client.ui.overlay.distance`
    - `client.ui.overlay.width`
    - `client.ui.overlay.height`
    - `server.clients.max`

### `m-karma/src/network/config/reconnect_policy.cpp` (4 alias callsites)

- `m-karma/src/network/config/reconnect_policy.cpp:9` `ReadUInt16Config({"network.ClientReconnectMaxAttempts", "network.ReconnectMaxAttempts"}, static_cast<uint16_t>(0))`
- `m-karma/src/network/config/reconnect_policy.cpp:12` `ReadUInt16Config({"network.ClientReconnectBackoffInitialMs", "network.ReconnectBackoffInitialMs"}, static_cast<uint16_t>(250))`
- `m-karma/src/network/config/reconnect_policy.cpp:15` `ReadUInt16Config({"network.ClientReconnectBackoffMaxMs", "network.ReconnectBackoffMaxMs"}, static_cast<uint16_t>(2000))`
- `m-karma/src/network/config/reconnect_policy.cpp:18` `ReadUInt16Config({"network.ClientReconnectTimeoutMs", "network.ReconnectTimeoutMs"}, static_cast<uint16_t>(1000))`

### `m-karma/src/demo/client/runtime.cpp` (1 alias callsite)

- `m-karma/src/demo/client/runtime.cpp:221` `ReadUInt16Config({"demo.runtime.clientConnectTimeoutMs", "network.ClientConnectTimeoutMs"}, 2000)`

### `m-karma/src/demo/server/runtime.cpp` (1 alias callsite)

- `m-karma/src/demo/server/runtime.cpp:143` `ReadUInt16Config({"demo.runtime.maxClients", "maxPlayers"}, 32)`

### `m-karma/src/ui/system.cpp` (3 alias callsites)

- `m-karma/src/ui/system.cpp:235` `ReadFloatConfig({"ui.overlayFallback.Distance", "ui.overlayTest.Distance"}, 0.75f)`
- `m-karma/src/ui/system.cpp:236` `ReadFloatConfig({"ui.overlayFallback.Width", "ui.overlayTest.Width"}, 1.2f)`
- `m-karma/src/ui/system.cpp:237` `ReadFloatConfig({"ui.overlayFallback.Height", "ui.overlayTest.Height"}, 0.7f)`

### Alias Purge Checklist

1. Confirm canonical-vs-legacy orientation for each pair with subsystem owners.
2. Ensure canonical keys exist in template configs (`data/client/config.json`, `data/server/config.json`, and relevant demo configs).
3. Update validation contracts so canonical keys are mandatory before alias removal.
4. Replace each `Read*Config({"new", "legacy"}, ...)` with single-key read (`ReadRequired*Config("new")` where policy requires hard-fail).
5. Remove legacy keys from shipped/demo JSON fixtures once all callsites are single-key.
6. Add regression tests that fail when removed legacy keys are reintroduced in code paths.

## 6) Follow-Up Task: Hardcoded Runtime Values -> JSON Contract

- Task: perform a whole-source sweep for hardcoded runtime constants and move them to explicit JSON config keys.
- Goal: remove behavior-defining constants from code paths, keeping values in config with required-key validation where appropriate.
- Scope: both `m-karma/src/` and `m-bz3/src/`, including networking, gameplay, UI, and server runtime wiring.
- Required deliverables:
  - inventory of hardcoded constants with file/line references,
  - proposed canonical JSON key for each constant,
  - migration order (safe/default-preserving first, behavioral changes last),
  - test updates proving config-driven behavior.

Specific tracked example:
- `m-bz3/src/server/net/transport_event_source/internal.hpp:224` `kMaxClients = 50`
  - promote to config key (`server.clients.max`) and remove hardcoded transport ceiling.
