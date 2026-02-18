# UI Architecture (current)

Read `src/AGENTS.md` and `src/game/AGENTS.md` before this file.

This document reflects the UI subsystem as it exists today, plus the near-term refactor direction. It is intended for future coding agents.

## 1) Big picture

The UI subsystem exposes a single backend interface (`ui::Backend`) and two frontend implementations:
- **ImGui** (`src/game/ui/frontends/imgui/`)
- **RmlUi** (`src/game/ui/frontends/rmlui/`)

The UI is split into two surfaces:
- **Console**: multi-tab UI (Community, Start Server, Settings, Bindings, ?)
- **HUD**: gameplay overlay (chat, radar, scoreboard, crosshair, FPS, dialog, quick menu)

Both frontends render HUD first, then Console (so the console overlays the HUD). The HUD can be forced off while the console is visible, depending on connection state.

## 2) Core API surface

### `UiSystem` (`src/game/ui/core/system.*`)
- Owns the chosen backend (created by `backend_factory.cpp`).
- On each update:
  - Reads `ConfigStore::Revision()` and updates HUD visibility flags from config.
  - Sets `hudModel.visibility.hud` based on connection + console visibility.
  - Sends the model to the backend and calls `backend->update()`.
- Optional parity tooling:
  - `ui.Validate` (config) enables HUD visibility self-checks per backend.
  - `--ui-smoke` (client CLI) toggles HUD elements on a timer for quick manual parity checks.

### `ui::Backend` (`src/game/ui/core/backend.hpp`)
Key responsibilities:
- Console interface access: `console()` returns `ui::ConsoleInterface`.
- Receives HUD state: `setHudModel(const ui::HudModel &)`.
- Exposes render output: `getRenderOutput()` and `getRenderBrightness()`.
- Handles input events and UI update loop (driven by `EngineApp`).

### `ui::ConsoleInterface` (`src/game/ui/console/console_interface.hpp`)
- Abstracts console operations (show/hide, list options, refresh requests, selections, status).
- Both frontends implement this on their console view.

### `HudModel` (`src/game/ui/models/hud_model.hpp`)
- Shared model passed to both frontends.
- Includes HUD visibility flags + data (scoreboard, dialog, etc.).

## 3) Backend selection

- `src/game/ui/core/backend_factory.cpp` chooses ImGui or RmlUi based on build configuration.
- The rest of the engine only sees `UiSystem` / `ui::Backend`.

## 4) Rendering + render outputs

### ImGui path
- Uses `ui::RendererBridge` and `ui::UiRenderTargetBridge` to render into a texture.
- Window renderers live under `src/karma-extras/ui/window/imgui/renderer_{bgfx,diligent}.*`.
- `ImGuiBackend::getRenderOutput()` returns a valid texture + visibility when the console or HUD drew.

### RmlUi path
- Uses RmlUi `RenderInterface` implementations in `src/karma-extras/ui/window/rmlui/renderer_{bgfx,diligent}`.
- `RenderOutput` is provided by both frontends and is always composited by the renderer.

## 5) Input handling

- Each backend maps `window::Event` into its UI system.
- Mapping logic is duplicated between ImGui and RmlUi and should be unified (`TODO.md`).
- Gameplay input is suppressed when UI input is active (console visible or chat focused).
  - The UI still receives input, and “global” actions (chat/escape/quick quit/fullscreen) remain active.
  - See `src/game/engine/client_engine.cpp` for the input gating rules.

## 6) Config and persistence

**All UI config reads/writes must go through `ConfigStore`.**
- Access via `ConfigStore::Get/Set/Erase`.
- Use `ConfigStore::Revision()` to resync when config changes.

Important: UI code should not load or write JSON files directly (except the Start Server override file, which is intentionally left as-is for now).

## 7) Console vs HUD behavior

