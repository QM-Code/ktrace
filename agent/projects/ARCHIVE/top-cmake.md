# Top-Level CMake Decomposition (`m-bz3` + `m-karma`)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (TC1 + TC2 landed; TC3 convergence review complete; optional TC4 decision pending)`
- Immediate next task: decide `TC4` optional support-file migration (`cmake/support` vs `cmake/package`) or close this track without TC4.
- Validation gate:
  - `m-overseer`: `./agent/scripts/lint-projects.sh`
  - `m-bz3`: `./abuild.py -c -d <bz3-build-dir> --karma-sdk ../m-karma/out/karma-sdk --ignore-lock` and `KARMA_SDK_ROOT=../m-karma/out/karma-sdk ./scripts/test-server-net.sh <bz3-build-dir>`
  - `m-karma`: `./abuild.py -c -d <karma-build-dir> --install-sdk out/karma-sdk`, `./scripts/test-engine-backends.sh <karma-build-dir>`, `./scripts/test-server-net.sh <karma-build-dir>`

## Mission
Split top-level monolithic coordinators into smaller include fragments while preserving target names, install/export behavior, backend toggles, and runtime behavior:
- `m-bz3/CMakeLists.txt`
- `m-karma/CMakeLists.txt`

## Foundation References
- `m-bz3/CMakeLists.txt`
- `m-karma/CMakeLists.txt`
- `m-karma/cmake/KarmaEngineConfig.cmake.in`
- `m-karma/cmake/KarmaSdkHeaders.cmake`
- `m-overseer/agent/docs/building.md`
- `m-overseer/agent/projects/ARCHIVE/cmake.md`

## Locked Structure Decisions
1. Top-level phase fragments live directly under each repo `cmake/` directory.
2. Auxiliary/package/template files should live under a dedicated subdirectory (`cmake/support/` or `cmake/package/`) when migrated.
3. Numeric filename prefixes are optional but accepted as ordering aids (`00_`, `10_`, ...).  
4. Include order is always authoritative via explicit `include(...)` sequence in root `CMakeLists.txt`.
5. Existing support-file paths stay unchanged in the first decomposition slice unless explicitly included in scope.

## TC0 Contract Lock (2026-02-21)
Locked for `TC1` and `TC2`:
1. Use the fragment filenames listed in `Proposed File Layout` exactly.
2. Keep numeric prefixes in this first split (`00_`, `10_`, ... `90_`) to make include order explicit.
3. Keep root `CMakeLists.txt` files as thin coordinators with explicit ordered `include(...)` statements.
4. Do not move support/template files in this slice (`KarmaEngineConfig.cmake.in`, `KarmaSdkHeaders.cmake`, or other existing support files).
5. No target/add_library/add_executable semantic changes beyond file movement into fragments.
6. No dependency version/source changes, backend token changes, or install/export contract changes.

## Why This Is Separate
- Prior `cmake.md` completed source-root (`src/game`, `src/engine`) decomposition only.
- Top-level files have broader blast radius (toolchain, dependency resolution, SDK packaging/export).
- This needs stricter sequencing and clear boundaries around support-file relocation.

## Baseline Findings
1. Both top-level files still contain large multi-phase logic (toolchain, backend options, dependency acquisition, repo subdirectory delegation, and in `m-karma` SDK packaging/export).
2. `m-karma/cmake/KarmaEngineConfig.cmake.in` is a consumer package config template, not a phase fragment.
3. `m-karma/cmake/KarmaSdkHeaders.cmake` is an SDK header manifest used by both CMake install logic and `scripts/check-sdk-header-allowlist.sh`.
4. `m-bz3` Diligent path is now package-based (`find_package(DiligentEngine CONFIG REQUIRED)`) and aligned with `m-karma`.

## Proposed File Layout

`m-bz3/cmake/` (phase fragments)
- `00_toolchain.cmake`
- `10_backend_options.cmake`
- `20_dependencies.cmake`
- `30_bgfx.cmake`
- `31_diligent.cmake`
- `40_karma_sdk.cmake`
- `90_subdirs.cmake`

`m-karma/cmake/` (phase fragments)
- `00_toolchain.cmake`
- `10_backend_options.cmake`
- `20_dependencies.cmake`
- `30_bgfx.cmake`
- `31_diligent.cmake`
- `40_engine_subdir.cmake`
- `50_sdk_install_export.cmake`
- `60_package_config.cmake`
- `90_tests.cmake`

Optional follow-on (separate slice):
- relocate auxiliary files under `cmake/support/` or `cmake/package/` and update all references atomically.

## Interface Boundaries
- Inputs consumed:
  - existing top-level options/cache variables, dependency acquisition rules, and install/export/package contracts
- Outputs exposed:
  - unchanged consumer-facing package and target behavior
- Coordinate before changing:
  - `m-karma/cmake/KarmaEngineConfig.cmake.in`
  - `m-karma/cmake/KarmaSdkHeaders.cmake`
  - `m-bz3/cmake/KarmaEngineConfig.cmake.in`
  - `m-overseer/agent/docs/building.md`

