# Engine Refactor Tracker (BZ3 -> Karma-Style)

This file tracks the incremental refactor from the current BZ3 layout to the
engine-purist Karma-style architecture described in NEW-ENGINE-REFACTOR.md.

Each phase should keep the build working at a defined checkpoint.

## Phase 0 - Audit (complete)

- Coupling map (game -> engine subsystems):
  - Rendering: game-owned render orchestration in `src/game/renderer/*`.
  - UI: game-owned HUD/console lifecycle in `src/game/ui/*`.
  - Input: game-owned input gating in `src/game/engine/client_engine.*`.
  - Physics: game uses engine physics types in world/player.
  - Audio: game uses engine audio types in player/client.
- Network: game uses engine transport API in ENet backend.
- Game still uses backend-specific UI hooks (bgfx thumbnail cache, ImGui/RmlUi renderers in karma-extras). Plan: move thumbnail cache to engine/extras graphics helpers and remove any direct backend includes from src/game so game is backend-agnostic.
  - Config/data/i18n: widespread `karma/common/*` usage in client/server/ui/world.
- Orchestration hotspots:
  - Client loop: `src/game/client/main.cpp`.
  - Server loop: `src/game/server/main.cpp`.
  - Engine update pipeline: `src/game/engine/*_engine.*`.
  - Render orchestration: `src/game/renderer/*`.

## Phase 1 - Public API Snapshot (build-safe) (complete)

Goal: Provide `include/karma/*` headers in BZ3 that mirror the Karma layout,
forwarding to existing engine headers. No behavior changes.

Checklist:
- Add public header tree under `include/karma/` (forwarders).
- Keep existing includes working.
- Build checkpoint: `bz3` + `bz3-server` compile unchanged.

## Phase 2 - Engine App Skeleton (complete)

Goal: Introduce Karma-style `EngineApp`, `GameInterface`, `EngineConfig` stubs,
hooked to existing engine loop internally. No gameplay migration yet.

Checklist:
- Add `EngineApp` / `GameInterface` stubs (if not already present).
- Add `EngineConfig` and `start/tick/isRunning` API.
- Optional sample/main that uses EngineApp under a build flag.
- Build checkpoint: existing binaries compile.

## Phase 3 - ECS Compatibility Layer (in progress)

Goal: Add ECS components and a compatibility layer to mirror current runtime
objects. Gameplay can write ECS data while engine reads from old paths.

Checklist:
- Add ECS components (Transform/Mesh/Camera/Light/Collider/Rigidbody/Audio).
- Sync layer between ECS and existing renderer/physics/audio.
- Render sync system stub added (opt-in via EngineConfig.enable_ecs_render_sync).
- Physics sync system stub added (opt-in via EngineConfig.enable_ecs_physics_sync).
- Audio sync system stub added (opt-in via EngineConfig.enable_ecs_audio_sync).
- Camera sync system stub added (opt-in via EngineConfig.enable_ecs_camera_sync).
- Client CLI `--ecs-smoke` added to exercise ECS render/camera sync path.
- ECS world mesh now rendered via ECS by default (no CLI flag).
- Radar now synced from ECS via game-owned `RadarRenderable` tag.
- Build checkpoint: compile + run current flows.
- ECS remote player rendering now default (no CLI flag).
- ECS shots rendering now default (no CLI flag).
- ECS local player entity now default (mesh skipped to avoid first-person tank).
- ECS renderer now prefers `MeshComponent.mesh_key` for model entities (restores materials).
- Added `ProceduralMesh` ECS component + sync system for runtime meshes.
- Removed legacy renderer model creation via render IDs (radar IDs only).
- Camera is now ECS-driven (camera sync enabled by default; renderer camera setters removed).
- UI overlay composition + present moved into EngineApp tick (engine-owned lifecycle).
- EngineApp now begins frames + renders main scene (game renderer handles radar only).
- Audio listener now ECS-driven (audio sync enabled by default in client).
- ConfigStore ticking moved into EngineApp tick (engine-owned lifecycle).
- EngineApp now derives graphics device from renderer core when not explicitly wired.
- Input event polling + Input update moved into EngineApp tick.
- UI event handling + UI update moved into EngineApp tick (overlay-owned lifecycle).
- Roaming camera update/apply moved into ClientEngine lateUpdate (main loop cleanup).
- Global UI input (escape/quick menu) moved into ClientEngine lateUpdate.

## Phase 4 - Engine-Owned Loop (client)

Goal: Engine owns loop and timing, game implements `GameInterface`.

Checklist:
- Move client loop ordering into engine.
- Game runs via `EngineApp::start()` + `tick()`.
- Keep old entry as fallback (temporary).
- Client now runs via EngineApp start/tick by default (flag removed).
- Build checkpoint: compile + run client.

## Phase 4b - Engine-Owned Loop (server)

Checklist:
- Server now runs via EngineApp start/tick by default (flag removed).
- Legacy GameInterface callbacks and EngineApp::run removed (start/tick only).
- Keep old loop as fallback (temporary).
- Build checkpoint: compile + run server.

## Networking - Join Handshake (in progress)

- Added join request/response handshake to avoid connect-then-kick UX.
- Hard protocol bump (no backwards compatibility).

## Phase 5 - Renderer Ownership Migration

Goal: Replace game render orchestration with engine systems using ECS data.

Checklist:
- Move `src/game/renderer/*` into engine systems or extension points.
- Kept game renderer/radar in `src/game/renderer` (engine renderer remains generic).
- Replace render IDs with ECS components.
- Build checkpoint: render scene + radar path.

## Phase 6 - UI Overlay Migration

Goal: Game supplies overlay only; engine owns UI lifecycle.

Checklist:
- Promote UI to overlay interface.
- Migrate HUD/console to overlays.
- Build checkpoint: HUD/console visible.

## Phase 7 - Physics/Input/Audio Ownership

Goal: Engine owns pump/step/update for input, physics, audio.

Checklist:
- Game only reads input state and mutates ECS.
- Engine performs physics step and audio updates.
- Build checkpoint: input + physics + audio verified.

## Phase 8 - Library Boundary & Build Targets

Goal: `src/engine` becomes a standalone library; `src/game` links to it and
includes only `include/karma/*`.

Checklist:
- Create `karma` library target.
- Install/export headers under `include/karma/`.
- Remove direct engine source compilation from game targets.
- Build checkpoint: compile + run client/server.
 - Engine now builds as static libraries (`karma`, `karma_server`).
 - Game targets now link `karma` / `karma_server` instead of object libraries.
 - `include/karma/*` forwarders now point directly at `src/engine/*`.
 - Removed in-tree forwarder headers under `src/engine/karma`.
 - Added install rules for `karma` libraries and `include/karma` headers.
 - Added CMake package export (`karmaTargets`, `karma-config.cmake`).

## Phase 9 - Cleanup & Docs

Goal: Remove compatibility shims and update documentation.

Checklist:
- Remove old entry points and shims.
- Update README / architecture / AGENTS / CONFIG-SCHEMA as needed.
- Add minimal example mirroring Karma `examples/main.cpp`.
 - Added top-level engine-only and game-only build modes.
