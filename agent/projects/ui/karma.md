# UI Karma (m-karma Overlay Substrate)

## Project Snapshot
- Current owner: `codex`
- Status: `in progress (overlay technique under active convergence)`
- Immediate next task: produce a concrete engine overlay contract map that `m-bz3/src/ui/*` and `ui/radar.md` can both consume without backend leakage.
- Validation gate: docs updates: `cd m-overseer && ./agent/scripts/lint-projects.sh`; implementation slices: `cd m-karma && ./abuild.py -c -d <build-dir> -b bgfx,diligent,imgui,rmlui` plus runtime smoke in `m-bz3`.

## Mission
Expose a workable, engine-owned overlay rendering/integration technique in `m-karma` so game/UI code in `m-bz3` can integrate HUD/console/radar behavior through one stable path.

## Foundation References
- `projects/ui.md`
- `projects/ui/bz3.md`
- `projects/ui/radar.md`
- `../docs/building.md`
- `../docs/testing.md`

## Why This Is Separate
This track owns engine substrate decisions and should not be blocked by game-level refactors, but it must remain compatible with `ui/bz3` and `ui/radar` requirements.

## Direction Lock
1. Engine owns UI lifecycle, backend wiring, render submission, and diagnostics.
2. Game code must not directly depend on renderer-backend implementation details.
3. Overlay composition must support both ImGui and RmlUi.
4. Radar and non-radar HUD overlays must use shared engine substrate rather than one-off integration paths.

## End Goal (Meet-In-The-Middle)
`m-karma` provides one backend-neutral overlay integration contract that:
- supports text/panel-style overlays and richer frontend UI composition,
- supports renderer-produced external texture presentation for radar and similar widgets,
- can be consumed by refactored `m-bz3/src/ui/*` code with minimal game-side adapter glue.

## Owned Paths
- `m-karma/include/karma/ui/*`
- `m-karma/src/ui/*`
- `m-karma/include/karma/renderer/*` (overlay-facing contracts only)
- `m-karma/src/renderer/*` (overlay integration seams only)

## Interface Boundaries
- Inputs consumed:
  - backend capability constraints from `m-karma` renderer/UI implementations
  - game-side requirements from `projects/ui/bz3.md` and `projects/ui/radar.md`
- Outputs exposed:
  - stable engine-level overlay contract and lifecycle hooks
  - backend-neutral overlay/external-texture composition path
- Coordinate before changing:
  - `m-bz3/src/ui/*`
  - `projects/ui/bz3.md`
  - `projects/ui/radar.md`
  - `projects/ui.md`

## Current State
- Existing overlay bring-up has focused on parity slices and backend wiring behavior.
- `m-bz3` now contains imported legacy UI files that still need adaptation to the final engine contract.
- Radar infrastructure requirements are now explicitly coupled to this track via `ui/radar.md`.

## Gaps
1. Final engine overlay contract is not yet locked across all consumers.
2. External texture presentation path must be validated for both UI backends.
3. Integration guidance for imported `m-bz3/src/ui/*` code is not yet fully codified.

## Execution Plan
### E1: Contract Inventory
- enumerate current engine UI/renderer overlay seams and identify missing generic primitives.

### E2: Contract Lock Proposal
- produce a minimal contract proposal shared with `ui/bz3` and `ui/radar`.

### E3: Bounded Engine Slice
- implement one minimal substrate slice that unblocks both non-radar and radar overlays.

### E4: Cross-Project Validation
- verify the slice against both UI backends and both renderer backends.

## Non-Goals
- Do not move game semantics into engine UI contracts.
- Do not build one-off radar-only APIs in engine code.
- Do not reintroduce `m-dev` bridge coupling as a permanent architecture.

## Validation
From `m-karma/` and `m-bz3/` as needed:

```bash
cd m-karma
./abuild.py -c -d <build-dir> -b bgfx,diligent,imgui,rmlui

cd m-bz3
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
- `render.system`

## Current Status
- `2026-02-22`: this doc was realigned under `ui.md` to enforce explicit compatibility with `ui/bz3` and `ui/radar` end goals.

## Open Questions
- Should external-texture composition be a first-class overlay command or an explicit UI-backend bridge surface?
- Where should engine-owned offscreen-pass descriptors live to keep game call sites clean?
- What is the smallest stable API that keeps room for future overlay widgets beyond radar?

## Handoff Checklist
- [ ] Engine contract proposal is documented and reviewed against `ui/bz3` + `ui/radar` needs.
- [ ] One bounded substrate slice is implemented and validated.
- [ ] Cross-track docs are updated in the same handoff.
- [ ] Risks and unresolved design questions are carried forward.
