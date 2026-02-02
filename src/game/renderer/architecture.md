# src/game/renderer/architecture.md

The renderer wraps the engine renderer core and provides game-owned features:
- Radar-only render ID management
- Camera control
- Radar render target and overlay elements

Radar uses an overlay render target that is composited by the HUD.
Radar shader paths are provided via `RadarConfig` when the game starts.
Radar dot markers are supplied by ECS (`RadarCircle`), while world meshes use `RadarRenderable`.
