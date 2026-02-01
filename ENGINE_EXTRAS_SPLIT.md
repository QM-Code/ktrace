# Engine vs Extras Mapping (KARMA-REPO baseline)

This document inventories the current BZ3 engine headers/sources and maps them
against `KARMA-REPO/` (the reference engine tree). The goal is **convergence**:
`src/engine/` and `KARMA-REPO/` may both change, and the end state is that they
match each other in scope and API, based on the *best overall design*. Anything
that does not belong in the micro-engine core should be moved into
`src/karma-extras/` (or left in `src/game/` if game-specific).

## Baseline summary (KARMA-REPO, source-based)
KARMA-REPO currently implements the following in source (`include/` + `src/`);
this is a starting point, not a one-way constraint:
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
`src/game/` should not link directly to backend libs (bgfx/diligent/jolt/physx/sdl);
those are consumed by `karma`/`karma_extras` and propagate transitively.

## Keep in src/engine (core)
Match to KARMA-REPO equivalents:

- `src/engine/app/*` (EngineApp/GameInterface/EngineConfig)
- `src/engine/ecs/*` (optional engine sync systems only; ECS core types live in `include/karma/ecs/`)
- `src/engine/scene/*` **(currently missing; add or align)**
- `src/engine/core/*` (core ids/types)
- `src/engine/math/*` **(removed; standardize on glm instead)**
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

- `src/karma-extras/ui/*` (RmlUi/ImGui bridges, UI overlay helpers)
- Any engine-owned UI framework glue or UI render-target bridges
- Engine-specific ECS render glue components (RenderMesh/RenderEntity/RenderLayer/Material/Transparency/ProceduralMesh)

## Public header mismatches (examples)
Current `include/karma/` contains many headers not present in KARMA-REPO sources:
- `karma/ui/*`
- `karma/input/mapping/*` and `karma/input/bindings_text.hpp`
These should move to `include/karma_extras/` and be compiled into `karma_extras`.

## Render ECS glue (extras)
The engine-specific ECS render components now live in:
- `include/karma_extras/ecs/render_components.h`

These are considered **extras**, not core engine, because they depend on engine
graphics types and are not part of KARMA-REPO’s core ECS.

## src/game candidates for extras
- Any **generic** UI/HUD/console code that is not BZ3-specific
- Any reusable config/IO helpers embedded in game code

## Immediate alignment tasks
1) Make `include/karma/karma.h` mirror `KARMA-REPO/include/karma/karma.h`.
2) Add missing `scene` + `math` headers or align BZ3 equivalents.
3) Remove `ui/` from engine target and move
   into `karma_extras` target + `include/karma_extras/`.
4) Keep `geometry/mesh_loader` in core (present in KARMA-REPO).
5) Ensure `karma_extras` links against `karma` and contains all removed helpers.

## Notes
- `KARMA-REPO/` and `src/engine/` must converge; neither is sacred. If unsure,
  choose the better design and make both match.
- Do not move gameplay code into engine; only move generic helpers into extras.
- One-line shim headers (e.g., re-export-only `.hpp` wrappers) should be removed
  during alignment, not moved into `karma-extras`.
