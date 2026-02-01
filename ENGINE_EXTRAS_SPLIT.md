# Engine vs Extras Mapping (KARMA-REPO baseline)

This document inventories the current BZ3 engine headers/sources and maps them
against `KARMA-REPO/` (the authoritative micro-engine baseline). Anything **not**
present in `KARMA-REPO/` should be removed from `src/engine/` and moved into
`src/karma-extras/` (or left in `src/game/` if game-specific).

## Baseline summary (KARMA-REPO, source-based)
KARMA-REPO implements the following in source (`include/` + `src/`):
- app: EngineApp, GameInterface, UIContext/UIDrawData (UI texture create/update/destroy)
- ecs: world/entity/registry + component storage
- components: audio, camera, collider, environment, layers, light, mesh, player controller,
  rigidbody, script, tag, transform, visibility
- scene: Scene + Node
- systems: ISystem + SystemGraph
- renderer: RenderSystem, GraphicsDevice, Backend interface, resource registry, materials,
  textures, render targets, skybox + Diligent backend implementation
- physics: PhysicsSystem + world + rigid/static/player; backends for Jolt + Bullet
- audio: AudioSystem + backend; backends for miniaudio + SDL
- input: InputSystem with action bindings (bindKey/bindMouse/trigger) + actionDown/Pressed
- platform: Window interface + SDL/GLFW backends
- network: transport interface + ENet client/server
- geometry: mesh_loader
- math + core IDs/types

Anything outside these concerns (config/data paths, file I/O helpers, UI framework
bridges, world/content loading, JSON, i18n, etc.) belongs in **karma-extras**.

## Keep in src/engine (core)
Match to KARMA-REPO equivalents:

- `src/engine/app/*` (EngineApp/GameInterface/EngineConfig)
- `src/engine/ecs/*` (world, registry, system graph, core ECS types)
- `src/engine/scene/*` **(currently missing; add or align)**
- `src/engine/core/*` (core ids/types)
- `src/engine/math/*` **(if missing, align with KARMA-REPO math headers)**
- `src/engine/renderer/*` (device/backend/resource registry/types)
- `src/engine/graphics/*` (renderer backend abstraction + device)
- `src/engine/physics/*` (backend + world + rigid/static/player)
- `src/engine/audio/*` (backend + audio system)
- `src/engine/input/*` (InputSystem equivalent with action binding API)
- `src/engine/platform/*` (window/events)
- `src/engine/network/*` (transport + factory)
- `src/engine/geometry/*` (mesh loader is present in KARMA-REPO)

## Move to src/karma-extras (not in KARMA-REPO)
These do **not** exist in KARMA-REPO sources and should be removed from core engine:

- `src/engine/common/*` (config store, JSON, data paths, file utils, i18n)
- `src/engine/world/*` (world/content loading + fs backend)
- `src/engine/ui/*` (RmlUi/ImGui bridges, UI overlay helpers)
- Any engine-owned UI framework glue or UI render-target bridges

## Public header mismatches (examples)
Current `include/karma/` contains many headers not present in KARMA-REPO sources:
- `karma/common/*`
- `karma/world/*`
- `karma/ui/*`
- `karma/input/mapping/*` and `karma/input/bindings_text.hpp`
These should move to `include/karma_extras/` and be compiled into `karma_extras`.

## src/game candidates for extras
- Any **generic** UI/HUD/console code that is not BZ3-specific
- Any reusable config/IO helpers embedded in game code

## Immediate alignment tasks
1) Make `include/karma/karma.h` mirror `KARMA-REPO/include/karma/karma.h`.
2) Add missing `scene` + `math` headers or align BZ3 equivalents.
3) Remove `common/`, `world/`, `ui/` from engine target and move
   into `karma_extras` target + `include/karma_extras/`.
4) Keep `geometry/mesh_loader` in core (present in KARMA-REPO).
5) Ensure `karma_extras` links against `karma` and contains all removed helpers.

## Notes
- `KARMA-REPO/` is canonical. If unsure where a feature belongs, compare against
  its headers and docs first.
- Do not move gameplay code into engine; only move generic helpers into extras.
- One-line shim headers (e.g., re-export-only `.hpp` wrappers) should be removed
  during alignment, not moved into `karma-extras`.
