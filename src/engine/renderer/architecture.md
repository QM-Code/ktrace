# src/engine/renderer/architecture.md

Engine renderer core abstractions.

- `RendererCore` owns the graphics device + scene renderer.
- `RendererContext` carries camera and layer data for each frame.
- `SceneRenderer` is the backend-facing draw interface used by systems.