## Non-Goals
- No gameplay/render/physics behavior changes.
- No target rename/retype/output-path changes.
- No dependency version changes as part of decomposition.
- No support-file path migration in the initial decomposition slice.

## Execution Plan
1. `TC0` contract and packet
- Lock exact fragment names/order and forbidden-change list.

2. `TC1` `m-bz3` top-level split
- Extract top-level phases into `m-bz3/cmake/*.cmake`.
- Keep root `CMakeLists.txt` as thin coordinator.

3. `TC2` `m-karma` top-level split
- Extract top-level phases into `m-karma/cmake/*.cmake`.
- Preserve SDK install/export/package behavior exactly.

4. `TC3` convergence review
- Cross-repo symmetry check and no-behavior-change validation.

5. `TC4` optional support-file migration
- Move auxiliary/package files to subdirectory only after TC1/TC2 stabilization.

## Validation
```bash
# m-bz3
cd m-bz3
export ABUILD_AGENT_NAME=specialist-top-cmake-bz3
./abuild.py --claim-lock -d <bz3-build-dir>
./abuild.py -c -d <bz3-build-dir> --karma-sdk ../m-karma/out/karma-sdk --ignore-lock
KARMA_SDK_ROOT=../m-karma/out/karma-sdk ./scripts/test-server-net.sh <bz3-build-dir>
./abuild.py --release-lock -d <bz3-build-dir>

# m-karma
cd ../m-karma
export ABUILD_AGENT_NAME=specialist-top-cmake-karma
./abuild.py --claim-lock -d <karma-build-dir>
./abuild.py -c -d <karma-build-dir> --install-sdk out/karma-sdk
./scripts/test-engine-backends.sh <karma-build-dir>
./scripts/test-server-net.sh <karma-build-dir>
./abuild.py --release-lock -d <karma-build-dir>

# overseer docs
cd ../m-overseer
./agent/scripts/lint-projects.sh
```

## Current Status
- `2026-02-21`: Scope and structure decisions documented from overseer/user design discussion.
- `2026-02-21`: Locked preference captured: phase fragments at `cmake/` root; auxiliary files in subdirectory track.
- `2026-02-21`: `m-bz3` Diligent consumption aligned to package mode (`find_package(DiligentEngine CONFIG REQUIRED)`), reducing top-level split risk.
- `2026-02-21`: `TC0` contract lock published with explicit filenames/order and forbidden-change list; dispatch packets prepared for `TC1` (`m-bz3`) and `TC2` (`m-karma`).
- `2026-02-21`: `TC2` landed in `m-karma`: root `CMakeLists.txt` converted to ordered include coordinator and required `cmake/00..90` fragments created without changing dependency sources/versions or support-file paths.
- `2026-02-21`: `TC2` validation on `build-cmake-karma` passed configure/build/install and `test-server-net`; `test-engine-backends` failed only on known separate issue `physics_backend_parity_jolt` (tracked in `jolt.md`), unchanged by this slice.
- `2026-02-21`: `TC1` landed in `m-bz3`: root `CMakeLists.txt` decomposed into ordered includes with locked phase fragments `cmake/00_toolchain.cmake`, `10_backend_options.cmake`, `20_dependencies.cmake`, `30_bgfx.cmake`, `31_diligent.cmake`, `40_karma_sdk.cmake`, and `90_subdirs.cmake`; no support/template paths moved and no dependency source/version changes.
- `2026-02-21`: `TC1` validation on `build-cmake-bz3` passed `./abuild.py -c -d build-cmake-bz3 --karma-sdk ../m-karma/out/karma-sdk --ignore-lock` and `ABUILD_AGENT_NAME=specialist-top-cmake-bz3 KARMA_SDK_ROOT=../m-karma/out/karma-sdk ./scripts/test-server-net.sh build-cmake-bz3` after a lock-env rerun of the wrapper invocation.
- `2026-02-21`: `TC3` convergence review completed by overseer:
  - both repos match locked fragment filenames/order from `TC0`,
  - root `CMakeLists.txt` files are thin ordered include coordinators with direct top-level `project(...)`,
  - support/template files (`KarmaEngineConfig.cmake.in`, `KarmaSdkHeaders.cmake`) were not moved or modified,
  - overseer docs gate `./agent/scripts/lint-projects.sh` passed.

## Open Questions
- Prefer `cmake/support/` or `cmake/package/` as the auxiliary-file subdirectory name?
- Keep numeric prefixes from `TC1` or use plain names with ordering documented in root include list only?

## Handoff Checklist
- [x] `TC0` packet published (names/order/forbidden changes)
- [x] `TC1` landed with validation results
- [x] `TC2` landed with validation results
- [x] `TC3` convergence review complete
- [x] `ASSIGNMENTS.md` updated
- [ ] optional `TC4` decision captured
