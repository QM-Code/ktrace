# Jolt Backend Parity Failure (Static Mesh Test Path)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress` (new follow-on issue track carved out from `cmake.md`)
- Immediate next task: dispatch a bounded specialist slice to fix parity test asset-path resolution in `m-karma` without changing runtime backend behavior.
- Validation gate:
  - `m-karma`: `ctest --test-dir build-cmake-karma -R physics_backend_parity_jolt -V --output-on-failure`
  - `m-karma`: `./scripts/test-engine-backends.sh build-cmake-karma`
  - `m-overseer`: `./scripts/lint-projects.sh`

## Mission
Resolve the deterministic `physics_backend_parity_jolt` failure in `m-karma` that reports:
- `backend=jolt failed to create static mesh-shape body from mesh path`

This work is scoped to test/parity harness correctness and issue triage/closure, not CMake structure work.

## Foundation References
- `m-overseer/agent/projects/ARCHIVE/cmake.md`
- `m-karma/scripts/test-engine-backends.sh`
- `m-karma/src/physics/tests/physics_backend_parity_test.cpp`
- `m-karma/src/physics/backends/jolt.cpp`
- `m-overseer/agent/docs/building.md`
- `m-overseer/agent/docs/specialists.md`

## Why This Is Separate
- Failure reproduces in both `build-cmake-karma` and pre-existing `build-a6`, so it is not introduced by `KARMA-S1` CMake decomposition.
- Keeping this isolated lets `cmake.md` close on its own objective (mechanical CMake split) while tracking the pre-existing parity defect independently.

## Baseline Findings
1. Reproduction is deterministic:
  - `ctest --test-dir m-karma/build-cmake-karma -R physics_backend_parity_jolt -V --output-on-failure` fails.
  - `ctest --test-dir m-karma/build-a6 -R physics_backend_parity_jolt -V --output-on-failure` fails.
2. Failure point in parity test:
  - `m-karma/src/physics/tests/physics_backend_parity_test.cpp:787` fails creating static mesh body from resolved mesh path.
3. Asset-path resolution appears incorrect for current repo layout:
  - `ResolveTestAssetPath(...)` climbs 5 parent directories from `__FILE__` (`m-karma/src/physics/tests/physics_backend_parity_test.cpp:96`).
  - This resolves to workspace root (`/home/karmak/dev/bz3-rewrite`) instead of `m-karma` repo root.
  - Expected test asset exists at `m-karma/demo/worlds/r55man-2/world.glb`; resolved workspace-root path does not exist.
4. Backend behavior is consistent with load failure:
  - `m-karma/src/physics/backends/jolt.cpp:430` returns invalid body when mesh triangle load fails.

## Owned Paths
- `m-overseer/agent/projects/jolt.md`
- `m-overseer/agent/projects/ASSIGNMENTS.md`
- `m-karma/src/physics/tests/physics_backend_parity_test.cpp`
- optional (only if needed by bounded fix): `m-karma/src/physics/tests/*` shared asset helper files

## Interface Boundaries
- Inputs consumed:
  - existing physics parity test harness and test fixture assets under `m-karma/demo/*`
- Outputs exposed:
  - deterministic, layout-robust parity test asset resolution for static-mesh checks
- Coordinate before changing:
  - `m-karma/src/physics/backends/jolt.cpp` (read-only for this track unless explicitly required)
  - `m-karma/src/physics/backends/physx.cpp` (read-only for this track unless explicitly required)

## Non-Goals
- No CMake structure changes.
- No changes to production/runtime Jolt physics behavior.
- No broad refactor of test framework beyond asset-path resolution scope.
- No SDK packaging/toolchain policy changes.

## Execution Plan
1. `J0` lock baseline and acceptance criteria
- Confirm deterministic fail evidence in assigned build dir.
- Confirm asset existence mismatch evidence.

2. `J1` bounded fix in parity test resolver
- Prefer robust repo-root resolution strategy over fixed parent-count assumptions.
- Keep behavior for direct-path runs intact.

3. `J2` validation and regression check
- Re-run single-test gate and wrapper gate.
- Confirm no unintended changes in other backend parity checks.

## Validation
```bash
cd m-karma
ctest --test-dir build-cmake-karma -R physics_backend_parity_jolt -V --output-on-failure
./scripts/test-engine-backends.sh build-cmake-karma
```

## Build/Run Commands
```bash
cd m-karma
export ABUILD_AGENT_NAME=specialist-jolt-parity
./abuild.py --claim-lock -d build-cmake-karma
./abuild.py -c -d build-cmake-karma
ctest --test-dir build-cmake-karma -R physics_backend_parity_jolt -V --output-on-failure
./scripts/test-engine-backends.sh build-cmake-karma
./abuild.py --release-lock -d build-cmake-karma
```

## First Session Checklist
1. Reproduce failure in assigned build dir.
2. Confirm path-resolution mismatch evidence (`resolved path` vs `actual fixture path`).
3. Implement minimal resolver fix.
4. Re-run parity and wrapper gates.
5. Update this doc and `ASSIGNMENTS.md`.

## Current Status
- `2026-02-21`: Project created from `cmake.md` close-out as a pre-existing blocker follow-on.
- `2026-02-21`: Deterministic fail confirmed in both `build-cmake-karma` and `build-a6`.
- `2026-02-21`: Root-cause hypothesis documented: test asset-path resolver parent-depth assumption mismatch after repo layout changes.

## Open Questions
- Should fix strategy be minimal parent-depth correction (`5 -> 4`) or robust upward search for repo-root sentinel?
- Should static-mesh fixture path resolution helper be centralized to reduce repeated layout assumptions in parity tests?

## Handoff Checklist
- [ ] Bounded resolver fix landed
- [ ] `physics_backend_parity_jolt` passes
- [ ] `test-engine-backends.sh` rerun and results captured
- [ ] `ASSIGNMENTS.md` updated
- [ ] Risks/open questions recorded
