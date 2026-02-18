# UI Engine

## Project Snapshot
- Current owner: `codex`
- Status: `in progress (HUD/console + chat-entry/input-focus parity slices landed)`
- Immediate next task: add one follow-up console parity slice for escape/focus-release ordering without expanding backend-specific surface area.
- Validation gate: `./abuild.py -c -d <build-dir> -b bgfx,diligent,imgui,rmlui` and client runs with explicit renderer/ui CLI overrides.

## Mission
Port and stabilize BZ3 UI behavior in `m-rewrite` while preserving engine ownership of frame lifecycle and backend boundaries.

## UI Policy (Locked)
1. Engine supports both `imgui` and `rmlui`.
2. Game code intentionally chooses a primary UI backend.
3. Engine owns lifecycle, backend wiring, render submission, and diagnostics.
4. Game owns UI content/state mapping and gameplay-to-UI translation.

## Owned Paths
- `m-rewrite/include/karma/ui/backend.hpp`
- `m-rewrite/src/engine/ui/backends/driver.hpp`
- `m-rewrite/src/engine/ui/system.cpp`
- `m-rewrite/src/engine/ui/backends/*`
- `m-rewrite/include/karma/ui/*`
- `m-rewrite/src/game/*` (UI call sites only)

## Layout + Naming Convention (Current)
- Top-level UI runtime plumbing lives at:
  - `m-rewrite/include/karma/ui/backend.hpp` (public backend selection API)
  - `m-rewrite/src/engine/ui/backends/driver.hpp` (internal backend driver contract)
  - `m-rewrite/src/engine/ui/system.cpp`
- Backend-specific implementations live under:
  - `m-rewrite/src/engine/ui/backends/software/*`
  - `m-rewrite/src/engine/ui/backends/imgui/*`
  - `m-rewrite/src/engine/ui/backends/rmlui/*`
- Use directory-scoped filenames with no redundant `ui_` prefixes.
- Inside backend directories, do not repeat backend names in filenames (for example use `backend.cpp`, `input.cpp`, `markup.cpp`, `cpu_renderer.cpp`, `system_interface.cpp`, `stub.cpp`).
- Public `UiSystem` header remains boundary-clean and does not expose backend driver internals.

## Interface Boundaries
- Inputs: engine frame events + game UI intent.
- Outputs: backend UI frame output and overlay presentation.
- Public backend override/config surface is `karma::ui::backend::BackendKind` (`imgui`, `rmlui`; `software` is internal fallback policy).
- Coordinate before changing:
  - `m-rewrite/src/engine/renderer/*` (overlay integration)
  - `m-rewrite/src/game/client/game/*` and related game UI call sites

## Current State (Implemented)
1. Engine-owned UI system lifecycle is integrated in client engine tick flow (`karma::app::client::Engine`).
2. Runtime UI backend selection via config + CLI is working.
3. ImGui and RmlUi software bridge paths are integrated.
4. Cross-backend overlay behavior is functioning on BGFX/Diligent baseline paths.
5. Game-side HUD/console state mapping now includes parity slices from `m-dev`: HUD visibility follows `connected || !consoleVisible`, console toggles/closes via global actions, and gameplay input is suppressed while console/chat-entry focus state is active.
6. UI source layout has been normalized to backend subdirectories with directory-scoped filenames (no `ui_` filename prefix convention).

## Current Gaps
1. Full HUD/console parity with `m-dev` is incomplete.
2. UI asset/service interfaces can be tightened for cleaner game-side usage.
3. Some backend-specific parity polish remains (layout/visual behavior edge cases).

## Execution Order
1. Select one concrete UI parity slice.
2. Implement it through engine-owned lifecycle hooks.
3. Validate on both render backends for chosen UI backend.
4. Expand to next slice without broad refactors.

## Validation
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent,imgui,rmlui
./<build-dir>/bz3 --backend-render bgfx --backend-ui imgui
./<build-dir>/bz3 --backend-render diligent --backend-ui imgui
./<build-dir>/bz3 --backend-render bgfx --backend-ui rmlui
./<build-dir>/bz3 --backend-render diligent --backend-ui rmlui
```

## Trace Channels
- `ui.system`
- `ui.system.overlay`
- `ui.system.imgui`
- `ui.system.rmlui`

## Handoff Checklist
- [x] UI behavior change scoped to one parity slice.
- [x] Engine/game boundary preserved.
- [x] Multi-backend validation recorded.
- [x] Docs updated with new policy/behavior if changed.

## Status/Handoff Notes (2026-02-10)
- Slices landed: HUD/console + chat-entry/input-focus parity mapping in `m-rewrite/src/game/client/game/*` + `m-rewrite/src/game/game.hpp`.
- Behavior landed: console toggles with global `console` action (grave by default).
- Behavior landed: escape (`quickMenu` action) closes console if open.
- Behavior landed: `chat` action opens console (if needed) and enters chat-entry focus state.
- Behavior landed: HUD presentation rule matches `m-dev` slice target (`connected || !consoleVisible`).
- Behavior landed: spawn/fire gameplay actions are suppressed while console/chat-entry focus state is active.
- Implementation note: presentation stays backend-policy-safe via engine-owned lifecycle (`onUiUpdate`) and backend-agnostic `UiDrawContext::TextPanel`.

Validation command matrix (required now):
- `cd m-rewrite && ./abuild.py -c -d <build-dir> -b bgfx,diligent,imgui,rmlui`
- `cd m-rewrite && timeout 20s ./<build-dir>/bz3 --backend-render bgfx --backend-ui imgui`
- `cd m-rewrite && timeout 20s ./<build-dir>/bz3 --backend-render diligent --backend-ui imgui`
- `cd m-rewrite && timeout 20s ./<build-dir>/bz3 --backend-render bgfx --backend-ui rmlui`
- `cd m-rewrite && timeout 20s ./<build-dir>/bz3 --backend-render diligent --backend-ui rmlui`

Remaining risks/open questions:
- This slice uses text-panel presentation plus game-side focus state as a bounded interim step; full text-entry widget parity and submitted chat message path are still pending.
- UI capture policy remains config-gated (`ui.captureInput`); if disabled, backend capture signals do not gate camera input.
