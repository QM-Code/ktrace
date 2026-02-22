# Cleanup S4 (`CLN-S4`): Physics Sync Decomposition

## Project Snapshot
- Current owner: `specialist-cln-s4`
- Status: `in progress (S4-4 complete: boundary pass decided no additional shared fallback extraction; CLN-S4 implementation slices are closed behavior-neutrally)`
- Immediate next task: prepare CLN-S5 handoff package (stable seams + deferred-risk notes) for parity-suite decomposition work.
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
- `2026-02-22`: completed `S4-2` by extracting update-path reconcile/apply flow in `EcsSyncSystem::preSimulate` into cohesive local helpers (`reject/teardown`, `structural rebuild`, and runtime reconcile/apply), keeping `create_binding` structurally intact and preserving runtime-command apply/recovery ordering and trace semantics.
- `2026-02-22`: `S4-2` validation passed in `build-cln-s4` via `./abuild.py -c -d build-cln-s4 -b jolt,physx`, `./scripts/test-engine-backends.sh build-cln-s4`, and `ctest --test-dir build-cln-s4 -R "physics_backend_parity_.*" --output-on-failure`.
- `2026-02-22`: completed real file split for `S4-2` by moving runtime-command trace/classification/apply helpers out of `src/physics/sync/ecs_sync_system.cpp` into `src/physics/sync/runtime_command_sync.hpp/.cpp` and wiring `cmake/sdk/sources.cmake`.
- `2026-02-22`: measured `ecs_sync_system.cpp` line reduction from pre-slice baseline `1538` lines to `947` lines (`-591`), exceeding the `>=100` requirement.
- `2026-02-22`: completed `S4-3` by extracting create-path runtime-command orchestration seams out of `src/physics/sync/ecs_sync_system.cpp` into `src/physics/sync/runtime_command_sync.hpp/.cpp` via `detail::ApplyAndTraceRuntimeCommandsForCreate(...)` and `detail::ApplyCreateAwakeAndRuntimeCommands(...)`, leaving create-path call-site glue in `create_binding`.
- `2026-02-22`: validation run for `S4-3` in `build-cln-s4` passed via `./abuild.py -c -d build-cln-s4 -b jolt,physx`, `./scripts/test-engine-backends.sh build-cln-s4`, and `ctest --test-dir build-cln-s4 -R "physics_backend_parity_.*" --output-on-failure`.
- `2026-02-22`: measured `ecs_sync_system.cpp` line reduction for `S4-3` validation run from `911` lines (`BEFORE`) to `910` lines (`AFTER`) (`-1`), with cumulative CLN-S4 reduction from `1538` to `910` (`-628`).
- `2026-02-22`: completed `S4-4` boundary pass with explicit no-extract decision for shared create/update fallback helper. Rationale: fallback paths are not equivalent at call-contract level (`create_binding` failure destroys transient body pre-commit, while update rebuild path must destroy existing bound body, clear metadata, erase existing binding iterator, optionally preserve state, and optionally mark runtime-command recovery path), so a shared helper here would either widen coupling/context or risk subtle behavior drift.
- `2026-02-22`: CLN-S5-facing seams are now stable: runtime-command classification/trace/apply/create orchestration is isolated in `runtime_command_sync.*`, while `ecs_sync_system` retains orchestration-only call sites for create/update and deterministic fallback decisions.

## CLN-S5 Handoff Notes
- Stable seams for parity-suite decomposition:
  - `runtime_command_sync.hpp/.cpp` now owns runtime-command typed decisions, trace tagging/classification, update apply flow, and create-path runtime-command orchestration.
  - `ecs_sync_system.cpp` now consumes runtime-command helpers as call-site glue, reducing coupling to parity-observed trace and runtime-command state transitions.
- Intentionally deferred risks/boundaries:
  - create vs update fallback orchestration remains duplicated by design because lifecycle contracts differ (pre-commit transient-create failure vs existing-binding rebuild/teardown flow).
  - static-mesh ingest recovery and runtime metadata lifecycle logic remain in `ecs_sync_system.cpp`; CLN-S5 should treat these as fixed orchestration contracts unless parity evidence requires a dedicated follow-up slice.

## Open Questions
- For CLN-S5 kickoff, which parity domain family should consume the new runtime-command seam first to minimize merge contention with remaining `ecs_sync_system` orchestration code?

## Handoff Checklist
- [x] `S4-1`: runtime-command classification scaffolding extracted in `ecs_sync_system` with typed decision input skeleton and zero intended behavior change.
- [x] `S4-2`: runtime-command trace/classification/apply flow extracted into dedicated sync unit(s) with `ecs_sync_system` reduced to orchestration glue and zero intended behavior change.
- [x] `S4-3`: create-path runtime-command orchestration seams extracted into dedicated sync helper(s) with zero intended behavior change.
- [x] `S4-4`: completed boundary pass; no additional shared fallback helper extracted because create/update fallback contracts are not equivalent.
- [x] CLN-S5-facing handoff notes captured (stable seams + deferred risks/boundaries).
- [x] Physics parity tests remain green for the `S4-3` validation run.
