# UI Game Alignment (Staged Import)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (staged import landed; not wired into build/runtime)`
- Source baseline: `m-dev/src/game/ui/` imported into `m-rewrite/src/game/ui/` (excluding `bridges/`)
- Immediate next task: execute `G0` classification pass by tagging each imported subtree as `engine-candidate`, `game-owned`, or `backend-coupled`, and record dependency blockers.
- Validation gate: `./docs/scripts/lint-project-docs.sh` (this track is planning/staging only until explicit compile wiring is approved).

## Mission
Prepare the imported `m-dev` game UI tree for rewrite alignment while renderer/backend glue work is still stabilizing.

This track is explicitly a staging/alignment track:
- import now,
- organize and classify now,
- do **not** require immediate runtime functionality,
- do **not** wire into build/runtime until alignment checkpoints are complete.

## Foundation References
- `docs/foundation/policy/rewrite-invariants.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/projects/ui-engine.md`
- `docs/projects/gameplay-migration.md`
- `docs/projects/radar.md`

## Why This Is Separate
- `ui-engine.md` is currently focused on active parity slices in existing rewrite UI runtime paths.
- This document tracks a broader staged import/alignment of legacy UI code that is intentionally non-functional at first.
- Keeping this separate prevents mixed goals (parity bug-fix slices vs large non-wired codebase staging) in one project file.

## Imported Baseline (2026-02-18)
- Imported source root: `m-dev/src/game/ui/`
- Staged destination: `m-rewrite/src/game/ui/`
- Explicitly excluded during copy: `m-dev/src/game/ui/bridges/`
- Imported files: `154` (`157` total in source minus `3` under `bridges/`)

## Owned Paths
- `docs/projects/ui-game.md`
- `docs/projects/ASSIGNMENTS.md`
- `src/game/ui/*` (staged tree imported from `m-dev`)

## Interface Boundaries
- Inputs consumed:
  - staged source behavior/structure from `m-dev/src/game/ui/*`
  - active rewrite UI contracts from `include/karma/ui/*` and `src/engine/ui/*`
- Outputs exposed:
  - alignment documentation and staged directory organization only (until compile wiring is explicitly approved)
- Coordinate before changing:
  - `src/engine/CMakeLists.txt`
  - `src/game/CMakeLists.txt`
  - `include/karma/ui/*`
  - `src/engine/ui/*`
  - `docs/projects/ui-engine.md`

## Direction Lock
1. This track does **not** require the imported code to build/run yet.
2. Do not wire `src/game/ui/*` into CMake targets until `G0-G2` are complete and approved.
3. Preserve clear ownership boundaries:
   - engine-owned lifecycle/backend substrate remains in `src/engine/ui/*`,
   - gameplay semantics remain game-owned.
4. Keep imports diff-friendly: no broad rewrites before dependency classification.
5. Future wiring must align to the current engine UI surface in `include/karma/ui/ui_draw_context.hpp`:
   - `addImGuiDraw(...)` for ImGui callbacks,
   - `addRmlUiDraw(...)` for RmlUi callbacks,
   - `addTextPanel(...)` for backend-agnostic panel fallback/presentation.
   Imported `src/game/ui/*` code should target these contracts (or approved successors) rather than legacy bridge APIs.

## Directory Classification Baseline
Initial posture for staged alignment:

| Directory | Initial classification | Notes |
|---|---|---|
| `src/game/ui/config` | `engine-candidate` | Mostly generic config wrappers/settings models; validate key-name parity with rewrite config schema. |
| `src/game/ui/console` | `mixed` | Largely generic interfaces/types, but includes game-semantic console concepts and actions. |
| `src/game/ui/controllers` | `mixed` | Mostly generic controllers; `bindings` path depends on game input definitions. |
| `src/game/ui/core` | `mixed` | Key seam layer; requires rewrite contract adaptation and backend bridge decisions. |
| `src/game/ui/fonts` | `engine-candidate` | Generic font selection/resolution helpers, pending rewrite asset-key alignment. |
| `src/game/ui/frontends` | `backend-coupled` | Long-term glue/migration lane for ImGui/RmlUi integration. |
| `src/game/ui/models` | `mostly engine-candidate` | Mostly data models; a few legacy type dependencies require decoupling. |

## Execution Plan

### G0: Classification and Dependency Ledger
- Walk each imported subtree and label file-level ownership (`engine-candidate`, `game-owned`, `backend-coupled`).
- Record direct dependencies that currently block clean alignment:
  - `karma_extras/*` includes,
  - `game/input/*` dependencies,
  - any non-rewrite config/key assumptions.
- Acceptance:
  - explicit ledger added to this doc with blockers per subtree.

### G1: Include/Namespace Alignment (No Wiring)
- Normalize include paths and namespace assumptions so imported code can coexist in rewrite tree.
- Avoid behavior rewrites; focus on mechanical compatibility prep.
- Acceptance:
  - import tree no longer depends on obviously invalid path roots from `m-dev`.

### G2: Dependency Decoupling Prep
- Define replacement seams for legacy dependencies (`karma_extras` types, input-binding helpers, config wrappers).
- Introduce minimal rewrite-local shims/interfaces where required, still without runtime wiring.
- Acceptance:
  - each known blocker has an explicit replacement seam and owner path.

### G3: Ownership Split Proposal
- Produce target placement map:
  - what should remain game-owned under `src/game/ui/*`,
  - what should move to engine-owned shared UI support.
- Acceptance:
  - approved move map and sequencing order for follow-on slices.

### G4: Incremental Wiring Plan (Future Track Entry)
- Define the first compile activation slice and its required validations.
- Keep this phase blocked until backend glue stabilization and `ui-engine.md` coordination.
- In the first bounded wiring slice, require an explicit contract map from imported `src/game/ui/*` output paths to
  `UiDrawContext` (`addImGuiDraw`, `addRmlUiDraw`, `addTextPanel`) and document any adapter shims needed.
- Acceptance:
  - first bounded compile/wiring slice packet is ready.

## Non-Goals
- Do not require functional UI parity in this track.
- Do not expand renderer/backend stabilization scope here.
- Do not replace current rewrite runtime UI paths during staging.
- Do not force immediate relocation of all staged files into `src/engine/ui/*`.

## Validation
From `m-rewrite/`:

```bash
# Structural docs gate
./docs/scripts/lint-project-docs.sh

# Import sanity checks
find src/game/ui -path '*/bridges/*' -type f
find src/game/ui -type f | wc -l
```

## Current Status
- `2026-02-18`: staged import completed from `m-dev/src/game/ui/*` into `m-rewrite/src/game/ui/*`, excluding `bridges/`.
- `2026-02-18`: no build/runtime wiring performed in this slice (intentional).
- `2026-02-18`: project file created to track alignment work before backend-glue integration.

## Open Questions
- Should the final steady-state home for shared generic UI support remain under `src/game/ui/*` or move to engine-owned paths after classification?
- Which console/community/server flows should stay explicitly game-owned vs abstracted behind generic UI contracts?
- How much of `frontends/*` should be preserved as reference-only vs incrementally adapted into rewrite runtime backends?

## Handoff Checklist
- [x] staged import completed (excluding `bridges/`)
- [x] project doc created with explicit non-functional staging scope
- [x] `docs/projects/ASSIGNMENTS.md` row updated in same handoff
- [ ] file-level classification ledger completed (`G0`)
- [ ] dependency replacement seams documented (`G2`)
