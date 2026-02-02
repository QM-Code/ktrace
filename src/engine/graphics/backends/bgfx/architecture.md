# src/engine/graphics/backends/bgfx/architecture.md

The bgfx backend implements the engine graphics interface. The renderer and
resource registry call into this backend to create GPU resources and submit
render passes.

UI overlay compositing uses premultiplied alpha blending (`ONE / INV_SRC_ALPHA`).
