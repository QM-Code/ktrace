# KARMA SDK Header Allowlist

## Project Snapshot
- Current owner: `codex`
- Status: `complete (H0-H6 delivered: SDK include boundary hardening + allowlist install + drift guard + compatibility-reviewed prune validated)`
- Immediate next task: maintain `cmake/KarmaSdkHeaders.cmake` classification on future header additions and rerun drift/integration validation gates.
- Validation gate:
  - `m-overseer`: `./scripts/lint-projects.sh`
  - `m-karma`: `./abuild.py -c -d build-a6 --install-sdk out/karma-sdk`
  - `m-bz3`: `./abuild.py -c -d build-a7 --karma-sdk ../m-karma/out/karma-sdk`
  - `m-bz3`: `./scripts/test-server-net.sh build-a7`

## Mission
Narrow `m-karma` SDK-installed headers from broad `include/karma/*` copy to an explicit allowlist while ensuring `m-bz3` actually consumes SDK headers (not local shadow copies) through the locked `find_package(KarmaEngine CONFIG REQUIRED)` integration contract.

## Foundation References
- `projects/diligent.md`
- `m-karma/CMakeLists.txt`
- `m-karma/src/engine/CMakeLists.txt`
- `m-bz3/CMakeLists.txt`
- `m-bz3/src/game/CMakeLists.txt`

## Why This Is Separate
This is a cross-repo packaging and boundary-hardening effort with direct impact on long-term SDK stability. It should be tracked independently from Diligent packaging and gameplay/renderer feature tracks.

## Baseline Findings (2026-02-19)
1. `m-karma` currently installs SDK headers with broad directory copy:
   - `install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/karma DESTINATION ${CMAKE_INSTALL_INCLUDEDIR} ...)` in `m-karma/CMakeLists.txt`.
2. Current `m-karma/include/karma/*` header count: `82`.
3. Unique `karma/*` headers directly included by `m-bz3/src/*`: `58`.
4. `m-bz3/src/*` currently references 3 headers that are not present in `m-karma/include/karma`:
   - `karma/app/ui_context.h`
   - `karma/input/bindings_text.hpp`
   - `karma/graphics/texture_handle.hpp`
5. `m-bz3` still adds local include roots to targets (`${PROJECT_SOURCE_DIR}/include`, `${PROJECT_SOURCE_DIR}/src`) in `m-bz3/src/game/CMakeLists.txt`, so header shadowing can mask SDK drift.

## Issue Matrix
1. Header shadowing in consumer repo (`m-bz3`) prevents reliable SDK-boundary validation.
2. Missing/legacy include paths in `m-bz3` need either migration to existing public headers or explicit promotion into SDK surface.
3. SDK install list in `m-karma` is over-broad and does not encode intended public surface.
4. No automated guard exists to prevent accidental public-header surface growth.
5. Allowlist changes must preserve locked KARMA -> BZ3 package contract.

## Owned Paths
- `m-overseer/projects/headers.md`
- `m-overseer/projects/ASSIGNMENTS.md`
- `m-karma/CMakeLists.txt`
- `m-karma/include/karma/*` (public-header declarations only)
- `m-karma/cmake/*` (if helper manifest/list file is introduced)
- `m-karma/scripts/*` (if allowlist guard script is introduced)
- `m-bz3/CMakeLists.txt`
- `m-bz3/src/game/CMakeLists.txt`
- `m-bz3/src/**` include statements that require migration

## Interface Boundaries
- Inputs consumed:
  - current packaging/export behavior in `m-karma`
  - current consumer include/link behavior in `m-bz3`
- Outputs exposed:
  - explicit, reviewable SDK header allowlist
  - consumer build path that resolves SDK headers without local shadowing
  - automated drift checks for SDK header surface
- Coordinate before changing:
  - `m-karma/CMakeLists.txt`
  - `m-bz3/CMakeLists.txt`
  - `m-bz3/src/game/CMakeLists.txt`
  - `projects/diligent.md` (only for cross-impact notes)

