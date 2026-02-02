# src/AGENTS.md

This is the top of the **cascading agent docs** for the `src/` tree.
Read this first, then follow the AGENTS.md files down the directory tree.

## What you are looking at
`src/` is split into two top-level domains:

- `src/engine/` — the **Karma engine** (game-agnostic systems).
- `src/game/` — the **BZ3 game** (tanks, HUD, radar, net protocol, UI, rules).

Today both live in one repo for fast iteration, but **Karma is intended to become its
own repository**. The structure and naming already reflect that goal:

- Engine code uses the `karma::` namespace and lives under `src/engine/`.
- Game code uses `game::` or `ui::` and lives under `src/game/`.
- `include/karma/` contains **public headers** so game code can include
  `karma/...` even while Karma is still in-tree.

## The engine/game boundary (mental model)
Think of Karma as a reusable runtime with rendering, input mapping, physics,
network transports, world loading, and UI bridges. BZ3 plugs into Karma by
providing game-specific logic:

- **Engine** owns: render devices, backend selection, input mapping, physics
  backends, audio backends, windowing, common config/i18n, data path resolution,
  UI render bridges, ECS scaffolding, and generic world/content loading (moved to karma-extras).
- **Game** owns: gameplay simulation, networking protocol, world session logic,
  UI/HUD/console, gameplay rendering and radar, and any content rules.

The eventual “engine repo” will export headers and libraries that BZ3 links
against; for now they are built together in one CMake project.

## Build-time backend selection
Backends are selected by CMake cache variables (see top-level README and
CMakeLists). Only these combinations are valid now:

- Render: `bgfx` or `diligent`
- Physics: `jolt` or `physx`
- UI: `imgui` or `rmlui`
- Audio: `miniaudio` or `sdlaudio`
- Window: `sdl3` (sdl2 stub exists)

If you see code for Forge or Bullet, it should not exist anymore. The supported physics backends are Jolt and PhysX.

## Cascading rule (important)
Each subdirectory contains its own `AGENTS.md`, `README.md`, and `architecture.md`.
You should **not** read every file in the tree. Read only the files that apply
to the subsystem you’re working on.

Recommended flow:

1) Read **this** file (src/AGENTS.md).
2) Read `src/engine/AGENTS.md` or `src/game/AGENTS.md` depending on your task.
3) Continue down the tree only as far as needed for the directory you will edit.

This is intentionally “CSS-like”: high-level context at the top, more detail as
you descend. If you’re working on e.g. `src/game/ui/`, you should read:

- `src/AGENTS.md`
- `src/game/AGENTS.md`
- `src/game/ui/AGENTS.md`
- Any deeper AGENTS.md in the UI subtree

## Typical agent requests (examples)
- “We’re working on the engine renderer. Please read `src/engine/AGENTS.md` then
  `src/engine/graphics/AGENTS.md` and `src/engine/renderer/AGENTS.md`.”
- “We need to adjust the network protocol. Read `src/game/net/AGENTS.md` and
  `src/game/protos/AGENTS.md`.”
- “Please update HUD behavior. Start at `src/game/ui/AGENTS.md` and then
  `src/game/ui/frontends/AGENTS.md`.”

If you’re unsure where to start, check the closest AGENTS.md above your target.
