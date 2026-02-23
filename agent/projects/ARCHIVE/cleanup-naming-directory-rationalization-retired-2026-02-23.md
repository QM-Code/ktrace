# Cleanup S10 (`CLN-S10`): Naming + Directory Rationalization

## Project Snapshot
- Current owner: `overseer`
- Status: `done (closeout completed by operator decision; no additional naming slices queued)`
- Immediate next task: none; reopen only if new cross-repo naming/path debt is explicitly prioritized.
- Validation gate: `cd m-bz3 && ./abuild.py -c -d <bz3-build-dir>` and `cd m-karma && ./abuild.py -c -d <karma-build-dir>`.

## Mission
Align naming and directory ownership with actual runtime/build responsibilities, reducing ambiguity and navigation cost.

## Foundation References
- `projects/cleanup.md`
- `m-bz3/cmake/targets/*`
- `m-bz3/src/client/game/game.hpp`
- `m-karma/cmake/40_sdk_subdir.cmake`
- `m-karma/cmake/sdk/*`

## Why This Is Separate
Naming/layout normalization is cross-cutting and often mechanical; isolating it prevents noise in behavior-changing tracks.

## Owned Paths
- `m-bz3/cmake/*`
- `m-bz3/src/client/*` (naming/layout only)
- `m-karma/cmake/*`
- `m-overseer/agent/projects/ARCHIVE/cleanup-naming-directory-rationalization-retired-2026-02-23.md`

## Interface Boundaries
- Inputs consumed:
  - structure decisions from active cleanup tracks.
- Outputs exposed:
  - consistent naming conventions and path ownership.
- Coordinate before changing:
  - `projects/cleanup.md`
  - `projects/ui.md`

## Non-Goals
- Do not combine behavior refactors with broad rename churn in one slice.
- Do not rename public SDK contracts without explicit migration decision.

## Validation
```bash
cd m-bz3
./abuild.py -c -d <bz3-build-dir>

cd ../m-karma
./abuild.py -c -d <karma-build-dir>
```

## Trace Channels
- `cleanup.s10`
- `build.layout`

## Build/Run Commands
```bash
cd m-bz3
./abuild.py -c -d <bz3-build-dir>
```

## Current Status
- `2026-02-21`: major ownership normalization landed (`src/game` and `src/engine` CMake ownership removed).
- `2026-02-22`: follow-on naming cleanup tracked as dedicated child lane under superproject.
- `2026-02-23`: closeout completed by operator direction; no additional naming/path normalization slices scheduled in cleanup scope.

## Open Questions
- none at closeout.

## Handoff Checklist
- [x] Remaining high-friction naming drift identified and prioritized.
- [x] Renames applied in behavior-neutral slices.
- [x] Build wiring and scripts updated for each rename wave.
