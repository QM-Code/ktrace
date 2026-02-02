# src/game/architecture.md

This document is a **high-level architecture overview** of the BZ3 game layer.
Subsystem-level architecture lives in each directory’s `architecture.md`.

## Runtime flow (client)
1) Engine initializes subsystems (render, input, physics, audio, UI).
2) Connects to server (network protocol in `net/`).
3) `ClientWorldSession` loads world data and spawns game entities.
4) `Game` updates gameplay state and ECS data; engine renders.
5) UI frontends draw HUD and console using engine UI bridges.

## Runtime flow (server)
1) Server initializes physics + network.
2) `ServerWorldSession` manages world state.
3) Plugins can hook server events.

## Key integration points with Karma
- **Renderer**: Game renderer uses engine renderer core for the main scene and owns radar rendering.
- **Input**: Engine input mapping provides named actions; game interprets them.
- **Physics**: Engine physics world drives player/rigid bodies.
- **UI**: Game frontends render into engine UI bridges.

## Next steps
- `src/game/ui/architecture.md` for UI specifics
- `src/game/net/architecture.md` for protocol flow
- `src/game/renderer/architecture.md` for rendering
