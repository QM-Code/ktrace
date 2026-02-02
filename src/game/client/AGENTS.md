# src/game/client/AGENTS.md

Read `src/AGENTS.md` and `src/game/AGENTS.md` first.
This directory contains the **client-side gameplay runtime**.

## Responsibilities
- Client game loop coordination
- Player entity and camera updates
- Roaming mode camera (no player body)
- World session orchestration on client

## Key files
- `main.cpp`
  - Client entry point (build-time links to Karma engine).

- `game.*`
  - High-level client gameplay container.
  - Owns player, world session, and UI interactions.

- `player.*`
  - Player state updates and camera follow (when not roaming).

- `roaming_camera.*`
  - Free-fly camera used when joining in roam mode.

- `world_session.*`
  - Client world lifecycle: loading world data, syncing entities.

- `console.*`
  - Client-side console hook (chat input, command routing).

## How it connects to engine
- Uses `ClientEngine` (from `src/game/engine/`) to access render/input/audio.
- Uses engine input mapping with game action IDs.
- Roaming mode uses engine input actions (`roam*`).

## Gotchas
- Roaming mode should never spawn a player body.
- Player camera overrides engine renderer camera each frame when in play mode.
- UI tooling: `--ui-smoke` toggles HUD elements on a timer for quick parity checks.
