# Test Infrastructure Refactor (Shared Support + Explicit Roots)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (TST-S1/TST-S2/TST-S3 landed; parity decomposition pending)`
- Immediate next task: execute `TST-S4` to split `physics_backend_parity_test` into themed translation units while preserving executable + CTest contracts.
- Validation gate:
  - `m-karma`: `ctest --test-dir <build-dir> -R physics_backend_parity_jolt -V --output-on-failure`
  - `m-karma`: `./scripts/test-engine-backends.sh <build-dir>`
  - `m-overseer`: `./agent/scripts/lint-projects.sh`

## Mission
Refactor `m-karma` test infrastructure to remove layout-fragile path logic and reduce monolithic test maintenance cost by:
- introducing a shared test-support layer,
- defining an explicit test root contract (compile-time + env override),
- migrating physics parity to shared helpers,
- decomposing parity checks into multiple translation units while preserving current CTest names and runtime behavior.

## Foundation References
- `m-overseer/agent/docs/building.md`
- `m-overseer/agent/docs/testing.md`
- `m-overseer/agent/docs/specialists.md`
- `m-overseer/agent/projects/jolt.md`
- `m-karma/cmake/sdk/tests.cmake`
- `m-karma/src/physics/tests/physics_backend_parity_test.cpp`
- `m-karma/src/network/tests/support/loopback_endpoint_alloc.hpp`
- `m-karma/src/network/tests/support/loopback_enet_fixture.hpp`

## Why This Is Separate
- This is a test-architecture track, not backend/runtime behavior work.
- Scope touches many test files and CMake test wiring and benefits from independent planning + handoff.
- Keeping this separate protects active gameplay/engine tracks from mixed concerns.

## Pass Budget
- Aggressive track (allows temporary breakage between slices): `6-8` passes.
- Conservative track (green at each slice): `9-12` passes.
- Recommended default: aggressive internally, but every handoff to overseer must include at least one green validation command.

## Owned Paths
- `m-overseer/agent/projects/tests.md`
- `m-overseer/agent/projects/ASSIGNMENTS.md`
- `m-karma/cmake/sdk/tests.cmake`
- `m-karma/src/tests/support/*`
- `m-karma/src/physics/tests/*`
- optional adoption scope (only when explicitly scheduled): `m-karma/src/audio/tests/*`, `m-karma/src/renderer/tests/*`, `m-karma/src/network/tests/*`

## Interface Boundaries
- Inputs consumed:
  - test fixtures under `m-karma/demo/*`
  - existing test target registration in `m-karma/cmake/sdk/tests.cmake`
- Outputs exposed:
  - shared test-support API for path/fixture resolution
  - explicit root contract for test binaries
  - parity executable split into maintainable source units with unchanged behavior contracts
- Coordinate before changing:
  - `m-karma/src/physics/backends/jolt.cpp` (read-only unless explicitly authorized)
  - `m-karma/src/physics/backends/physx.cpp` (read-only unless explicitly authorized)
  - `m-karma/src/common/data/path_resolver.*` (coordinate before reusing/modifying shared production resolver code)

## Non-Goals
- No production physics/backend behavior changes.
- No SDK packaging policy changes.
- No game/runtime command contract changes.
- No test-name churn for existing CTest entries (`physics_backend_parity_jolt`, `physics_backend_parity_physx` if enabled).

## Target Layout (Desired End State)
- New shared test-support module:
  - `m-karma/src/tests/support/test_asset_path.hpp`
  - `m-karma/src/tests/support/test_asset_path.cpp`
  - optional follow-on:
    - `m-karma/src/tests/support/test_fixture_manifest.hpp`
    - `m-karma/src/tests/support/test_fixture_manifest.cpp`
- Physics parity decomposition (same executable name, multiple source files):
  - `m-karma/src/physics/tests/parity/main.cpp`
  - `m-karma/src/physics/tests/parity/common.hpp`
  - `m-karma/src/physics/tests/parity/common.cpp`
  - `m-karma/src/physics/tests/parity/lifecycle_checks.cpp`
  - `m-karma/src/physics/tests/parity/collider_runtime_checks.cpp`
  - `m-karma/src/physics/tests/parity/ecs_sync_checks.cpp`
  - `m-karma/src/physics/tests/parity/engine_sync_checks.cpp`
  - `m-karma/src/physics/tests/parity/facade_checks.cpp`

## Execution Plan
1. `TST-S0` baseline + invariants lock
- Capture baseline test list and command outputs for assigned build dir.
- Confirm invariant list:
  - executable name remains `physics_backend_parity_test`
  - CTest names remain unchanged
  - fixture roots remain under `m-karma/demo/*`

