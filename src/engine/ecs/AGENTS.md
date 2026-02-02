# src/engine/ecs/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory contains the **engine ECS scaffolding**: lightweight component
types and optional sync systems that operate on `karma::ecs::World`.

## Purpose
ECS is **optional**. The engine can render and simulate without it, but the
scaffolding exists to support more data-driven game logic later.

## Key pieces
- `render_components.h` (in `include/karma_extras/ecs/`)
  - Engine-only render components (mesh/material/layer/etc) now live in karma-extras.
- `systems/`
  - Optional engine-level systems that sync engine state to ECS
    (render/audio/physics/camera/procedural mesh).

## How the game uses it
- The game renderer may register ECS entities for rendering.
- Game code is free to ignore ECS entirely.

## Gotchas
- Keep ECS generic and engine-level. Avoid game-specific components here.
- Use `karma::ecs::World::storage/has/get/add` (KARMA-REPO API).