## Non-Goals
- No renderer/audio/physics behavior changes.
- No gameplay feature implementation.
- No change to official BZ3 consume contract (`find_package(KarmaEngine)` stays required).
- No Diligent packaging refactor in this track (owned by `diligent.md`).

## Execution Plan
1. H0: Consumer shadowing baseline
- Identify and document exact `m-bz3` targets currently relying on local `include/karma/*` shadowing.
- Run one bounded target slice proving SDK-only include resolution in practice.

2. H1: Resolve missing header references
- For each missing include (`ui_context.h`, `bindings_text.hpp`, `texture_handle.hpp`):
  - map to existing public SDK header if equivalent exists, or
  - explicitly decide to promote a new header into `m-karma/include/karma/*`.
- Land migrations before tightening install allowlist.

3. H2: Define explicit allowlist
- Build allowlist from:
  - required public entry headers for exported targets,
  - transitive `karma/*` includes required by those headers,
  - additional explicitly approved consumer headers.
- Store allowlist in a maintained manifest (CMake list or dedicated file).

4. H3: Replace broad install with allowlist install
- Remove broad `install(DIRECTORY ... include/karma ...)`.
- Install only allowlisted headers in stable paths under `${CMAKE_INSTALL_INCLUDEDIR}`.
- Keep package config/targets unchanged unless required by header-surface update.

5. H4: Add drift guard
- Add a guard script/check that compares installed SDK header surface against allowlist.
- Fail on unapproved header growth.

6. H5: End-to-end validation
- Re-export SDK from `m-karma`.
- Build `m-bz3` against SDK path with no local header fallback.
- Confirm locked contract remains clean.

## Validation
```bash
# Overseer tracking
cd m-overseer
./scripts/lint-config-projects.sh

# Karma SDK export
cd ../m-karma
./abuild.py -c -d build-sdk --install-sdk out/karma-sdk

# BZ3 SDK consume
cd ../m-bz3
./abuild.py -c -d build-sdk --karma-sdk ../m-karma/out/karma-sdk --ignore-lock
./scripts/test-server-net.sh build-sdk
```

## Build/Run Commands
```bash
# Audit current consumer includes
rg -o "karma/[A-Za-z0-9_./-]+\\.(hpp|h)" m-bz3/src -g"*.hpp" -g"*.h" -g"*.cpp" | sed 's#^.*/src/[^:]*:##' | sort -u

# Re-export SDK after header changes
cd m-karma
./abuild.py -c -d build-sdk --install-sdk out/karma-sdk

# Consume from BZ3
cd ../m-bz3
./abuild.py -c -d build-sdk --karma-sdk ../m-karma/out/karma-sdk --ignore-lock
```

## First Session Checklist
1. Confirm and record all `m-bz3` local-header shadowing points.
2. Resolve the 3 currently missing include references or approve header promotions.
3. Draft and review initial SDK header allowlist manifest.
4. Switch installation from broad copy to explicit allowlist.
5. Add allowlist drift guard and run end-to-end SDK consume validation.

## Current Status
- `2026-02-19`: Project initialized; baseline issue set captured from current `m-karma`/`m-bz3` state.
- `2026-02-19`: `H0` completed with one bounded target slice on `m-bz3`:
  - edited `m-bz3/src/game/CMakeLists.txt` to remove `${PROJECT_SOURCE_DIR}/include` from `target_include_directories(bz3-server PRIVATE ...)` while keeping `${PROJECT_SOURCE_DIR}/src`.
  - validated via:
    - `cd m-karma && ABUILD_AGENT_NAME=specialist-headers-h0 ./abuild.py -c -d build-a6 --install-sdk out/karma-sdk`
    - `cd m-bz3 && ABUILD_AGENT_NAME=specialist-headers-h0 ./abuild.py -c -d build-a7 --karma-sdk ../m-karma/out/karma-sdk`
    - `cd m-bz3 && ABUILD_AGENT_NAME=specialist-headers-h0 ./scripts/test-server-net.sh build-a7`
  - compile flag evidence:
    - `build-a7/src/game/CMakeFiles/bz3-server.dir/flags.make` now contains `-isystem /home/karmak/dev/bz3-rewrite/m-karma/out/karma-sdk/include` and does **not** contain `-I/home/karmak/dev/bz3-rewrite/m-bz3/include`.
    - `build-a7/src/game/CMakeFiles/bz3.dir/flags.make` still contains `-I/home/karmak/dev/bz3-rewrite/m-bz3/include` (expected; out of scope for H0).
