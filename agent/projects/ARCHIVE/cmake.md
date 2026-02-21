# CMake Structure Harmonization (`m-bz3` + `m-karma`)

## Project Snapshot
- Current owner: `overseer`
- Status: `done` (`BZ3-S1` and `KARMA-S1` landed as mechanical splits; `C1` convergence/symmetry review complete; `m-bz3` validation clean with explicit `--karma-sdk`; pre-existing `physics_backend_parity_jolt` issue carved out to `jolt.md`)
- Immediate next task: none in this track; keep for close-out reference while `jolt.md` handles the pre-existing parity failure.
- Validation gate:
  - `m-overseer`: `./scripts/lint-projects.sh`
  - `m-bz3`: `./abuild.py -c -d <bz3-build-dir> --karma-sdk ../m-karma/out/karma-sdk --ignore-lock` and `KARMA_SDK_ROOT=../m-karma/out/karma-sdk ./scripts/test-server-net.sh <bz3-build-dir>`
  - `m-karma`: `./abuild.py -c -d <karma-build-dir> --install-sdk out/karma-sdk`, `./scripts/test-engine-backends.sh <karma-build-dir>`, and `./scripts/test-server-net.sh <karma-build-dir>`

## Mission
Coordinate two separate specialists in parallel to split monolithic source-root CMake coordinators into clearer, smaller include fragments while preserving target names, outputs, and runtime behavior:
- `m-bz3/src/game/CMakeLists.txt`
- `m-karma/src/engine/CMakeLists.txt`

## Foundation References
- `m-bz3/CMakeLists.txt`
- `m-bz3/src/game/CMakeLists.txt`
- `m-karma/CMakeLists.txt`
- `m-karma/src/engine/CMakeLists.txt`
- `m-overseer/agent/templates/SPECIALIST_PACKET.md`

## Why This Is Separate
This is shared build-system maintenance across two repos and should not compete with gameplay/physics integration throughput. It needs overseer-led symmetry control and parallel execution.

## Baseline Findings
1. `m-bz3/CMakeLists.txt` delegates source target graph definition with `add_subdirectory(src/game)`.
2. `m-karma/CMakeLists.txt` delegates source target graph definition with `add_subdirectory(src/engine)`.
3. Both delegated files act as broad target aggregators. The cleanup goal is parity-friendly decomposition, not behavior change.

## Owned Paths
- `m-overseer/agent/projects/cmake.md`
- `m-overseer/agent/projects/ASSIGNMENTS.md`
- `m-bz3/src/game/CMakeLists.txt`
- `m-bz3/src/game/cmake/*` (new)
- `m-karma/src/engine/CMakeLists.txt`
- `m-karma/src/engine/cmake/*` (new)

## Interface Boundaries
- Inputs consumed:
  - existing target/source/include/link/install definitions in both repos
- Outputs exposed:
  - unchanged target names, output binaries/libraries, install/export contracts, and test registration behavior
- Coordinate before changing:
  - `m-bz3/CMakeLists.txt`
  - `m-karma/CMakeLists.txt`
  - any SDK export/install sections in `m-karma/src/engine/CMakeLists.txt`

## Non-Goals
- No gameplay, physics, networking, or rendering behavior changes.
- No target rename/retype/output-path changes.
- No dependency-version/toolchain policy changes.
- No SDK packaging contract churn beyond mechanical refactor needs.

## Execution Plan
1. `C0` overseer symmetry contract (planning packet)
- Define fragment naming/mirroring rules both specialists must follow.
- Define forbidden changes (target names, exports, runtime flags).

2. `BZ3-S1` (specialist A; in parallel)
- Refactor `m-bz3/src/game/CMakeLists.txt` into included fragment files under `m-bz3/src/game/cmake/`.
- Keep `m-bz3/src/game/CMakeLists.txt` as thin coordinator.
- Preserve all current targets/tests and build outputs.

3. `KARMA-S1` (specialist B; in parallel)
- Refactor `m-karma/src/engine/CMakeLists.txt` into included fragment files under `m-karma/src/engine/cmake/`.
- Keep `m-karma/src/engine/CMakeLists.txt` as thin coordinator.
- Preserve current engine targets, tests, and SDK/export/install behavior.

4. `C1` overseer convergence and symmetry check
- Compare both lanes for structure consistency.
- Approve only if both are no-behavior-change and validation-clean.

## Validation
```bash
# m-bz3 lane
cd m-bz3
export ABUILD_AGENT_NAME=specialist-cmake-bz3
./abuild.py --claim-lock -d <bz3-build-dir>
./abuild.py -c -d <bz3-build-dir> --karma-sdk ../m-karma/out/karma-sdk --ignore-lock
KARMA_SDK_ROOT=../m-karma/out/karma-sdk ./scripts/test-server-net.sh <bz3-build-dir>
./abuild.py --release-lock -d <bz3-build-dir>

# m-karma lane
cd ../m-karma
export ABUILD_AGENT_NAME=specialist-cmake-karma
./abuild.py --claim-lock -d <karma-build-dir>
./abuild.py -c -d <karma-build-dir> --install-sdk out/karma-sdk
./scripts/test-engine-backends.sh <karma-build-dir>
./scripts/test-server-net.sh <karma-build-dir>
./abuild.py --release-lock -d <karma-build-dir>

# overseer docs
cd ../m-overseer
./scripts/lint-projects.sh
```

