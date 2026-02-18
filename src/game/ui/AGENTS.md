# UI AGENTS.md

Read `src/AGENTS.md` and `src/game/AGENTS.md` before this file.

This file is UI-specific onboarding for future coding agents working under `src/game/ui/`.
It complements the repo-level AGENTS.md and focuses on how the UI subsystem is organized
and what to tackle next.

## Quick map (what lives where)

### Core UI entry points (`src/game/ui/core/`)
- `src/game/ui/core/system.*`:
  - Owns the high-level UI system.
  - Bridges config changes into HUD visibility (via `ConfigStore`).
  - Calls `backend->update()` each frame.
  - Key logic: HUD visible when connected OR console is hidden (see `UiSystem::update`).

- `src/game/ui/core/backend.hpp`:
  - Interface for UI backends.
  - Both ImGui and RmlUi implement this.
  - Exposes `console()`, `setHudModel()`, `getRenderOutput()`, etc.

- `src/game/ui/core/backend_factory.cpp`:
  - Picks ImGui or RmlUi backend at build time.

### Config + settings (`src/game/ui/config/`)
- `ui_config.*`: typed ConfigStore facade.
- `render_settings.*`: brightness state + dirty tracking.
- `hud_settings.*`: HUD visibility toggles (scoreboard/chat/radar/fps/crosshair).
- `render_scale.*`: UI render scale.
- `input_mapping.*`: shared input mapping.
- `config.*`: Start Server override config helpers.

### Frontend backends (ImGui + RmlUi)
- ImGui: `src/game/ui/frontends/imgui/`
  - `backend.*`: ImGui frame lifecycle, input processing, render-to-texture via ImGui render bridge.
  - `console/`: UI console panels and logic (community/settings/bindings/start server/docs).
  - `hud/`: HUD rendering (chat/radar/scoreboard/crosshair/fps).

- RmlUi: `src/game/ui/frontends/rmlui/`
  - `backend.*`: RmlUi context + doc loading + render loop.
  - `console/`: panels are class-based (`panel_*`), RML/RCSS templates live in `data/client/ui`.
  - `hud/`: RmlUi HUD document and components.

### Rendering bridges (BGFX/Diligent)
- ImGui render targets are provided by `src/karma-extras/ui/platform/imgui/renderer_*` and should stay out of core engine.
- RmlUi uses render interfaces in `src/karma-extras/ui/platform/rmlui/renderer_{bgfx,diligent}.*`.
- RmlUi renderers expose output textures for the shared render bridge; see `src/karma-extras/ui/bridges/` + RmlUi platform renderers.
- Backend-agnostic goal: avoid direct bgfx/diligent/jolt/physx usage in `src/game/`. UI thumbnail cache is the main remaining exception to remove later.

## Console vs HUD

- The **Console** is the in-game UI with tabs (Community, Start Server, Settings, Bindings, ?).
- The **HUD** is gameplay overlay (chat, radar, scoreboard, crosshair, fps, dialog, quick menu).
- The HUD draws beneath the console when connected; when not connected and the console is open,
  the HUD is hidden. This is enforced in `UiSystem::update` and backend draw logic.
- Crosshair is explicitly suppressed when the console is visible to avoid the “white square” leak.

## ConfigStore usage (important)

- All UI config reads/writes must go through `ConfigStore` (no direct JSON file I/O).
- The UI should only use:
  - `karma::common::config::ConfigStore::Get/Set/Erase` for config values.
  - `karma::common::config::ConfigStore::Revision()` to detect changes.
- Example usage is in:
  - RmlUi Settings panel (`panel_settings.cpp`)
  - ImGui Settings panel (`panel_settings.cpp`)
  - Console community credentials in both frontends.

## Current focus (from TODO)

- Radar flicker/missing blips across all backends (renderer-side investigation).
- Feature parity decisions for HUD + Settings/Bindings.
- UI smoke harness for quick regression checks.

## Files worth reading first

1) `src/game/ui/core/backend.hpp` and `src/game/ui/core/system.cpp` (core API + update rules)
2) `src/game/ui/frontends/imgui/backend.cpp` (ImGui render loop + HUD/console draw order)
3) `src/game/ui/frontends/rmlui/backend.cpp` (RmlUi context + HUD/console docs)
4) `src/game/ui/frontends/imgui/console/console.*` (ImGui console tabs, panels, state)
5) `src/game/ui/frontends/rmlui/console/console.*` (RmlUi console + panel wiring)
6) `src/game/ui/frontends/rmlui/console/panels/*` (panel behavior + ConfigStore usage)
7) `src/game/ui/TODO.md` (current refactor plan)

## Frontend-specific notes

### ImGui
- Rendering order: HUD then Console. Console visibility affects crosshair drawing.
- `console/console.cpp` owns tab layout; `panel_*` files implement UI.
- Community refresh should trigger on tab activation or click.
- Fonts: ImGui does **not** do runtime font fallback like RmlUi. Every font used
  for headings/body must have fallback glyphs merged into its atlas, or non-Latin
  text will render as `?`. Keep heading/title fonts merged with the same fallback
  glyph ranges as the regular font.

### RmlUi
- Document load order: HUD document is loaded before console to ensure HUD renders beneath.
- Panels are instantiated in `backend.cpp` and registered to console view.
- RML layout lives in `data/client/ui/*.rml` and styles in `*.rcss`.

### ImGui/RmlUi parity
- UI changes should be mirrored in **both** ImGui and RmlUi, in both directions.
- Exceptions are allowed only for backend-specific bugs or feature gaps.
- When in doubt about parity, ask before implementing.

## Known TODOs

- Remaining work is tracked in `src/game/ui/TODO.md`.

## Gotchas

- Console visibility and HUD visibility are deliberately coupled in `UiSystem::update`.
- ConfigStore has save/merge intervals and revision tracking; use `Revision()` to resync UI state.
- Start Server panels still write a JSON override file for server instances (left as-is for now).
- Radar uses an overlay render target; failures affect both ImGui/RmlUi equally.
- Radar visibility/orientation was recently stabilized (world mesh on radar + overlayed blips/FOV + horizontal flip); avoid undoing those changes unless you re-test both UI backends.