- `2026-02-19`: `H1` completed with bounded consumer-side include migration (migrate, not promote):
  - missing legacy include mappings:
    - `karma/app/ui_context.h` -> `ui/core/ui_context_compat.hpp`
    - `karma/input/bindings_text.hpp` -> `ui/console/bindings_text.hpp`
    - `karma/graphics/texture_handle.hpp` -> `ui/core/texture_handle.hpp`
  - added compatibility/migration headers:
    - `m-bz3/src/ui/core/ui_context_compat.hpp`
    - `m-bz3/src/ui/console/bindings_text.hpp`
    - `m-bz3/src/ui/core/texture_handle.hpp`
  - include-site rewires:
    - `m-bz3/src/ui/core/backend.hpp`
    - `m-bz3/src/ui/core/system.hpp`
    - `m-bz3/src/ui/core/ui_layer_adapter.hpp`
    - `m-bz3/src/ui/console/keybindings.hpp`
    - `m-bz3/src/ui/frontends/imgui/hud/radar.hpp`
    - `m-bz3/src/ui/frontends/rmlui/hud/radar.hpp`
  - validation:
    - `rg -n "karma/(app/ui_context\\.h|input/bindings_text\\.hpp|graphics/texture_handle\\.hpp)" m-bz3/src m-bz3/include` -> no matches under code paths.
    - `cd m-bz3 && ABUILD_AGENT_NAME=specialist-headers-h1 ./abuild.py -c -d build-a7 --karma-sdk ../m-karma/out/karma-sdk`
    - `cd m-bz3 && ABUILD_AGENT_NAME=specialist-headers-h1 ./scripts/test-server-net.sh build-a7` (`2/2` tests passed).
- `2026-02-19`: `H2-H3` completed by moving SDK header installation to an explicit allowlist manifest:
  - added `m-karma/cmake/KarmaSdkHeaders.cmake` with a reviewed `KARMA_ENGINE_SDK_HEADER_RELATIVE` list (current count: `82`).
  - wired `m-karma/CMakeLists.txt` to consume the manifest and install each allowlisted header explicitly.
  - removed broad directory install (`install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/karma ...)`) in favor of per-header install calls with missing-file guardrails.
  - validation:
    - `cd m-karma && ABUILD_AGENT_NAME=specialist-headers-h2 ./abuild.py -c -d build-a6 --install-sdk out/karma-sdk`
    - `cd m-bz3 && ABUILD_AGENT_NAME=specialist-headers-h2 ./abuild.py -c -d build-a7 --karma-sdk ../m-karma/out/karma-sdk`
    - `cd m-bz3 && ABUILD_AGENT_NAME=specialist-headers-h2 ./scripts/test-server-net.sh build-a7` (`2/2` tests passed).
- `2026-02-19`: `H4` completed with automated SDK header drift guard:
  - added executable guard script: `m-karma/scripts/check-sdk-header-allowlist.sh`.
  - wired guard into CTest as `karma_sdk_header_allowlist_guard` in `m-karma/CMakeLists.txt`.
  - validation:
    - `cd m-karma && ./scripts/check-sdk-header-allowlist.sh` (`OK: manifest matches include tree (82 headers)`).
    - `cd m-karma/build-a6 && ctest -R karma_sdk_header_allowlist_guard --output-on-failure` (`1/1` passed).
    - `cd m-karma && ABUILD_AGENT_NAME=specialist-headers-h4 ./abuild.py -c -d build-a6 --install-sdk out/karma-sdk`
    - `cd m-bz3 && ABUILD_AGENT_NAME=specialist-headers-h4 ./abuild.py -c -d build-a7 --karma-sdk ../m-karma/out/karma-sdk`
    - `cd m-bz3 && ABUILD_AGENT_NAME=specialist-headers-h4 ./scripts/test-server-net.sh build-a7` (`2/2` tests passed).
