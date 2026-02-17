# UI Integration

## Project Snapshot
- Current owner: `unassigned`
- Status: `in progress`
- Immediate next task: define and land the next concrete HUD/console parity slice from `m-dev` while preserving the engine-owned UI lifecycle and backend policy.
- Validation gate: `./abuild.py -a` and run client across both render backends with selected UI backend(s).

This is the canonical project file for UI integration and overlay/backend policy.

## Newcomer Read Order
1. Read this top section for assignment context.
2. Read `## Engine vs Game UI Model`.
3. Read `## What Is Implemented Today`.
4. Read `## Current Gaps (Before Porting Full m-dev HUD/Console)`.
5. Execute from `## Immediate Next Steps (Foundation-First Sequence)`.

## Consolidation
This file supersedes:
- `docs/projects/ui-integration-playbook.md`
- `docs/projects/ui_overlay_track.md`

## Quick Start
1. Read `AGENTS.md`.
2. Read `docs/AGENTS.md`.
3. Use this file as the only project-level source for UI integration work.

## Note
The source-material sections below are preserved verbatim for no-loss migration context and may mention retired `*_track`/`*playbook` filenames and pre-refactor UI file naming (`ui_*` filenames before the `src/engine/ui/backends/*` layout).

---

## Source Material: ui-integration-playbook.md
# UI Integration Playbook (m-rewrite)

## Project Track Linkage
- Delegation track: `docs/projects/ui-integration.md`

## Purpose
This document defines the UI migration target and execution plan for `m-rewrite`.

Primary goal:
- Port the real BZ3 HUD/Console behavior from `m-dev` into `m-rewrite` while preserving the engine-owned lifecycle model.

Secondary goal:
- Keep enough UI infrastructure in `src/engine/` that game developers do not need direct BGFX/Diligent integration code.
- Keep UI backend choice explicit for game teams: use ImGui or RmlUi directly, not a forced pseudo-common UI DSL.

Non-goals:
- Blindly copying `KARMA-REPO` or `m-dev` internals.
- Re-introducing ad-hoc game-owned render loops.
- Exposing renderer-backend APIs directly to game UI code.

---

## Project Context (What We Are Building)
`m-rewrite` is the production rewrite. It is not a continuation of proof-of-concept work.

Design stance:
- `KARMA-REPO` is a reference for ideas and lessons learned.
- `m-dev` is the reference for shipped behavior and feature parity targets.
- `m-rewrite` is a clean reimplementation under a correct subsystem order and engine-owned lifecycle.

Repository boundary target:
- `src/engine/` and `src/game/` are expected to become separately managed repositories in the future.
- Engine should be consumable as libraries + headers.
- Game should depend on engine APIs, not engine internals.
- Runtime data for rewrite binaries is rooted at `m-rewrite/data/`.

---

## Engine vs Game UI Model

## UI Backend Choice Policy (Important)
- Engine supports both ImGui and RmlUi because different teams prefer different UI paradigms.
- This dual support is a choice model, not a requirement that every game co-develop both backends.
- A production game is expected to pick one backend and build UI directly for it.
- The engine should hide renderer boilerplate and lifecycle complexity, but it does not need to hide whether the UI backend is ImGui or RmlUi.
- Supporting both in the same game tree remains valid for demos/reference parity, but is optional.
- This is intentionally different from platform/render/physics/audio backends, which remain engine-internal abstractions.

## Engine Responsibilities (`src/engine/`)
- Own frame lifecycle and call order.
- Own input collection and per-frame event fan-out.
- Own UI system lifecycle: init, begin frame, build output texture, submit overlay, shutdown.
- Own renderer/backend intersection required for UI render-to-texture.
- Own backend selection and fallback behavior.
- Own cross-backend parity (BGFX and Diligent should behave the same from game perspective).
- Own shared diagnostics and trace channels.

## Game Responsibilities (`src/game/`)
- Own game-specific UI content and state mapping.
- Own gameplay-to-UI data translation.
- Submit UI intent through engine-facing UI API (`UiDrawContext` and future higher-level API).
- Choose UI backend strategy for the game (`imgui` or `rmlui`) and implement UI directly against that frontend.
- Avoid touching renderer backend details directly.

Practical rule:
- If code is game-specific presentation/content, it stays in `src/game/`.
- If code is backend boilerplate/bridge/lifecycle that every game would need, it belongs in `src/engine/`.

---