2. `TST-S1` shared test-support foundation
- Add `src/tests/support/test_asset_path.hpp/.cpp`.
- API contract:
  - resolve direct existing paths,
  - resolve from `KARMA_TEST_ROOT` env override when set,
  - resolve from compile-time project root fallback,
  - preserve missing-path behavior for negative tests by returning deterministic candidate path.
- Wire helper into CMake test targets (direct source inclusion or support library; support library preferred for reuse).

3. `TST-S2` explicit root contract in CMake
- Define compile-time root for test targets in `cmake/sdk/tests.cmake` (for example `KARMA_TEST_SOURCE_ROOT`).
- Ensure contract is available to shared helper without hard-coded parent traversal.

4. `TST-S3` parity migration to shared helper
- Remove file-local resolver in `physics_backend_parity_test.cpp`.
- Replace call sites with shared helper.
- Revalidate missing-asset negative tests still fail for expected reason.

5. `TST-S4` parity file decomposition
- Split monolithic parity source into themed translation units under `src/physics/tests/parity/`.
- Keep executable + CTest command contract unchanged.
- Keep behavior ordering and backend-selection semantics unchanged.

6. `TST-S5` adoption/audit for other test suites
- Audit `audio`, `renderer`, and `network` tests for layout-sensitive path logic.
- Migrate only confirmed path-resolution duplicates.
- If none found, record evidence and close slice without code churn.

7. `TST-S6` stabilization + docs close-out
- Run required wrappers and targeted CTest gates.
- Update this project doc + `ASSIGNMENTS.md`.
- Record unresolved risks and optional follow-ons (fixture manifest centralization, extra support APIs).

## Validation
```bash
cd m-karma
ctest --test-dir <build-dir> -R physics_backend_parity_jolt -V --output-on-failure
./scripts/test-engine-backends.sh <build-dir>
```

Additional recommended checks:
```bash
cd m-karma
ctest --test-dir <build-dir> -R "physics_backend_parity_(jolt|physx)" -V --output-on-failure
ctest --test-dir <build-dir> -R "client_transport_contract_test|server_transport_contract_test|renderer_directional_shadow_contract|audio_backend_smoke_.*" --output-on-failure
```

## Build/Run Commands
```bash
cd m-karma
export ABUILD_AGENT_NAME=specialist-tests-refactor
./abuild.py --claim-lock -d <build-dir>
./abuild.py -c -d <build-dir>

# run required gates for this track
ctest --test-dir <build-dir> -R physics_backend_parity_jolt -V --output-on-failure
./scripts/test-engine-backends.sh <build-dir>

./abuild.py --release-lock -d <build-dir>
```

## Specialist Slice Contracts
- One slice per handoff, with concrete goal + bounded file list.
- If a slice intentionally leaves tree red, specialist must include:
  - exact failing command,
  - root-cause statement,
  - next slice that restores green.
- Any production code touch outside tests/CMake requires explicit overseer approval.

## First Session Checklist
1. Read required docs + this project file.
2. Confirm assigned build dir and lock ownership.
3. Execute `TST-S0` baseline capture.
4. Execute `TST-S1` only (do not combine with decomposition in first slice).
5. Run required validation and update docs.

## Current Status
- `2026-02-21`: Project created to track the "better" long-term test refactor beyond the bounded Jolt path fix.
- `2026-02-21`: Strategy chosen: shared support + explicit root contract + parity decomposition.
- `2026-02-21`: Completed `TST-S1` + `TST-S2` + `TST-S3` on `build-sdk-10`:
  - added `m-karma/src/tests/support/test_asset_path.hpp/.cpp`,
  - wired `karma_test_support` in `m-karma/cmake/sdk/tests.cmake` with compile-time `KARMA_TEST_SOURCE_ROOT`,
  - removed file-local parity resolver and migrated to shared helper,
  - validation passed:
    - `m-karma`: `./abuild.py -c -d build-sdk-10`
    - `m-karma`: `ctest --test-dir build-sdk-10 -R physics_backend_parity_jolt -V --output-on-failure`
    - `m-karma`: `./scripts/test-engine-backends.sh build-sdk-10`

## Open Questions
- Do we want a fixture manifest in the first pass, or defer until after parity split lands?
- Should `physics_backend_parity_test` remain one executable with many TUs, or split into multiple executables with labeled CTest groups?

## Handoff Checklist
- [x] Shared test-support module added and wired
- [x] Explicit test root contract defined in CMake
- [x] Parity resolver migrated to shared helper
- [ ] Parity test file decomposition completed
- [x] Required validation commands passed
- [x] `ASSIGNMENTS.md` updated
- [x] Risks/open questions recorded
