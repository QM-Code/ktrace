# src/game/AGENTS.md

Read `src/AGENTS.md` first. This file describes the **BZ3 game** subtree.
Everything here is game-specific: tanks, HUD, radar, protocol, gameplay rules.

## How BZ3 uses Karma
BZ3 is built on top of Karma (engine layer). The high-level flow is:

- Engine initializes subsystems (render, input, audio, physics, UI).
- Game owns gameplay state and updates ECS data.
- UI frontends (ImGui/RmlUi) render HUD/console via engine UI bridges.
- Game networking protocol drives world sessions and player state.

## Where to start (common tasks)
- **Gameplay / client** → `src/game/client/AGENTS.md`
- **Server / world** → `src/game/server/AGENTS.md` and `src/game/world/AGENTS.md`
- **Network protocol** → `src/game/net/AGENTS.md` and `src/game/protos/AGENTS.md`
- **UI / HUD / Console** → `src/game/ui/AGENTS.md`
- **Renderer (game-specific)** → `src/game/renderer/AGENTS.md`
- **Input actions** → `src/game/input/AGENTS.md`

## Cascading rule
Every subdirectory under `src/game/` has its own AGENTS.md, README.md, and
architecture.md. Read from the top down:

1) `src/AGENTS.md`
2) `src/game/AGENTS.md`
3) The subsystem AGENTS.md you care about

## Subtree map (top-level)
- `client/` — client runtime, player, roaming camera, client loop
- `server/` — server runtime, world session, plugins
- `net/` — protocol and message handling
- `protos/` — protobuf schema
- `renderer/` — game-level render orchestration (radar, overlay)
- `ui/` — HUD/console, frontends, models, controllers
- `input/` — game action IDs and defaults
- `engine/` — game-specific engine glue / facades
- `common/` — game-level data path specs and shared helpers
- `world/` — world config and loaders (game layer)

If you’re unsure, read the subsystem’s README.md for an overview and then the
architecture.md for structure.