## Engine-Owned UI Lifecycle (Current Flow)
Current `EngineApp::tick()` flow:
1. `window_->pollEvents()`
2. `ui_system_.beginFrame(dt, events)`
3. `input_system_.update(events)`
4. `game_->onUpdate(dt)`
5. `game_->onUiUpdate(dt, ui_system_.drawContext())`
6. `scene_.updateWorldTransforms()`
7. `ui_system_.update(*render_system_)`
8. `ui_system_.endFrame()`
9. `render_system_->beginFrame(...)`
10. `render_system_->renderFrame()`
11. `render_system_->endFrame()`

This is the intended Unity/Unreal/Godot-style ownership model:
- Game submits logic and content.
- Engine owns lifecycle timing and rendering submission.

---

## What Is Implemented Today

## UI backend architecture
- Engine UI system exists in `src/engine/ui/`.
- Runtime UI backend choice is supported:
  - config key: `ui.backend`
  - CLI override: `--backend-ui imgui|rmlui`
- Render backend choice is supported:
  - config key: `render.backend`
  - CLI override: `--backend-render auto|bgfx|diligent`

## Software bridge path (render-to-texture overlay)
- Both ImGui and RmlUi currently render into a CPU texture.
- Engine submits that texture on an in-world overlay quad (`kLayerUI`).
- Overlay material uses alpha blending.
- Overlay mesh includes both windings for backend cull tolerance.

## Current game-side integration
- Game currently submits a simple test/status panel via `UiDrawContext::TextPanel`.
- This proves:
  - game -> engine UI submission works,
  - backend switching works,
  - engine-owned overlay submission works.

## Recent parity/debugging outcomes
- BGFX world rendering artifact regression was fixed.
- RmlUi panel border sizing issue was fixed by explicit panel dimensions in generated markup.
- RmlUi background transparency issue was fixed by using byte alpha in `rgba(...)` for this path.
- Trace logging for UI is categorized and mostly non-spam by default.

---

## Current Gaps (Before Porting Full m-dev HUD/Console)
1. UI API is still too primitive for full HUD/Console (currently mostly text panel + callback hooks).
2. RmlUi integration uses a generated single-document debug panel path, not real game RML documents yet.
3. ImGui/RmlUi parity is proven at a basic level, but not yet validated for full gameplay UI feature set.
4. Input capture policy for gameplay-vs-UI interaction needs explicit mode rules for real console/HUD behavior.
5. Asset/config conventions for real UI bundles (RML/CSS/fonts/textures) need a finalized loading contract.

---

## Migration Target for UI
Target state:
- Game owns HUD/Console feature logic and screen composition intent.
- Engine owns UI runtime, backend bridges, overlay submission, and backend parity.
- Game can target a single chosen UI backend (ImGui or RmlUi) without being forced to maintain both.
- No direct BGFX/Diligent calls in game UI code.
- No `karma-extras` style detached bridge dependency; bridge lives in engine-side code.

Expected migration shape:
- Keep shared game UI models/controllers where useful, but do not require frontend-agnostic purity.
- Implement the chosen game frontend directly:
  - `game/ui/frontends/imgui/*` or
  - `game/ui/frontends/rmlui/*`
- Optional: maintain both frontends in parallel for demo/reference parity.
- Use engine UI services for frame lifecycle, event feed, and final texture submission.

---

## Immediate Next Steps (Foundation-First Sequence)
These are the next several concrete steps before full m-dev UI port.

1. Stabilize UI foundation contract in engine.
- Finalize `UiDrawContext` direction for non-trivial UI.
- Decide whether callback API needs typed backend context wrappers (preferred) vs pure high-level primitives.
- Freeze minimal engine-side UI services usable by either backend, without requiring a fully unified game-level UI API.

2. Add a small engine UI asset interface.
- Define how game asks for UI asset resolution (RML/CSS/font/image paths).
- Keep resolution rooted in existing config/data layering.
- Avoid backend-specific file-loading behavior in game code.

3. Build a minimal real UI vertical slice in game.
- Replace test panel with one simple but real screen slice (for example: top-left console card or small status widget).
- Implement it in the selected backend first (ImGui or RmlUi).
- Verify it across both render backends (BGFX and Diligent) for that selected UI backend.
- Optional: mirror it in the other UI backend as a reference/demo path.

4. Formalize input capture policy.
- Define rules for when UI captures mouse/keyboard.
- Ensure roaming/game controls and UI controls do not conflict.
- Add config toggle defaults that match expected gameplay flow.

5. Establish UI parity checklist and trace checklist.
- Required matrix for a chosen UI backend:
  - `bgfx + <chosen-ui>`
  - `diligent + <chosen-ui>`
- Optional extended matrix (engine/demo parity work):
  - `bgfx + imgui`
  - `bgfx + rmlui`
  - `diligent + imgui`
  - `diligent + rmlui`
