# src/engine/graphics/backends/bgfx/AGENTS.md

Read `src/engine/graphics/AGENTS.md` first.
This directory contains the bgfx graphics backend.

Key responsibilities:
- Device initialization and shutdown
- Shader/pipeline setup
- Mesh/material/texture uploads
- Render targets and frame submission
- UI render bridge integration

If a render bug only happens on bgfx, this is where to debug.

## UI overlay blending
- The bgfx UI overlay composite is premultiplied (`ONE / INV_SRC_ALPHA`).
- ImGui uses straight alpha internally, but its UI render target is composited
  here; keep the overlay blend premultiplied to avoid double‑dark/washed alpha.
