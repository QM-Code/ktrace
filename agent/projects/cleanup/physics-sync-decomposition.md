# Cleanup S4 (`CLN-S4`): Physics Sync Decomposition

## Project Snapshot
- Current owner: `specialist-cln-s4`
- Status: `in progress (S4-1 complete: classification-only scaffolding + typed decision skeleton landed with behavior/trace parity preserved)`
- Immediate next task: execute `S4-2` by extracting reconcile/apply update flow from `ecs_sync_system` into cohesive helpers while preserving runtime outcomes and diagnostics.
- Validation gate: `cd m-karma && ./abuild.py -c -d <karma-build-dir> -b jolt,physx` plus `./scripts/test-engine-backends.sh <karma-build-dir>` and targeted parity tests.

## Mission
Reduce complexity in `ecs_sync_system` by splitting concerns and replacing boolean-heavy classification APIs with explicit typed state/decision objects.

## Foundation References
- `projects/cleanup.md`
- `m-karma/src/physics/sync/ecs_sync_system.cpp`
- `m-karma/src/physics/sync/ecs_sync_system.hpp`

## Why This Is Separate
This is high-complexity physics-internal refactoring that can run in parallel with server runtime and factory standardization work.

## Owned Paths
- `m-karma/src/physics/sync/*`
- `m-overseer/agent/projects/cleanup/physics-sync-decomposition.md`

## Interface Boundaries
- Inputs consumed:
  - runtime command and ECS sync contracts from parity tests.
- Outputs exposed:
  - smaller cohesive modules and clearer decision surfaces.
- Coordinate before changing:
  - `projects/cleanup/physics-parity-suite.md`

## Non-Goals
- Do not alter high-level physics backend selection behavior.
- Do not collapse trace diagnostics needed by parity checks.

## Validation
```bash
cd m-karma
./abuild.py -c -d <karma-build-dir>
ctest --test-dir <karma-build-dir> -R "physics_backend_parity_.*" --output-on-failure
```

## Trace Channels
- `physics.sync`
- `physics.sync.trace`
- `cleanup.s4`

## Build/Run Commands
```bash
cd m-karma
./abuild.py -c -d <karma-build-dir>
```

## Current Status
- `2026-02-21`: `ecs_sync_system` complexity identified as major `P1` target.
- `2026-02-22`: moved under cleanup superproject child structure.
- `2026-02-22`: completed `S4-1` in `m-karma/src/physics/sync/ecs_sync_system.cpp` by extracting runtime-command classification scaffolding (`RuntimeCommandIntentFlags`) and introducing typed decision input (`RuntimeCommandTraceDecisionInput`) routed through classifier helpers with no intended reconcile/apply/trace behavior change.
- `2026-02-22`: validation passed in `build-cln-s4` via `./abuild.py -c -d build-cln-s4 -b jolt,physx`, `./scripts/test-engine-backends.sh build-cln-s4`, and `ctest --test-dir build-cln-s4 -R "physics_backend_parity_.*" --output-on-failure`.

## Open Questions
- What is the minimum typed decision model that preserves current trace classifier semantics?
- Which decomposition boundary minimizes merge conflict with parity test work?

## Handoff Checklist
- [x] `S4-1`: runtime-command classification scaffolding extracted in `ecs_sync_system` with typed decision input skeleton and zero intended behavior change.
- [ ] `S4-2`: reconcile/apply extraction plan implemented (rebuild/mutation/fallback flow split into cohesive helpers).
- [x] Physics parity tests remain green for the `S4-1` validation run.