- `2026-02-19`: `H6` optional pruning follow-on completed (compatibility-reviewed):
  - split header classification in `m-karma/cmake/KarmaSdkHeaders.cmake`:
    - `KARMA_ENGINE_SDK_HEADER_RELATIVE`: `68` installed/public headers
    - `KARMA_ENGINE_INTERNAL_HEADER_RELATIVE`: `14` non-installed internal headers kept in source tree
  - updated drift guard (`m-karma/scripts/check-sdk-header-allowlist.sh`) to validate manifest classification as a disjoint public/internal partition that exactly matches `include/karma/*`.
  - validated pruned install to fresh SDK path:
    - `cd m-karma && ABUILD_AGENT_NAME=specialist-headers-h6 ./abuild.py -c -d build-a6 --install-sdk out/karma-sdk-pruned`
    - installed header count in `out/karma-sdk-pruned/include/karma/*`: `68`
    - excluded from install (internal list): `14` headers
  - integration validation:
    - `cd m-karma/build-a6 && ctest -R karma_sdk_header_allowlist_guard --output-on-failure` (`1/1` passed)
    - `cd m-bz3 && ABUILD_AGENT_NAME=specialist-headers-h6 ./abuild.py -c -d build-a7 --karma-sdk ../m-karma/out/karma-sdk-pruned`
    - `cd m-bz3 && ABUILD_AGENT_NAME=specialist-headers-h6 ./scripts/test-server-net.sh build-a7` (`2/2` tests passed)

### H0 Shadowing Inventory (2026-02-19)
Targets in `m-bz3/src/game/CMakeLists.txt` that still (or previously) carried `${PROJECT_SOURCE_DIR}/include` in include dirs:
- `bz3`
- `bz3-server` (updated in H0 to remove local include root)
- `server_net_contract_test`
- `server_runtime_event_rules_test`
- `shot_system_physics_hit_test`
- `shot_system_query_diagnostics_test`
- `server_runtime_shot_physics_integration_test`
- `server_runtime_shot_damage_integration_test`
- `server_runtime_shot_damage_idempotence_integration_test`
- `shot_physics_runtime_context_test`
- `server_runtime_shot_pilot_smoke_test`
- `shot_pilot_health_test`
- `shot_pilot_state_machine_test`
- `shot_pilot_recast_controller_test`
- `shot_pilot_calibration_test`
- `client_runtime_cli_contract_test`
- `community_heartbeat_integration_test`
- `server_join_runtime_contract_test`
- `server_session_runtime_contract_test`
- `transport_loopback_integration_test`
- `transport_multiclient_loopback_test`
- `transport_disconnect_lifecycle_integration_test`
- `client_world_package_safety_integration_test`
- `tank_drive_controller_test`
- `tank_collision_probe_shape_test`
- `tank_collision_query_test`
- `tank_collision_resolution_test`
- `tank_collision_guardrails_test`
- `tank_camera_collision_test`
- `tank_motion_authority_pilot_test`
- `tank_motion_authority_state_machine_test`

## Open Questions
- Should `m-bz3/include/karma/*` be fully removed, or kept temporarily behind a controlled migration flag?
- Should SDK header-surface CI/guard run in `m-karma` only, or also be validated from `m-bz3` integration CI?

## Handoff Checklist
- [x] Shadowing audit completed and documented
- [x] Missing include mappings resolved (migrate or promote)
- [x] Explicit SDK header allowlist implemented
- [x] Broad directory install removed/replaced
- [x] Drift guard added and passing
- [x] End-to-end KARMA SDK export + BZ3 consume validation passing
- [x] Optional compatibility-reviewed allowlist pruning completed and validated
