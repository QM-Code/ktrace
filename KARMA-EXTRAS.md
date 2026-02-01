# Karma Extras Split Plan

This project refactors the current BZ3 engine into two clean layers and **converges** the two engine codebases: `src/engine/` and `KARMA-REPO/` are both allowed to change, and the goal is to end with them matching each other (same scope and compatible API) based on the *best overall design*, not a one-way clone of either tree. Functionality that does not belong in the micro-engine core is moved into `src/karma-extras/`, while core engine functionality is kept aligned between `src/engine/` and `KARMA-REPO/`. A backup copy exists in `KARMA-REPO-ORIG/`; edits may be made directly in `KARMA-REPO/` and later pushed upstream. The goal is a true split: `src/engine/` and `KARMA-REPO/` become drop-in engine-only repos with the same shape, `src/karma-extras/` provides “middleman” conveniences (config/data, file I/O, UI bridges, asset loaders, input mapping helpers, etc.), and `src/game/` remains game-specific.
`src/karma-extras/` must be structured from the start to function as an independent repo (its own headers, CMake targets, install/export, and documentation), similar to how we are treating `src/engine/`.

Goal:
- `src/engine/` becomes the micro-engine (core API + ECS + scene + backend interfaces + EngineApp).
- `src/karma-extras/` becomes the middleman layer (config, file I/O, UI bridges, asset loaders, input mapping helpers).
- `src/game/` remains game-specific.
- Build order must be: (1) `src/engine/` builds core libs, (2) `src/karma-extras/` builds and links against engine headers/libs, (3) `src/game/` builds and links against both engine and extras.

## Non-goals / invariants
- `KARMA-REPO/` and `src/engine/` must converge; neither is sacred. Pick the better design and make both match.
- Do not move gameplay code from `src/game/` into `src/engine/`; only migrate reusable, non-game-specific pieces into `src/karma-extras/`.
- `src/karma-extras/` must be repo-ready from the start: its own public headers, CMake targets, install/export, and minimal documentation.
- Dependency direction is one-way: `karma-extras` can depend on `karma`, but `karma` must never depend on `karma-extras`.
- Use the name `karma-extras` consistently (typos like `karmak-extras` should be corrected).
- Engine-specific ECS render glue (RenderMesh/RenderEntity/RenderLayer/Material/Transparency/ProceduralMesh) now lives in `include/karma_extras/ecs/render_components.h` and is considered **extras**, not core.
- `src/game/` should not link directly to backend libraries (bgfx/diligent/jolt/physx/sdl); those are pulled in transitively by `karma` and `karma_extras`.

## Phase 0 — Inventory + decision map
- Walk `src/engine/` and `src/game/` and tag each module:
  - Core engine
  - Extras
  - Game
- Produce a file-level mapping doc (e.g., `ENGINE_EXTRAS_SPLIT.md`).

Likely mapping (high confidence):

Move to **extras**:
- `src/engine/common/` (config store, data path resolution)
- `src/karma-extras/ui/` (RmlUi/ImGui bridges + backend glue)
- `src/engine/geometry/` (mesh/GLB loading)
- `src/engine/input/` *(if we want raw input only in core and binding/mapping in extras)*
- Any generic HUD/console UI in `src/game/ui/` that isn’t BZ3-specific

Keep in **core**:
- `src/engine/app/` (EngineApp, GameInterface, UiLayer)
- `src/engine/ecs/`, `src/engine/core/`, `src/engine/scene/` (add if missing)
- `src/engine/graphics/` (backend interfaces + device abstraction)
- `src/engine/renderer/` (system orchestration, resource descriptors)
- `src/engine/physics/` (backend interfaces, thin wrappers)
- `src/engine/audio/` (backend interfaces, thin wrappers)
- `src/engine/network/` (backend interfaces, transport wrapper)
- `src/engine/platform/` (window backend interface + minimal implementation)

## Phase 1 — Create extras target (CMake + headers)
- Add `src/karma-extras/` tree.
- Add new CMake target `karma_extras`:
  - `target_link_libraries(karma_extras PUBLIC karma)`
  - Owns external deps (JSON, UI libs, file I/O helpers, etc.)
- Keep exports separate: `karma` (core) and `karma_extras`.

Deliverable:
- Builds still pass; `karma_extras` exists but may be empty initially.

## Phase 2 — Move config + data path helpers
- Move `src/engine/common/` → `src/karma-extras/common/`.
- Update includes to new include root (e.g., `include/karma_extras/common/...`).
- Update EngineApp/Game usage to access config through extras, not core.
- Minimal “core” config: only EngineConfig + runtime settings.

Deliverable:
- Core engine compiles without config/data helpers.

## Phase 3 — Move UI bridges
- Move UI bridges and platform renderers into `src/karma-extras/ui/`.
- Core keeps only UI interfaces (`UiLayer`, `UIContext`, `UIDrawData`).
- Extras supplies RmlUi/ImGui adapters that implement UiLayer and emit draw data.

Deliverable:
- UI libs no longer in core engine.

## Phase 4 — Move geometry loaders
- Move `src/engine/geometry/` → `src/karma-extras/geometry/`.
- Update renderer/resource registry to accept raw mesh data or an interface.
- Extras handles file parsing; core consumes mesh data.

Deliverable:
- Core doesn’t parse files.

## Phase 5 — Decide input mapping split
Option A (strict micro-engine):
- Core exposes raw input events only.
- Move action mapping (`src/engine/input/`) to extras.

Option B (practical):
- Keep input mapping in core (still generic).

Deliverable:
- One of the above chosen and implemented.

## Phase 7 — Update docs + examples
- Update `README.md`, `src/engine/README.md`, `architecture.md`.
- Add `karma-extras` README explaining it’s the middleman layer.
- Add/adjust demos: core demos should only use core APIs.

## Phase 8 — Stabilize build modes
- Add CMake options:
  - `KARMA_ENGINE_ONLY=ON`
  - `KARMA_EXTRAS=ON`
  - `BZ3_GAME_ONLY=ON` (already exists)
- Ensure `karma` and `karma_extras` can be installed separately.

## Phase 9 — Migration checklist + final cleanup
- Verify BZ3 builds against `karma + karma_extras`.
- Remove any engine headers that still include extras paths.
- Provide a short “how to create a new game repo” doc.