## Build/Run Commands
```bash
# symmetry baseline checks
rg -n "add_subdirectory\\(src/game\\)" m-bz3/CMakeLists.txt
rg -n "add_subdirectory\\(src/engine\\)" m-karma/CMakeLists.txt
rg -n "add_executable\\(|add_library\\(|target_link_libraries\\(" m-bz3/src/game/CMakeLists.txt
rg -n "add_executable\\(|add_library\\(|target_link_libraries\\(" m-karma/src/engine/CMakeLists.txt
```

## First Session Checklist
1. Publish `C0` symmetry contract and specialist packet boundaries.
2. Launch `BZ3-S1` and `KARMA-S1` in parallel (distinct build directories).
3. Require each specialist handoff to include exact commands/results and unchanged-target assurances.
4. Perform overseer convergence diff review.
5. Update this doc and `ASSIGNMENTS.md`.

## Current Status
- `2026-02-21`: Project created for coordinated dual-agent CMake decomposition across `m-bz3` and `m-karma`, superseding standalone `lone-cmake` planning.
- `2026-02-21`: `BZ3-S1` executed in `m-bz3` with a thin include coordinator at `src/game/CMakeLists.txt` plus new fragments: `cmake/sources.cmake`, `cmake/targets.cmake`, `cmake/target_wiring.cmake`, and `cmake/tests.cmake`.
- `2026-02-21`: `m-bz3` validation command sequence was executed using `ABUILD_AGENT_NAME=specialist-cmake-bz3` and `build-cmake-bz3`; both configure/build and server-net wrapper failed at configure due to unresolved `find_package(KarmaEngine)` (`KarmaEngineConfig.cmake` not found).
- `2026-02-21`: `KARMA-S1` executed in `m-karma` with a thin include coordinator at `src/engine/CMakeLists.txt` plus new fragments: `cmake/sources.cmake`, `cmake/targets.cmake`, `cmake/core_wiring.cmake`, `cmake/client_wiring.cmake`, `cmake/sandbox.cmake`, and `cmake/tests.cmake`.
- `2026-02-21`: `m-karma` validation command sequence was executed using `ABUILD_AGENT_NAME=specialist-cmake-karma` and `build-cmake-karma`; configure/build passed, `./scripts/test-server-net.sh build-cmake-karma` passed, and `./scripts/test-engine-backends.sh build-cmake-karma` failed consistently on `physics_backend_parity_jolt` (`backend=jolt failed to create static mesh-shape body from mesh path`).
- `2026-02-21`: Overseer `C1` convergence review completed; both lanes preserve target/test registration sets and represent mechanical decomposition with no intentional behavior change.
- `2026-02-21`: Validation policy correction captured: `m-bz3` consumer lane must use explicit SDK path (`--karma-sdk ../m-karma/out/karma-sdk --ignore-lock`) and set `KARMA_SDK_ROOT` when wrapper scripts reconfigure.
- `2026-02-21`: Overseer reran `m-bz3` validation with explicit SDK path (`./abuild.py -c -d build-cmake-bz3 --karma-sdk ../m-karma/out/karma-sdk --ignore-lock`) plus wrapper env (`KARMA_SDK_ROOT=../m-karma/out/karma-sdk ./scripts/test-server-net.sh build-cmake-bz3`); configure/build and server-net wrapper passed.
- `2026-02-21`: CMake track close-out accepted by exception: deterministic `physics_backend_parity_jolt` failure is pre-existing (repro in `build-a6`) and moved to dedicated follow-on project `m-overseer/agent/projects/jolt.md`.

## C1 Convergence Review (2026-02-21)
- `m-bz3` lane:
  - thin coordinator now includes `sources.cmake`, `targets.cmake`, `target_wiring.cmake`, `tests.cmake` in deterministic order.
  - extracted content preserves target names and `add_test(NAME ...)` registrations versus pre-split baseline.
- `m-karma` lane:
  - thin coordinator now includes `sources.cmake`, `targets.cmake`, `core_wiring.cmake`, `client_wiring.cmake`, `sandbox.cmake`, `tests.cmake` in deterministic order.
  - extracted content preserves target names and `add_test(NAME ...)` registrations versus pre-split baseline.
- Cross-lane verdict:
  - symmetry intent satisfied (same structural decomposition pattern; repo-specific wiring split accepted where domain differs).
  - remaining acceptance blockers are validation/environmental, not structural.

## Open Questions
- Should fragment file names be fully mirrored across repos (`client.cmake`, `server.cmake`, `tests.cmake`) or repo-specific to existing domain naming?
- Should both lanes include a one-shot script check that verifies target-count parity before/after split?

## Handoff Checklist
- [x] `C0` symmetry contract published
- [x] `BZ3-S1` landed (mechanical split only; target names, test names, and output wiring preserved)
- [x] `BZ3-S1` validation clean
- [x] `KARMA-S1` landed (mechanical split only; engine target/export/install wiring preserved)
- [x] `KARMA-S1` validation disposition captured (`physics_backend_parity_jolt` is pre-existing and tracked in `jolt.md`; server-net wrapper passes)
- [x] Cross-repo symmetry review completed
- [x] `ASSIGNMENTS.md` updated
