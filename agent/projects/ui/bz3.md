# UI BZ3 Alignment (m-bz3 HUD/Console Adoption)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (legacy UI imported; adaptation not yet wired)`
- Immediate next task: execute `G0` file-level classification and dependency ledger for `m-bz3/src/ui/*`, then map each class to the shared engine contract in `ui/karma.md`.
- Validation gate: planning/docs slices: `cd m-overseer && ./agent/scripts/lint-projects.sh`; compile/wiring slices: `cd m-bz3 && ./abuild.py -c -d <build-dir> -b bgfx,diligent,imgui,rmlui`.

## Mission
Refactor and integrate imported legacy HUD/console code in `m-bz3/src/ui/*` so it cleanly targets the engine-owned overlay substrate from `m-karma` while preserving game-owned behavior.

## Foundation References
- `projects/ui.md`
- `projects/ui/karma.md`
- `projects/ui/radar.md`
- `../docs/building.md`
- `../docs/testing.md`

## Why This Is Separate
This track is a game-facing migration/refactor effort and should not redefine engine contracts, but it must continuously converge with engine substrate and radar integration requirements.

## Direction Lock
1. Imported legacy code is staging input, not architectural truth.
2. Final HUD/console behavior remains game-owned in `m-bz3`.
3. Backend-specific integration points must route through engine contracts, not direct backend calls from gameplay/UI domain code.
4. Radar must appear as a normal HUD item from the game developer point of view.

## End Goal (Meet-In-The-Middle)
`m-bz3/src/ui/*` is reorganized and refactored so HUD/console/radar presentation uses one shared engine overlay API path, while retaining game-owned semantics and feature behavior.

## Imported Baseline
- Source: `m-dev/src/game/ui/`
- Destination: `m-bz3/src/ui/`
- Intent: staged intake for controlled adaptation, not direct runtime resurrection.

## Owned Paths
- `m-bz3/src/ui/*`
- `m-bz3/src/game/*` (HUD/console integration call sites)
- `m-overseer/agent/projects/ui/bz3.md`
- `m-overseer/agent/projects/ASSIGNMENTS.md`

## Interface Boundaries
- Inputs consumed:
  - engine UI contracts from `m-karma`
  - legacy behavior/parity references from `m-dev`
  - radar composition requirements from `projects/ui/radar.md`
- Outputs exposed:
  - game-owned HUD/console/radar behavior wired through the shared engine substrate
  - migration notes/blockers that may require engine contract adjustments
- Coordinate before changing:
  - `projects/ui/karma.md`
  - `projects/ui/radar.md`
  - `projects/ui.md`
  - `m-karma/include/karma/ui/*`
  - `m-karma/src/ui/*`

## Execution Plan
### G0: Classification + Dependency Ledger
- Tag each imported subtree/file as `engine-candidate`, `game-owned`, `backend-coupled`, or `mixed`.
- Record blockers (`karma_extras` deps, old input/config seams, backend-specific assumptions).

### G1: Mechanical Compatibility Prep
- Normalize includes/namespaces/path assumptions to current repo layout without behavior rewrites.

### G2: Adapter Surface Definition
- Define minimal game-side adapters from imported UI modules to current engine overlay contracts.

### G3: Incremental Runtime Wiring
- Activate one bounded vertical slice (console/HUD) through the new contract path.

### G4: Hardening and Cleanup
- Remove dead legacy bridge paths and lock post-migration ownership boundaries.

## Non-Goals
- Do not force a wholesale rewrite in one pass.
- Do not bypass engine lifecycle/overlay contracts.
- Do not embed backend API types into game UI domain modules.

## Validation
From `m-bz3/` when compile wiring starts:

```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent,imgui,rmlui

# Runtime smoke matrix (as slices become runnable)
timeout 20s ./<build-dir>/bz3 --backend-render bgfx --backend-ui imgui
timeout 20s ./<build-dir>/bz3 --backend-render diligent --backend-ui imgui
timeout 20s ./<build-dir>/bz3 --backend-render bgfx --backend-ui rmlui
timeout 20s ./<build-dir>/bz3 --backend-render diligent --backend-ui rmlui
```

## Trace Channels
- `ui.system`
- `ui.system.overlay`
- `ui.system.imgui`
- `ui.system.rmlui`
- `game.ui`
- `game.radar`

## Current Status
- `2026-02-22`: doc realigned to explicit convergence with `ui/karma` and `ui/radar` under `ui.md`.
- `2026-02-22`: import remains staged; integration wiring still pending by design.

## Open Questions
- Which imported modules should stay game-owned long-term vs move to shared engine support?
- What is the thinnest adapter layer that can preserve behavior while removing backend coupling?
- In what order should console, HUD, and radar slices be wired to minimize churn?

## Handoff Checklist
- [ ] File-level classification ledger completed and current.
- [ ] Adapter seams are documented against current engine contracts.
- [ ] Bounded runtime wiring slice validated on both UI backends and renderer backends.
- [ ] Cross-track docs (`ui.md`, `ui/karma.md`, `ui/radar.md`) updated in same handoff.
