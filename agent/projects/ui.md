# UI Superproject Alignment

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (orchestration + research kickoff)`
- Immediate next task: lock a shared overlay contract matrix that maps engine substrate (`ui/karma.md`), game adoption (`ui/bz3.md`), and radar delivery (`ui/radar.md`).
- Validation gate: `cd m-overseer && ./agent/scripts/lint-projects.sh`.

## Mission
Coordinate all HUD/console/radar work so `m-karma` and `m-bz3` converge on one compatible overlay architecture.

This is the parent orchestration track for:
- `projects/ui/karma.md` (`m-karma` overlay substrate + lifecycle ownership)
- `projects/ui/bz3.md` (`m-bz3` HUD/console import + refactor/adaptation)
- `projects/ui/radar.md` (engine substrate + game radar semantics + HUD composition)

## Foundation References
- `projects/ui/karma.md`
- `projects/ui/bz3.md`
- `projects/ui/radar.md`
- `../docs/building.md`
- `../docs/testing.md`

## Why This Is Separate
Each subproject can make local progress, but architectural drift between them would create rework.
This parent project exists to enforce one end-state contract and prevent divergent UI integration strategies.

## Alignment Contract (Direction Lock)
1. `m-karma` owns overlay lifecycle, render pass orchestration, backend abstraction, and diagnostics.
2. `m-bz3` owns HUD/console/radar semantics, gameplay state mapping, and UI behavior policy.
3. Both ImGui and RmlUi must remain first-class paths; no one-backend-only solution is acceptable.
4. Radar remains a game concept in `m-bz3`, but uses engine-provided generic offscreen/render-target/UI plumbing.
5. Legacy `m-dev` imports must be refactored toward the engine contract rather than restoring legacy bridge coupling.

## Interface Boundaries
- Inputs consumed:
  - implementation updates from `ui/karma.md`, `ui/bz3.md`, and `ui/radar.md`
  - architecture references from `q-karma` and parity references from `m-dev`
- Outputs exposed:
  - shared architecture decisions and sequencing constraints
  - compatibility acceptance criteria used by all UI subprojects
- Coordinate before changing:
  - `projects/ASSIGNMENTS.md`
  - `projects/ui/karma.md`
  - `projects/ui/bz3.md`
  - `projects/ui/radar.md`

## Milestones
### U0: Orchestration Baseline (this slice)
- Create/maintain this parent doc.
- Ensure all child docs explicitly state mutual-compatibility end goals.
- Acceptance:
  - child docs reference this track and each other where required.

### U1: Overlay Contract Lock
- Define the minimal stable engine/game UI contract surface for:
  - generic overlay draws,
  - external texture presentation,
  - backend-neutral scheduling.
- Acceptance:
  - all three child docs use the same contract terms.

### U2: Substrate + Adoption Convergence
- Track delivery of engine substrate slices (`ui/karma`, `ui/radar`) and game adaptation slices (`ui/bz3`).
- Acceptance:
  - no active blocker that requires bypassing the shared contract.

### U3: End-to-End HUD/Console/Radar Validation
- Require both renderer backends and both UI backends for runtime smoke.
- Acceptance:
  - one documented matrix proving HUD, console, and radar presentation through the agreed path.

## Non-Goals
- Do not implement large code slices directly in this parent track.
- Do not duplicate execution details owned by child project docs.
- Do not allow one child track to redefine architecture without cross-track updates.

## Validation
```bash
cd m-overseer
./agent/scripts/lint-projects.sh
```

## Trace Channels
- `ui.system`
- `ui.system.overlay`
- `ui.system.imgui`
- `ui.system.rmlui`
- `render.system`
- `game.radar`

## Current Status
- `2026-02-22`: parent UI orchestration track created.
- `2026-02-22`: child project set expanded to `ui/karma`, `ui/bz3`, and `ui/radar` with explicit convergence requirement.

## Open Questions
- What is the minimal engine overlay API that remains stable while `m-bz3/src/ui/*` is refactored?
- Should overlay texture presentation be a first-class command in the same queue as text/panel draws, or a separate bridge layer?
- What validation matrix is sufficient before declaring HUD/console/radar convergence complete?

## Handoff Checklist
- [ ] Child project docs remain aligned with this contract.
- [ ] Assignment board reflects current owners, status, and next tasks.
- [ ] Cross-track blockers are explicitly recorded.
- [ ] Any architecture decision changes are propagated to all three child docs.
