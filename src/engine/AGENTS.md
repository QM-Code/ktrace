# src/engine/AGENTS.md

Read `src/AGENTS.md` first. This file describes the **Karma engine** subtree.
Everything here is intended to be **game-agnostic** and eventually moved into its
own repository. While Karma is still in-tree, BZ3 includes it via `karma/...`
headers under `include/karma/`.

## What Karma provides
Karma is a modular runtime that owns:

- Platform and window management
- Graphics backends and GPU resource management
- UI rendering bridges (ImGui/RmlUi render-to-texture) (moved to karma-extras)
- Input mapping (action-agnostic)
- Physics backends (Jolt or PhysX)
- Audio backends (miniaudio or SDL audio)
- Network transports (ENet)
- World content and data path resolution
- Config/I18n helpers
- ECS scaffolding (optional for game use)

BZ3 uses these engine services to implement gameplay and UI.

## Where to start (common tasks)
- **Graphics / rendering** → `src/engine/graphics/AGENTS.md` and
  `src/engine/renderer/AGENTS.md`
- **Input mapping** → `src/engine/input/AGENTS.md`
- **Physics** → `src/engine/physics/AGENTS.md`
- **Windowing / events** → `src/engine/platform/AGENTS.md`
- **Config/I18n** → `src/engine/common/AGENTS.md`
- **UI bridges** → `src/karma-extras/ui/AGENTS.md`
- **Network transport** → `src/engine/network/AGENTS.md`
- **World content loading** → `src/karma-extras/world/AGENTS.md`

## Public headers
- Game code includes `karma/...` headers; these live in `include/karma/` and
  point directly at `src/engine/...`.

## Cascading rule
Every subdirectory under `src/engine/` has its own `AGENTS.md`, `README.md`,
and `architecture.md`. Read from the top down:

1) `src/AGENTS.md`
2) `src/engine/AGENTS.md`
3) The subsystem AGENTS.md you care about

## Subtree map (top-level)
- `app/` — engine runtime shell (`EngineApp`, lifecycle hooks)
- `audio/` — audio system + backend factory
- `common/` — config helpers, data paths, i18n
- `core/` — shared core types
- `data/` — engine-level default config
- `ecs/` — ECS primitives + systems
- `geometry/` — mesh loading utilities
- `graphics/` — backend factories, devices, resources
- `input/` — mapping from platform events to action IDs
- `network/` — transport layer (ENet)
- `physics/` — backend factory + physics world
- `platform/` — window/events abstraction
- `renderer/` — renderer core and scene orchestration
- `ui/` — moved to `src/karma-extras/ui/`
- `world/` — moved to `src/karma-extras/world/`


If you’re unsure, read the subsystem’s `README.md` for a human-level overview,
then `architecture.md` for structure, then `AGENTS.md` for detailed guidance.
