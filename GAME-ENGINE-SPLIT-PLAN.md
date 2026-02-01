# Game/Engine Split Plan

This document is a migration playbook for separating the engine from the BZ3 game so the two can live in separate repos. The goal is a reusable engine (“karma”) under `src/engine/` with stable headers and a thin game layer under `src/game/` that depends on the engine. It is written to guide a future agent step‑by‑step through the migration with minimal guesswork.

## Target shape (repo split ready)

- **Engine repo (karma)**: `src/engine/` only, headers exported as `karma/...`, no includes from `src/game/`, no game assets.
- **Game repo (bz3)**: `src/game/` only, depends on `karma` for runtime, ECS, graphics, input, audio, physics, networking, UI overlay plumbing.
- **Entry points**: game `main.cpp` instantiates `karma::app::EngineApp` and provides a `karma::app::GameInterface` implementation.
- **UI overlay**: engine exposes a render‑to‑texture overlay interface; game implements HUD/Console on top.
- **ECS**: engine provides `ecs::World` + `SystemGraph`; game registers components/systems.

## What should remain in `src/game/`

These modules are specific to the BZ3 game (tanks, shots, HUD/Console, BZ3 protocol) and should stay in the game layer.

- **Gameplay logic**: `src/game/client/*`, `src/game/server/*` (actors, shots, chat, world_session, game rules).
- **Game protocol**: `src/game/net/messages.hpp`, `src/game/net/proto_codec.*`, `src/game/protos/messages.proto`.
- **Game UI**: HUD, Console, bindings/settings panels, community browser, etc.
  - All of `src/game/ui/frontends/*`, `src/game/ui/controllers/*`, `src/game/ui/models/*`, `src/game/ui/console/*`, `src/game/ui/config/*`.
- **Game input actions/bindings**: `src/game/input/*` (actions, default bindings, state).
- **Game asset/config spec**: `src/game/common/data_path_spec.*`, `src/game/world/config.*`.

## What should move (or be split) into `src/engine/`

These modules are engine‑agnostic or are toolkit glue that should be reusable by a different game.

### 1) Rendering core vs game rendering
- `src/game/renderer/render.*` mixes engine‑level scene orchestration with game‑specific radar/HUD logic.
- Proposed split:
  - `src/engine/renderer/` (or `src/engine/graphics/scene/`): core scene renderer, camera, render targets, entity creation, materials, UI overlay plumbing.
  - `src/game/renderer/`: game‑specific rendering features (radar, FOV lines, scoreboard bindings, theme decisions).
  - Engine should expose hooks for secondary layers (radar, overlays), but gameplay defines what they represent.

### 2) UI toolkit plumbing vs game UI
- The UI frontends (ImGui/RmlUi) are game‑specific because they encode HUD/Console structure.
- But render bridges and backend glue are engine‑agnostic:
  - Move `src/game/ui/bridges/*` into `src/engine/ui/bridges/*`.
  - Move toolkit platform renderers (`renderer_bgfx/diligent.*`) under `src/engine/ui/platform/`.
  - Keep `src/game/ui/frontends/*` for layout, panels, HUD widgets.

### 3) Networking
- Engine transports live in `src/engine/network/*`.
- Game protocol and encode/decode remain in `src/game/net/*`.
- Backends are now game‑owned and are adapters atop engine transport.

### 4) Input
- Engine has mapping in `src/engine/input/mapping/*`.
- Game uses `src/game/input/*` for actions/bindings/state.
- Keep action definitions in game; keep binding parsing/serialization in engine.

### 5) World/content
- Engine already has generic world backends (`src/engine/world/*`).
- Game world config + gameplay constraints stay in `src/game/world/*`.
- Any purely data‑loading logic (mesh/asset extraction) can move to engine.

### 6) Runtime scaffolding (karma‑style EngineApp)
- `src/engine/app/engine_app.*` with `karma::app::EngineApp`.
- `karma::app::GameInterface` for game lifecycle hooks.
- Game provides `Bz3ClientGame` / `Bz3ServerGame` implementing GameInterface.
- `src/game/engine/client_engine.*` / `server_engine.*` act as thin adapters or get collapsed later.

## Status (completed vs remaining)

