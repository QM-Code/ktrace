# src/game/renderer/README.md

Game-owned rendering orchestration and radar rendering.

## Rendering flow
- Main scene renders through engine renderer core to the swapchain.
- Radar renders into an **overlay render target** owned by `RadarRenderer`.
- HUDs (ImGui/RmlUi) composite the radar texture on top of the main scene.

## Responsibilities
- `Renderer` owns engine renderer core and game-specific orchestration.
- `RadarRenderer` owns the radar target, radar entities, and FOV overlay primitives.
- `RadarConfig` bundles shader paths and defaults for radar setup.

## ECS radar data
- Tag world/static meshes with `game::renderer::RadarRenderable` to show them on radar.
- Add `game::renderer::RadarCircle` (radius, enabled) for player/shot dots.