- Trace channels for diagnosis:
  - `ui.system`
  - `ui.system.overlay`
  - `ui.system.imgui`
  - `ui.system.rmlui`
  - leaf frame channels only when needed.

6. Start controlled HUD/Console port from m-dev.
- Port shared UI models/controllers first.
- Port one panel at a time.
- Keep each port step parity-tested across BGFX/Diligent on the chosen UI backend before continuing.
- Optional: mirror-check the other UI backend for reference/demo parity.

---

## What To Reuse From m-dev vs Rewrite
Reuse from `m-dev`:
- Feature behavior and user-facing semantics.
- Shared UI state/model/controller logic where cleanly extractable.
- Known-good config keys and data conventions when still valid.

Rewrite in `m-rewrite`:
- Lifecycle ownership and call order.
- Backend bridges and renderer intersection.
- Any code that leaks renderer details into game space.
- Any code that relies on ad-hoc or unstable architecture patterns.

Decision rule:
- Preserve behavior, not structure.

---

## Testing and Validation Matrix
Every UI-facing change should be tested in:
1. `--backend-render bgfx --backend-ui <chosen-ui>`
2. `--backend-render diligent --backend-ui <chosen-ui>`
3. Optional extended parity:
- `--backend-render bgfx --backend-ui imgui`
- `--backend-render bgfx --backend-ui rmlui`
- `--backend-render diligent --backend-ui imgui`
- `--backend-render diligent --backend-ui rmlui`

Use traces to isolate divergence:
- `-v -t ui.system,ui.system.overlay,ui.system.imgui,ui.system.rmlui`
- Add leaf frame channels only when required.

Acceptance rule for each migration step:
- No backend-specific behavior differences unless explicitly documented and intentional.

---

## Definition of Done (UI Foundation)
Foundation is considered ready for broad HUD/Console migration when:
1. Engine UI API is stable and documented.
2. One non-trivial real UI slice runs in the chosen UI backend on both render backends.
3. Chosen-backend BGFX/Diligent render and interaction are correct.
4. Input capture policy is deterministic and validated.
5. Trace coverage is sufficient to debug backend divergence quickly.

---

## Handoff Notes for Future Agent
If you are picking up UI work next:
1. Start by running all four backend combinations and confirm baseline behavior.
2. Keep engine/game boundaries strict.
3. Prefer smallest vertical slices over broad partial ports.
4. Use trace-first debugging when parity breaks.
5. Do not reintroduce backend-specific code into `src/game/`.


---

## Source Material: ui_overlay_track.md

# UI Overlay Track

## Mission
Own UI overlay/system behavior for ImGui and RmlUi integration under engine-owned lifecycle.

## Primary Specs
- `docs/projects/ui-integration.md`
- `docs/projects/core-engine-infrastructure.md` (UI policy + boundaries)

## Why This Is Separate
UI backend work is largely independent from physics/audio/network internals when interface contracts are stable.

## Owned Paths
- `m-rewrite/src/engine/ui/*`
- UI-specific config usage in `m-rewrite/data/client/config.json`
- minimal game UI usage in `m-rewrite/src/game/game.cpp` (as needed)

## Interface Boundaries
- Inputs: UI backend selection and per-frame data.
- Outputs: UI rendering and overlay interaction.
- Coordinate before changing:
  - renderer integration points in `m-rewrite/src/engine/renderer/*`
  - `docs/projects/ui-integration.md`

## Non-Goals
- Protocol/network event semantics.
- Physics/audio backend internals.

## Validation
From `m-rewrite/`:

```bash
./abuild.py -a
./build-dev/bz3 --backend-render bgfx --backend-ui imgui
./build-dev/bz3 --backend-render diligent --backend-ui rmlui
```

## Trace Channels
- `ui.system`
- `ui.system.overlay`
- `ui.system.imgui`
- `ui.system.rmlui`

## Build/Run Commands
```bash
./abuild.py -a
```

## First Session Checklist
1. Read `docs/projects/ui-integration.md`.
2. Verify both UI backend paths at startup.
3. Implement scoped UI/backend lifecycle changes.
4. Validate both UI backends on at least one renderer each.
5. Update this project file status and handoff notes.

## Current Status
- See `Project Snapshot` at top of file for active owner and status.

## Open Questions
- See `Project Snapshot` and `Newcomer Read Order` at top of file.

## Handoff Checklist
- [ ] ImGui/RmlUi behavior validated.
- [ ] Engine-owned lifecycle constraints preserved.
- [ ] Any UI policy changes documented.