### Completed
- **Engine/Game include boundary**: no `src/engine` includes from `src/game` (hard split in code).
- **Namespace rename**: `bz::` → `karma::` across codebase.
- **UI overlay + render‑to‑texture**: engine owns bridge; game owns HUD/Console.
- **ECS in engine**: `ecs::World`, components, render system in place.
- **Render pipeline integration**: ECS render path feeding engine backends (bgfx/diligent).
- **Networking split**: transport in engine, protocol + backend adapters in game (`src/game/net/*`).
- **Binding text helpers**: moved `IsMouseBindingName` / join/split helpers to `src/engine/input/bindings_text.*`.
- **Build split**: engine now builds as object targets (`karma`, `karma_server`) and game links them; build output shows `karma.dir` vs `bz3.dir`.
- **Header export mapping**: engine headers are now consumed via `karma/...` include path; game‑local engine adapters use `game/engine/...`.

### Remaining (short list)
- **Optional cleanup**:
  - Audit any engine files that still embed game‑specific logic (radar/UI references should remain game‑side).
  - Consider moving more generic helpers (if any) from game → engine.

## Suggested staged migration plan (aligned with karma model + ECS)

### Phase 0 — Stabilize boundaries (no behavior changes)
- **Add a `karma` include root** for engine headers while keeping existing includes working.
- **Block engine → game includes**.

### Phase 1 — Engine runtime shell (EngineApp)
- EngineApp owns window, render, audio, physics, input, network.
- Game implements `GameInterface` for lifecycle hooks.

### Phase 2 — Engine UI overlay (render‑to‑texture)
- Engine composes overlay; game renders HUD/Console to the overlay.
- UI platform renderers live in engine.

### Phase 3 — ECS core in engine
- `ecs::World`, `SystemGraph`, render components.

### Phase 4 — RenderSystem + adapters
- Engine RenderSystem consumes ECS components.
- Game adapters feed ECS from gameplay objects.

### Phase 5 — Networking cleanup
- Engine transport, game protocol, game backends.

### Phase 6 — Optional migrations
- Migrate additional helpers or systems to engine as they become game‑agnostic.

## Optional next step

Before moving code into separate repos, generate a target dependency graph (engine → game only) to confirm no game‑specific types leak into engine modules.

## Repo split checklist (when ready to split)

Use this as a step‑by‑step cutover guide for separating the repos without guesswork.

1) **Engine repo extraction**
   - Copy `src/engine/` into new repo (karma).
   - Export public headers under `include/karma/` and ensure include root exposes them.
   - Retain engine dependencies and CMake options (`KARMA_*` macros/env vars).
   - Verify engine builds as libraries (or object libs) with no `src/game` references.

2) **Game repo extraction**
   - Copy `src/game/`, `data/`, and game‑specific build scripts.
   - Add karma as an external dependency (submodule or prebuilt package).
   - Replace any local engine include paths with `karma/...` (should already be done).

3) **CMake shape (split build in one repo)**
   - Keep a top‑level `CMakeLists.txt` that adds `engine/` and `game/` subdirs.
   - `engine/CMakeLists.txt`: defines `karma` and `karma_server` libraries, exports include dirs.
   - `game/CMakeLists.txt`: defines `bz3` and `bz3-server`, links to `karma` / `karma_server`.
   - Move build flags to engine scope where possible (render/audio/physics backend selection).

4) **Header export strategy**
   - Continue to expose engine headers via `karma/...`.
   - Decide whether to keep forwarders or install headers into an include prefix.
   - Update any new engine files to include via `karma/...` to enforce the boundary.

5) **Proto + namespace naming**
   - Keep protobuf package as `karma` for engine‑level types; game‑level messages stay under `game::net`.
   - Ensure public engine APIs remain under `karma::` namespace.

6) **Smoke test matrix**
   - Build: bgfx/rmlui, bgfx/imgui, diligent/rmlui, diligent/imgui.
   - Run: join server, render world, HUD/console visible, radar correct.

## Alignment notes with bz3‑karma

- `karma::app::EngineApp` matches `bz3‑karma`’s engine runtime model.
- Overlay in karma maps to BZ3’s existing UI render‑to‑texture path.
- ECS is intentionally required earlier here to converge with karma’s `World` + systems model.
