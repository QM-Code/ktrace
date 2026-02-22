# Cleanup S5 (`CLN-S5`): Physics Parity Suite Decomposition

## Project Snapshot
- Current owner: `specialist-cln-s5`
- Status: `in progress (bootstrap packet prepared; S5-1 domain migration pending)`
- Immediate next task: execute `S5-1` by migrating the `RunEcsSyncSystemPolicyChecks` domain family from `src/physics/tests/parity/common.cpp` into `src/physics/tests/parity/ecs_sync_checks.cpp` with local helpers and no parity-behavior drift.
- Validation gate: `cd m-karma && ctest --test-dir <karma-build-dir> -R "physics_backend_parity_.*" --output-on-failure`.

## Mission
Complete the parity-suite split so file boundaries reflect real test domains, not wrapper stubs over a single giant implementation unit.

## Foundation References
- `projects/cleanup.md`
- `m-karma/src/physics/tests/parity/main.cpp`
- `m-karma/src/physics/tests/parity/common.hpp`
- `m-karma/src/physics/tests/parity/common.cpp`
- `m-karma/cmake/sdk/tests.cmake`

## Why This Is Separate
This is a test-architecture cleanup track that can proceed in parallel with runtime and renderer refactors while guarding physics behavior contracts.

## Owned Paths
- `m-karma/src/physics/tests/parity/*`
- `m-karma/cmake/sdk/tests.cmake` (parity target wiring only)
- `m-overseer/agent/projects/cleanup/physics-parity-suite.md`

## Interface Boundaries
- Inputs consumed:
  - current parity assertions and backend matrix semantics.
- Outputs exposed:
  - maintainable domain-split parity suite with clear fixture/helper ownership.
- Coordinate before changing:
  - `projects/cleanup/physics-sync-decomposition.md`

## Non-Goals
- Do not weaken parity coverage to reduce file size.
- Do not alter command-line backend selection semantics in `parity/main.cpp`.

## Validation
```bash
cd m-karma
./abuild.py -c -d <karma-build-dir>
ctest --test-dir <karma-build-dir> -R "physics_backend_parity_jolt|physics_backend_parity_physx" --output-on-failure
```

## Trace Channels
- `cleanup.s5`
- `physics.tests.parity`

## Build/Run Commands
```bash
cd m-karma
./abuild.py -c -d <karma-build-dir>
./<karma-build-dir>/physics_backend_parity_test --backend jolt
./<karma-build-dir>/physics_backend_parity_test --backend physx
```

## Current Status
- `2026-02-22`: split scaffold exists (`parity/*.cpp` wrappers + `main.cpp`), but implementation remains concentrated in `parity/common.cpp` (~8.5k LOC).
- `2026-02-22`: moved under cleanup superproject child structure.

## Open Questions
- Which domain split yields the highest immediate maintainability gain first (`engine_sync`, `ecs_sync`, or `collider/runtime`)?
- Should shared fixture helpers be split into multiple headers before migrating checks?

## Handoff Checklist
- [ ] `common.cpp` no longer contains the majority of suite logic.
- [ ] Domain files own domain checks and local helpers.
- [ ] Parity binary behavior and backend CLI semantics unchanged.
