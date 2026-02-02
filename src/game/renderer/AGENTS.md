# src/game/renderer/AGENTS.md

Read `src/game/AGENTS.md` first.
This directory holds **game-owned rendering orchestration**.

## Scope
- Ties game entities to engine renderer core.
- Manages radar-only render IDs and ECS-driven radar sync.
- Owns radar rendering (overlay render target + overlays).

## Key files
- `renderer.*`
  - Game-facing renderer wrapper.
  - Controls camera and UI overlay composition.

- `radar_renderer.*`
  - Radar render target, radar objects (player/shot/buildings), FOV lines.
  - Handles radar orientation and world-to-radar mapping.
  - Radar dots are driven by ECS via `game::renderer::RadarCircle`.

## How it connects
- Uses engine renderer core (`src/engine/renderer/`).
- Uses engine graphics backends for GPU resources.
- HUD uses radar texture from here.

## Gotchas
- Radar is sensitive to orientation math; test in all backends after changes.
- UI overlay composition is done by the game renderer, not engine.
