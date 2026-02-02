# src/game/engine/AGENTS.md

Read `src/game/AGENTS.md` first.
This directory contains **game-level engine glue**: adapters and orchestration
that connect BZ3 to the Karma engine.

## Key files
- `client_engine.*`
  - Owns engine subsystems for the client (render, input, ui, physics, audio).
  - EngineApp now pumps input + overlay updates; this layer consumes the results.
  - Bridges UI output to renderer and handles config-driven updates.
  - Integrates roaming camera and input reloads.

## How it connects
- Used by `src/game/client/` code to drive the frame lifecycle.
- Sits between game logic and engine subsystems.

## Gotchas
- Keep this layer game-specific; it may wrap engine APIs but should not live
  under `src/engine/`.
- Gameplay input is gated off when UI input is active; only global actions
  (chat/escape/quick quit/fullscreen) remain.
