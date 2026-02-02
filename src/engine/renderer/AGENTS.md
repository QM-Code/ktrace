# src/engine/renderer/AGENTS.md

Read `src/engine/AGENTS.md` first.
This directory contains **engine renderer core headers**.

## Scope
- Renderer context/types shared by engine systems.
- Scene renderer interfaces used by backends and systems.

## Key files
- `renderer_context.hpp` — camera/layer context passed through render systems.
- `renderer_core.hpp` — core renderer entry point used by game/runtime.
- `scene_renderer.hpp` — scene interface for submitting renderables.