- **Console**: tabbed UI with panels implemented separately per frontend.
- **HUD**: overlay components (chat/radar/scoreboard/crosshair/fps/dialog).
- **Quick menu**: modal HUD overlay (resume/disconnect/quit), toggled by Escape.
- The HUD is suppressed behind the console when **not connected**.
- Crosshair is explicitly disabled while the console is visible to avoid a "white square" leak.

## 8) Frontend structure

### ImGui (`src/game/ui/frontends/imgui/`)
- `backend.*`:
  - ImGui frame lifecycle, input, render-to-texture, HUD/console draw order.
- `console/console.*`:
  - Owns tab layout and panel calls.
- `console/panels/*`:
  - Community, Start Server, Settings, Bindings, Documentation.
- `hud/*`:
  - HUD components (chat, radar, scoreboard, crosshair, fps, dialog).

### RmlUi (`src/game/ui/frontends/rmlui/`)
- `backend.*`:
  - RmlUi context, document load/reload, render loop.
- `console/console.*`:
  - Glue for panel instances and UI events.
- `console/panels/*`:
  - RmlUi panel classes and event listeners (panel_community, panel_settings, panel_bindings, etc.).
- `hud/*`:
  - RML-driven HUD elements (chat, radar, scoreboard, crosshair, dialog).

## 9) RML assets (RmlUi)

RmlUi layout and styles live under `data/client/ui/`:
- `console.rml`, `console.rcss`
- `console_panel_*.rml` + `console_panel_*.rcss`
- `hud.rml` and associated styles

Any frontend changes that add UI elements generally require editing these asset files.

## 10) Call-flow cheat sheet

### ImGui path (per frame)
1. `UiSystem::update()` updates `HudModel` and calls `ImGuiBackend::setHudModel()`.
2. `ImGuiBackend::update()`:
   - Converts `window::Event` -> ImGui input.
   - Begins ImGui frame.
   - Draws HUD first (if visible), then Console.
   - Renders to UI render target via `RendererBridge`.
3. Renderer composites `ui::RenderOutput` texture onto the frame.
   - bgfx UI overlay uses premultiplied blending (`ONE / INV_SRC_ALPHA`).

### RmlUi path (per frame)
1. `UiSystem::update()` updates `HudModel` and calls `RmlUiBackend::setHudModel()`.
2. `RmlUiBackend::update()`:
   - Converts `window::Event` -> RmlUi input.
   - Updates HUD model + console state.
   - Renders RmlUi documents via its render interface.
3. Renderer composites `RenderOutput` from RmlUi.

### Config change propagation
1. `ConfigStore` revision increments on any Set/Erase/Replace.
2. `UiSystem` sees revision change and updates `HudModel.visibility.*`.
3. Settings panels read `ConfigStore` in their own update loops and resync UI state.

## 11) Hot spots + gotchas

- **HUD visibility logic** lives in `UiSystem::update` (depends on connection + console visibility).
- **Crosshair leak** is handled by disabling crosshair when console is visible.
- **RmlUi output texture** is expected to be available across BGFX/Diligent.
- **ConfigStore** is authoritative; do not read/write JSON files in UI code.
- **Start Server panels** still write a JSON override file for spawned servers; this is an intentional exception for now.

## 12) Refactor status (short)

Most of the original refactor plan is now implemented and tracked in `src/game/ui/TODO.md`:
- Shared input mapping layer (both frontends).
- Typed UI config facade around ConfigStore.
- Renderer-agnostic models + controllers for HUD/Console/Settings/Bindings.
- Unified render-to-texture output for both ImGui and RmlUi.
- Shared font loading logic.
- Cleanup of duplicated helpers.

## 13) "Where do I start?"

Recommended read order:
1) `src/game/ui/core/system.cpp` + `src/game/ui/core/backend.hpp`
2) `src/game/ui/frontends/imgui/backend.cpp`
3) `src/game/ui/frontends/rmlui/backend.cpp`
4) `src/game/ui/frontends/imgui/console/console.cpp`
5) `src/game/ui/frontends/rmlui/console/console.cpp`
6) `src/game/ui/TODO.md`
