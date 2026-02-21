# Top-Level CMake Decomposition (`m-bz3` + `m-karma`)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (scope + structure contract drafted; implementation not yet dispatched)`
- Immediate next task: publish `TC0` specialist packet with locked directory layout and no-behavior-change guardrails, then dispatch one lane per repo.
- Validation gate:
  - `m-overseer`: `./scripts/lint-projects.sh`
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
./scripts/lint-projects.sh
```

## Current Status
- `2026-02-21`: Scope and structure decisions documented from overseer/user design discussion.
- `2026-02-21`: Locked preference captured: phase fragments at `cmake/` root; auxiliary files in subdirectory track.
- `2026-02-21`: `m-bz3` Diligent consumption aligned to package mode (`find_package(DiligentEngine CONFIG REQUIRED)`), reducing top-level split risk.

## Open Questions
- Prefer `cmake/support/` or `cmake/package/` as the auxiliary-file subdirectory name?
- Keep numeric prefixes from `TC1` or use plain names with ordering documented in root include list only?

## Handoff Checklist
- [ ] `TC0` packet published (names/order/forbidden changes)
- [ ] `TC1` landed with validation results
- [ ] `TC2` landed with validation results
- [ ] `TC3` convergence review complete
- [ ] `ASSIGNMENTS.md` updated
- [ ] optional `TC4` decision captured
