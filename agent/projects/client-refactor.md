# Client Topology Refactor (A): Remove `src/client/game` Layer

## Project Snapshot
- Current owner: `specialist-client-refactor-a1`
- Status: `completed (CR-A5 namespace convergence landed; client topology refactor migration work closed)`
- Immediate next task: project closeout only (no remaining `client-topology-refactor-a` migration tasks in `m-bz3`).
- Validation gate:
  - `m-overseer`: `./agent/scripts/lint-projects.sh`
  - `m-bz3`: `./abuild.py --agent specialist-client-refactor-a1 --claim-lock -d build-client-refactor-a1 && ./abuild.py --agent specialist-client-refactor-a1 -c -d build-client-refactor-a1 --karma-sdk ../m-karma/out/karma-sdk && ./abuild.py --agent specialist-client-refactor-a1 --release-lock -d build-client-refactor-a1`

## Mission
Eliminate the client-only `src/client/game` directory layer so client/server source topology is consistently role-based (`domain`, `runtime`, `net`) and no special client-only app bucket remains.

## Strategic Alignment
- Track: `client-topology-refactor-a`
- CR-A1 intent: de-risk high-churn structural path/include/CMake moves by locking the migration sequence before implementation.

## Foundation References
- `m-overseer/agent/docs/building.md`
- `m-overseer/agent/docs/testing.md`
- `m-bz3/src/client/runtime/game.hpp`
- `m-bz3/src/client/runtime/*`
- `m-bz3/src/client/domain/*`
- `m-bz3/src/server/runtime/server_game.hpp`
- `m-bz3/cmake/targets/*`

## Why This Is Separate
This is a broad structural migration across CMake paths, include paths, tests, and docs. Isolating it prevents runtime-behavior tracks from absorbing high-churn file movement.

## Scope Baseline (at project start)
- `src/client/game` include-path references: `~93`
- `namespace bz3::client::game` references: `~43`
- `src/client/game/*` CMake path references: `~30`

## Scope Snapshot (`2026-02-24`, CR-A1 measurement)
- `src/client/game/*` files: `28`
- include-path references to `client/game/*`: `62` total, `14` outside `src/client/game/*`
- `client::game::` symbol references: `261` total, `146` outside `src/client/game/*`
- CMake path references to `src/client/game/*`: `30` total (`cmake/targets/sources.cmake` + `cmake/targets/tests.cmake`)

## Owned Paths
- `m-bz3/src/client/*`
- `m-bz3/cmake/targets/*`
- `m-bz3/src/tests/*` (as needed for include/path updates)
- `m-overseer/agent/projects/client-refactor.md`
- `m-overseer/agent/projects/ASSIGNMENTS.md`

## Interface Boundaries
- Inputs consumed:
  - current client/server runtime contracts and existing test expectations
- Outputs exposed:
  - client source topology contract with no `src/client/game/*` subtree
  - updated include/CMake path contracts for all client-related targets/tests
- Coordinate before changing:
  - `m-overseer/agent/projects/fix-gameplay.md`

## Non-Goals
- Do not change gameplay behavior, net protocol semantics, or server authority rules as part of path migration.
- Do not combine this migration with broad server-side directory rework.
- Do not mix style-only rewrites with structural moves in the same slice.
- Do not introduce compatibility shims, forwarding headers, fallback paths, or dual-path include behavior in any slice.

## Execution Plan
### `CR-A1` Inventory + Move Map
- Output status: `completed (2026-02-24)`

#### Inventory: `src/client/game/*` (exact files)
- `src/client/game/audio.cpp`
- `src/client/game/game.hpp`
- `src/client/game/lifecycle.cpp`
- `src/client/game/math.hpp`
- `src/client/game/score_state.hpp`
- `src/client/game/shot_spawn.hpp`
- `src/client/game/tank_camera.cpp`
- `src/client/game/tank_camera_collision.cpp`
- `src/client/game/tank_camera_collision.hpp`
- `src/client/game/tank_collision.cpp`
- `src/client/game/tank_collision_guardrails.cpp`
- `src/client/game/tank_collision_guardrails.hpp`
- `src/client/game/tank_collision_probe_shape.cpp`
- `src/client/game/tank_collision_probe_shape.hpp`
- `src/client/game/tank_collision_query.cpp`
- `src/client/game/tank_collision_query.hpp`
- `src/client/game/tank_collision_resolution.cpp`
- `src/client/game/tank_collision_resolution.hpp`
- `src/client/game/tank_collision_runtime_query_context.cpp`
- `src/client/game/tank_collision_runtime_query_context.hpp`
- `src/client/game/tank_collision_step_stats.cpp`
- `src/client/game/tank_collision_step_stats.hpp`
- `src/client/game/tank_entity.cpp`
- `src/client/game/tank_motion.cpp`
- `src/client/game/tank_motion_authority_pilot.cpp`
- `src/client/game/tank_motion_authority_pilot.hpp`
- `src/client/game/tank_motion_authority_state_machine.cpp`
- `src/client/game/tank_motion_authority_state_machine.hpp`

#### Inventory: include-path references to `client/game/*` outside subtree (`14`)
- `src/client/runtime/internal.hpp`: `#include "client/game/game.hpp"`
- `src/client/runtime/run.cpp`: `#include "client/game/game.hpp"`
- `src/tests/client_score_state_contract_test.cpp`: `#include "client/game/score_state.hpp"`
- `src/tests/client_shot_reconciliation_test.cpp`: `#include "client/game/shot_spawn.hpp"`
- `src/tests/tank_camera_collision_test.cpp`: `#include "client/game/tank_camera_collision.hpp"`, `#include "client/game/tank_collision_step_stats.hpp"`
- `src/tests/tank_collision_guardrails_test.cpp`: `#include "client/game/tank_collision_guardrails.hpp"`
- `src/tests/tank_collision_probe_shape_test.cpp`: `#include "client/game/tank_collision_probe_shape.hpp"`
- `src/tests/tank_collision_query_test.cpp`: `#include "client/game/tank_collision_query.hpp"`, `#include "client/game/tank_collision_guardrails.hpp"`
- `src/tests/tank_collision_resolution_test.cpp`: `#include "client/game/tank_collision_resolution.hpp"`
- `src/tests/tank_motion_authority_pilot_test.cpp`: `#include "client/game/tank_motion_authority_pilot.hpp"`, `#include "client/game/tank_motion_authority_state_machine.hpp"`
- `src/tests/tank_motion_authority_state_machine_test.cpp`: `#include "client/game/tank_motion_authority_state_machine.hpp"`

#### Inventory: CMake path references to `src/client/game/*` (`30`)
- `cmake/targets/sources.cmake`: 15 references in `BZ3_CLIENT_GAME_SRCS` (`lifecycle.cpp` through `audio.cpp`).
- `cmake/targets/tests.cmake`: 15 references across test targets:
  - `tank_collision_probe_shape_test`
  - `tank_collision_query_test`
  - `tank_collision_resolution_test`
  - `tank_collision_guardrails_test`
  - `tank_camera_collision_test`
  - `tank_motion_authority_pilot_test`
  - `tank_motion_authority_state_machine_test`

#### Move Table: `src/client/game/*` -> topology-aligned destinations

| Source | Destination | Slice |
|---|---|---|
| `src/client/game/game.hpp` | `src/client/runtime/game.hpp` | `CR-A2` |
| `src/client/game/lifecycle.cpp` | `src/client/runtime/lifecycle.cpp` | `CR-A2` |
| `src/client/game/audio.cpp` | `src/client/runtime/audio.cpp` | `CR-A2` |
| `src/client/game/tank_entity.cpp` | `src/client/runtime/tank_entity.cpp` | `CR-A2` |
| `src/client/game/tank_motion.cpp` | `src/client/runtime/tank_motion.cpp` | `CR-A2` |
| `src/client/game/tank_collision.cpp` | `src/client/runtime/tank_collision.cpp` | `CR-A2` |
| `src/client/game/tank_camera.cpp` | `src/client/runtime/tank_camera.cpp` | `CR-A2` |
| `src/client/game/tank_collision_runtime_query_context.hpp` | `src/client/runtime/tank_collision_runtime_query_context.hpp` | `CR-A2` |
| `src/client/game/tank_collision_runtime_query_context.cpp` | `src/client/runtime/tank_collision_runtime_query_context.cpp` | `CR-A2` |
| `src/client/game/math.hpp` | `src/client/domain/math.hpp` | `CR-A3` |
| `src/client/game/score_state.hpp` | `src/client/domain/score_state.hpp` | `CR-A3` |
| `src/client/game/shot_spawn.hpp` | `src/client/domain/shot_spawn.hpp` | `CR-A3` |
| `src/client/game/tank_collision_guardrails.hpp` | `src/client/domain/tank_collision_guardrails.hpp` | `CR-A3` |
| `src/client/game/tank_collision_guardrails.cpp` | `src/client/domain/tank_collision_guardrails.cpp` | `CR-A3` |
| `src/client/game/tank_collision_query.hpp` | `src/client/domain/tank_collision_query.hpp` | `CR-A3` |
| `src/client/game/tank_collision_query.cpp` | `src/client/domain/tank_collision_query.cpp` | `CR-A3` |
| `src/client/game/tank_collision_probe_shape.hpp` | `src/client/domain/tank_collision_probe_shape.hpp` | `CR-A3` |
| `src/client/game/tank_collision_probe_shape.cpp` | `src/client/domain/tank_collision_probe_shape.cpp` | `CR-A3` |
| `src/client/game/tank_collision_resolution.hpp` | `src/client/domain/tank_collision_resolution.hpp` | `CR-A3` |
| `src/client/game/tank_collision_resolution.cpp` | `src/client/domain/tank_collision_resolution.cpp` | `CR-A3` |
| `src/client/game/tank_collision_step_stats.hpp` | `src/client/domain/tank_collision_step_stats.hpp` | `CR-A3` |
| `src/client/game/tank_collision_step_stats.cpp` | `src/client/domain/tank_collision_step_stats.cpp` | `CR-A3` |
| `src/client/game/tank_camera_collision.hpp` | `src/client/domain/tank_camera_collision.hpp` | `CR-A3` |
| `src/client/game/tank_camera_collision.cpp` | `src/client/domain/tank_camera_collision.cpp` | `CR-A3` |
| `src/client/game/tank_motion_authority_pilot.hpp` | `src/client/domain/tank_motion_authority_pilot.hpp` | `CR-A3` |
| `src/client/game/tank_motion_authority_pilot.cpp` | `src/client/domain/tank_motion_authority_pilot.cpp` | `CR-A3` |
| `src/client/game/tank_motion_authority_state_machine.hpp` | `src/client/domain/tank_motion_authority_state_machine.hpp` | `CR-A3` |
| `src/client/game/tank_motion_authority_state_machine.cpp` | `src/client/domain/tank_motion_authority_state_machine.cpp` | `CR-A3` |

#### Staged execution slices (ordered)

##### `CR-A2` Runtime-Orchestration Relocation
1. `CR-A2.1` Runtime entrypoint relocation.
   - Move: `game.hpp`, `lifecycle.cpp`, `audio.cpp`.
   - Update includes: `src/client/runtime/internal.hpp`, `src/client/runtime/run.cpp`.
   - Update CMake: `cmake/targets/sources.cmake` paths for moved `.cpp`.
2. `CR-A2.2` Runtime tank orchestration relocation.
   - Move: `tank_entity.cpp`, `tank_motion.cpp`, `tank_collision.cpp`, `tank_camera.cpp`, `tank_collision_runtime_query_context.hpp/.cpp`.
   - Update CMake: remaining runtime `.cpp` entries in `cmake/targets/sources.cmake`.
3. `CR-A2.3` Runtime internal include normalization.
   - Normalize moved-runtime includes to `client/runtime/*` and `client/domain/*`.
   - No compatibility include layer is allowed; converge all includes directly.

##### `CR-A3` Domain/Mechanics Relocation
1. `CR-A3.1` Utility/state header relocation.
   - Move: `math.hpp`, `score_state.hpp`, `shot_spawn.hpp`.
   - Update runtime + test includes.
2. `CR-A3.2` Collision mechanics relocation.
   - Move: `tank_collision_guardrails.*`, `tank_collision_query.*`, `tank_collision_probe_shape.*`, `tank_collision_resolution.*`, `tank_collision_step_stats.*`, `tank_camera_collision.*`.
   - Update internal collision includes and test includes.
   - Update CMake test source paths in `cmake/targets/tests.cmake`.
3. `CR-A3.3` Authority pilot relocation.
   - Move: `tank_motion_authority_pilot.*`, `tank_motion_authority_state_machine.*`.
   - Update runtime includes and pilot/state-machine test includes.
   - Update pilot/state-machine `.cpp` paths in `cmake/targets/tests.cmake`.

##### `CR-A4` CMake/Test/Doc Convergence
1. `CR-A4.1` CMake source-list convergence.
   - `cmake/targets/sources.cmake`: replace all `src/client/game/*` paths with runtime/domain destinations.
   - `cmake/targets/targets.cmake`: converge variable naming (`BZ3_CLIENT_GAME_SRCS` -> runtime/domain-aligned lists) and keep target composition behavior-neutral.
2. `CR-A4.2` Test/docs path convergence.
   - `cmake/targets/tests.cmake`: no `src/client/game/*` `.cpp` path references.
   - `src/tests/*`: no `#include "client/game/*"` includes.
   - `m-overseer` project docs/scripts: no stale `src/client/game/*` migration TODO placeholders.
3. `CR-A4.3` Legacy path purge.
   - Verify no includes or CMake/test references to `src/client/game/*` remain.
   - Delete `src/client/game/` subtree after build green.

### `CR-A2` Runtime-Orchestration Relocation
- Execute slices `CR-A2.1` -> `CR-A2.3` exactly in order.
- No gameplay/input/network behavior changes; path/include/CMake changes only.

#### `CR-A2` Progress (`2026-02-24`)
- `CR-A2.1` completed with direct convergence (no compatibility layer):
  - moved `src/client/game/game.hpp` -> `src/client/runtime/game.hpp`
  - moved `src/client/game/lifecycle.cpp` -> `src/client/runtime/lifecycle.cpp`
  - moved `src/client/game/audio.cpp` -> `src/client/runtime/audio.cpp`
  - updated all `#include "client/game/game.hpp"` callsites to `#include "client/runtime/game.hpp"` (runtime + remaining game translation units)
  - updated `cmake/targets/sources.cmake` to source `lifecycle.cpp` and `audio.cpp` from `src/client/runtime/*`
- Compatibility-artifact audit result for this slice:
  - no pre-existing forwarding headers/shims/dual-path include logic found for these files
  - no compatibility shims/forwarders introduced
  - no bridge files retained under `src/client/game/` for moved files
- `CR-A2.2` completed with direct convergence (no compatibility layer):
  - moved `src/client/game/tank_entity.cpp` -> `src/client/runtime/tank_entity.cpp`
  - moved `src/client/game/tank_motion.cpp` -> `src/client/runtime/tank_motion.cpp`
  - moved `src/client/game/tank_collision.cpp` -> `src/client/runtime/tank_collision.cpp`
  - moved `src/client/game/tank_camera.cpp` -> `src/client/runtime/tank_camera.cpp`
  - moved `src/client/game/tank_collision_runtime_query_context.hpp` -> `src/client/runtime/tank_collision_runtime_query_context.hpp`
  - moved `src/client/game/tank_collision_runtime_query_context.cpp` -> `src/client/runtime/tank_collision_runtime_query_context.cpp`
  - updated all `#include "client/game/tank_collision_runtime_query_context.hpp"` callsites to `#include "client/runtime/tank_collision_runtime_query_context.hpp"`
  - updated `cmake/targets/sources.cmake` to source the five moved runtime `.cpp` files from `src/client/runtime/*` and remove old `src/client/game/*` paths for these units
- Compatibility-artifact audit result for `CR-A2.2`:
  - no pre-existing forwarding headers/shims/dual-path include logic found for the six moved units
  - no compatibility shims/forwarders introduced
  - no bridge files retained under `src/client/game/` for moved units

### `CR-A3` Domain/Mechanics Relocation
- Execute slices `CR-A3.1` -> `CR-A3.3` after `CR-A2` builds green.
- Keep `bz3::client::game::*` namespaces unchanged in this phase.

#### `CR-A3` Progress (`2026-02-24`)
- `CR-A3.1` completed with direct convergence (no compatibility layer):
  - moved `src/client/game/math.hpp` -> `src/client/domain/math.hpp`
  - moved `src/client/game/score_state.hpp` -> `src/client/domain/score_state.hpp`
  - moved `src/client/game/shot_spawn.hpp` -> `src/client/domain/shot_spawn.hpp`
  - updated all include callsites from `client/game/{math,score_state,shot_spawn}.hpp` to `client/domain/{math,score_state,shot_spawn}.hpp` (runtime + tests)
  - no CMake source-list changes required (header-only relocation)
- Compatibility-artifact audit result for `CR-A3.1`:
  - no pre-existing forwarding headers/shims/dual-path include logic found for these three headers
  - no compatibility shims/forwarders introduced
  - no bridge files retained under `src/client/game/`
- `CR-A3.2` completed with direct convergence (no compatibility layer):
  - moved `src/client/game/tank_collision_guardrails.hpp/.cpp` -> `src/client/domain/tank_collision_guardrails.hpp/.cpp`
  - moved `src/client/game/tank_collision_query.hpp/.cpp` -> `src/client/domain/tank_collision_query.hpp/.cpp`
  - moved `src/client/game/tank_collision_probe_shape.hpp/.cpp` -> `src/client/domain/tank_collision_probe_shape.hpp/.cpp`
  - moved `src/client/game/tank_collision_resolution.hpp/.cpp` -> `src/client/domain/tank_collision_resolution.hpp/.cpp`
  - moved `src/client/game/tank_collision_step_stats.hpp/.cpp` -> `src/client/domain/tank_collision_step_stats.hpp/.cpp`
  - moved `src/client/game/tank_camera_collision.hpp/.cpp` -> `src/client/domain/tank_camera_collision.hpp/.cpp`
  - updated include callsites in runtime/tests from `client/game/*` to `client/domain/*` for the six relocated collision mechanics headers
  - updated `cmake/targets/sources.cmake` and `cmake/targets/tests.cmake` `.cpp` paths from `src/client/game/*` to `src/client/domain/*` for relocated units
- Compatibility-artifact audit result for `CR-A3.2`:
  - no pre-existing forwarding headers/shims/dual-path include logic found for these six collision mechanics units
  - no compatibility shims/forwarders introduced
  - no bridge files retained under `src/client/game/`
- `CR-A3.3` completed with direct convergence (no compatibility layer):
  - moved `src/client/game/tank_motion_authority_pilot.hpp/.cpp` -> `src/client/domain/tank_motion_authority_pilot.hpp/.cpp`
  - moved `src/client/game/tank_motion_authority_state_machine.hpp/.cpp` -> `src/client/domain/tank_motion_authority_state_machine.hpp/.cpp`
  - updated include callsites from `client/game/*` to `client/domain/*` in runtime (`game.hpp`, `tank_entity.cpp`, `tank_motion.cpp`) and authority tests
  - updated `cmake/targets/sources.cmake` and `cmake/targets/tests.cmake` `.cpp` paths from `src/client/game/*` to `src/client/domain/*` for authority pilot/state-machine units
- Compatibility-artifact audit result for `CR-A3.3`:
  - no pre-existing forwarding headers/shims/dual-path include logic found for authority pilot/state-machine units
  - no compatibility shims/forwarders introduced
  - no bridge files retained under `src/client/game/`

### `CR-A4` CMake/Test/Doc Convergence
- Execute slices `CR-A4.1` -> `CR-A4.3` after `CR-A3` builds green.
- Do not introduce compatibility artifacts in this track; each slice must fully converge include and CMake references for moved files.

#### `CR-A4` Progress (`2026-02-24`)
- `CR-A4.1` completed with direct convergence (no compatibility layer):
  - renamed `BZ3_CLIENT_GAME_SRCS` -> `BZ3_CLIENT_DOMAIN_SRCS` in `cmake/targets/sources.cmake`
  - updated `cmake/targets/targets.cmake` to consume `${BZ3_CLIENT_DOMAIN_SRCS}`
  - source membership preserved (naming/topology alignment only; no target behavior changes)
  - `cmake/targets` scan confirms no `src/client/game/` path references remain
- Compatibility-artifact audit result for `CR-A4.1`:
  - no compatibility shims/forwarders introduced
  - no dual-path include/target fallback logic introduced
- `CR-A4.2` completed with direct convergence (no compatibility layer):
  - updated `m-bz3/src/ui/architecture.md` stale path reference: `src/client/game/lifecycle.cpp` -> `src/client/runtime/lifecycle.cpp`
  - scoped audit in `src`, `cmake`, and `CMakeLists.txt` confirms no remaining `#include "client/game/*"` references
  - scoped audit in `src`, `cmake`, and `CMakeLists.txt` confirms no remaining `src/client/game/*` path references
  - `find src/client/game -maxdepth 1 -type f` confirms directory contains zero files (empty directory retained for `CR-A4.3`)
- Compatibility-artifact audit result for `CR-A4.2`:
  - no compatibility shims/forwarders introduced
  - no dual-path include/target fallback logic introduced
- `CR-A4.3` completed with direct convergence (no compatibility layer):
  - removed legacy empty directory `src/client/game/` via `rmdir src/client/game`
  - `test ! -d src/client/game` confirms directory no longer exists
  - final scoped audits in `src`, `cmake`, and `CMakeLists.txt` confirm zero `#include "client/game/*"` and zero `src/client/game/*` path references
- Compatibility-artifact audit result for `CR-A4.3`:
  - no compatibility shims/forwarders introduced
  - no dual-path include/path fallback logic introduced
- `CR-A4` completion statement:
  - `CR-A4.1` + `CR-A4.2` + `CR-A4.3` are fully landed; legacy client game-layer path migration is closed.

### `CR-A5` Optional Namespace Convergence
- If approved by operator, migrate `bz3::client::game::*` namespaces to topology-aligned names.
- Keep this as a separate pass after path migration is green.

#### `CR-A5` Progress (`2026-02-24`)
- `CR-A5` completed with direct convergence (no compatibility layer):
  - converged `bz3::client::game::detail` -> `bz3::client::domain::detail`
  - converged `bz3::client::game::collision` -> `bz3::client::domain::collision`
  - converged `bz3::client::game::pilot` -> `bz3::client::domain::pilot`
  - converged all `client::game::*` callsites to `client::domain::*` across `src/client/domain/*`, `src/client/runtime/*`, and `src/tests/*`
  - scoped audit confirms no residual `client::game::*` namespace/callsite references in `src`, `cmake`, or `CMakeLists.txt`
- Compatibility-artifact audit result for `CR-A5`:
  - no compatibility aliases/shims/forwarders introduced
  - no dual-namespace support introduced
  - no fallback logic introduced

## Validation
```bash
cd m-overseer
./agent/scripts/lint-projects.sh

cd ../m-bz3
export ABUILD_AGENT_NAME=<agent-name>
./abuild.py --claim-lock -d <bz3-build-dir>
./abuild.py -c -d <bz3-build-dir> --karma-sdk ../m-karma/out/karma-sdk
ctest --test-dir <bz3-build-dir> -R "tank_.*|client_.*|server_.*runtime.*" --output-on-failure
./abuild.py --release-lock -d <bz3-build-dir>
```

## Trace Channels
- `refactor.client.layout`
- `refactor.client.includes`
- `refactor.client.cmake`

## Build/Run Commands
```bash
cd m-bz3
./abuild.py -c -d <bz3-build-dir> --karma-sdk ../m-karma/out/karma-sdk
```

## Current Status
- `2026-02-23`: project doc created for specialist delegation.
- `2026-02-24`: `CR-A1` completed with exact inventory, include/CMake reference maps, concrete 28-file move table, and staged execution slices (`CR-A2`/`CR-A3`/`CR-A4`).
- `2026-02-24`: required validation commands passed:
  - `m-overseer`: `./agent/scripts/lint-projects.sh` -> `OK`.
  - `m-bz3`: lock claim + `abuild.py -c` + lock release in `build-client-refactor-a1` completed successfully.
- `2026-02-24`: operator policy lock applied: this refactor must not introduce compatibility shims, forwarding headers, fallback logic, or dual-path compatibility behavior.
- `2026-02-24`: `CR-A2.1` landed with direct runtime entrypoint relocation and no compatibility artifacts.
- `2026-02-24`: CR-A2.1 validation results:
  - `m-overseer` lint: pass
  - `m-bz3` `abuild.py -c`: pass
  - `ctest --test-dir build-client-refactor-a1 -R "tank_.*|client_.*|server_.*runtime.*" --output-on-failure`: `18/19` pass, `1` fail (`server_join_runtime_contract_test`: missing i18n key `feedback.server.runtime.joinRejectReasons.Default`)
  - old-path file checks for moved files: pass
  - `#include "client/game/game.hpp"` scan in `src`: no matches
  - build lock released successfully
- `2026-02-24`: `CR-A2.2` landed with direct runtime tank-orchestration/query-context relocation and no compatibility artifacts.
- `2026-02-24`: CR-A2.2 validation results:
  - `m-overseer` lint before + after: pass
  - `m-bz3` lock claim + `abuild.py -c` + lock release: pass
  - `ctest --test-dir build-client-refactor-a1 -R "tank_.*|client_.*|server_.*runtime.*" --output-on-failure`: `18/19` pass, only known baseline fail (`server_join_runtime_contract_test`: missing key `feedback.server.runtime.joinRejectReasons.Default`)
  - moved-path existence/absence checks for six `CR-A2.2` units: pass
  - `#include "client/game/tank_collision_runtime_query_context.hpp"` scan in `src`: no matches
  - `src/client/game/(tank_entity|tank_motion|tank_collision|tank_camera|tank_collision_runtime_query_context).(cpp|hpp)` scan in `cmake` + `src`: no matches
- `2026-02-24`: `CR-A3.1` landed with direct utility/state header relocation and no compatibility artifacts.
- `2026-02-24`: CR-A3.1 validation results:
  - `m-overseer` lint before + after: pass
  - `m-bz3` lock claim + `abuild.py -c` + lock release: pass
  - `ctest --test-dir build-client-refactor-a1 -R "tank_.*|client_.*|server_.*runtime.*" --output-on-failure`: `18/19` pass, only known baseline fail (`server_join_runtime_contract_test`: missing key `feedback.server.runtime.joinRejectReasons.Default`)
  - moved-path existence/absence checks for `math.hpp`, `score_state.hpp`, `shot_spawn.hpp`: pass
  - `#include "client/game/(math|score_state|shot_spawn).hpp"` scan in `src` + `cmake`: no matches
  - `#include "client/domain/(math|score_state|shot_spawn).hpp"` scan in runtime + tests: expected matches present
- `2026-02-24`: `CR-A3.2` landed with direct collision-mechanics relocation and no compatibility artifacts.
- `2026-02-24`: CR-A3.2 validation results:
  - `m-overseer` lint before + after: pass
  - `m-bz3` lock claim + `abuild.py -c` + lock release: pass
  - `ctest --test-dir build-client-refactor-a1 -R "tank_.*|client_.*|server_.*runtime.*" --output-on-failure`: `18/19` pass, only known baseline fail (`server_join_runtime_contract_test`: missing key `feedback.server.runtime.joinRejectReasons.Default`)
  - moved-path existence/absence checks for twelve `CR-A3.2` files: pass
  - `#include "client/game/(tank_collision_guardrails|tank_collision_query|tank_collision_probe_shape|tank_collision_resolution|tank_collision_step_stats|tank_camera_collision).hpp"` scan in `src` + `cmake`: no matches
  - `src/client/game/(tank_collision_guardrails|tank_collision_query|tank_collision_probe_shape|tank_collision_resolution|tank_collision_step_stats|tank_camera_collision).cpp` scan in `cmake/targets` + `src`: no matches
  - `#include "client/domain/(tank_collision_guardrails|tank_collision_query|tank_collision_probe_shape|tank_collision_resolution|tank_collision_step_stats|tank_camera_collision).hpp"` scan in runtime + tests: expected matches present
- `2026-02-24`: `CR-A3.3` landed with direct authority pilot/state-machine relocation and no compatibility artifacts.
- `2026-02-24`: CR-A3.3 validation results:
  - `m-overseer` lint before + after: pass
  - `m-bz3` lock claim + `abuild.py -c` + lock release: pass
  - `ctest --test-dir build-client-refactor-a1 -R "tank_.*|client_.*|server_.*runtime.*" --output-on-failure`: `18/19` pass, only known baseline fail (`server_join_runtime_contract_test`: missing key `feedback.server.runtime.joinRejectReasons.Default`)
  - moved-path existence/absence checks for four `CR-A3.3` files: pass
  - `#include "client/game/(tank_motion_authority_pilot|tank_motion_authority_state_machine).hpp"` scan in `src` + `cmake`: no matches
  - `src/client/game/(tank_motion_authority_pilot|tank_motion_authority_state_machine).cpp` scan in `cmake/targets` + `src`: no matches
  - `#include "client/domain/(tank_motion_authority_pilot|tank_motion_authority_state_machine).hpp"` scan in runtime + tests: expected matches present
- `2026-02-24`: `CR-A4.1` landed with CMake source-list variable convergence and no compatibility artifacts.
- `2026-02-24`: CR-A4.1 validation results:
  - `m-overseer` lint before + after: pass
  - `m-bz3` lock claim + `abuild.py -c` + lock release: pass
  - `ctest --test-dir build-client-refactor-a1 -R "tank_.*|client_.*|server_.*runtime.*" --output-on-failure`: `18/19` pass, only known baseline fail (`server_join_runtime_contract_test`: missing key `feedback.server.runtime.joinRejectReasons.Default`)
  - `BZ3_CLIENT_GAME_SRCS` scan in `cmake/targets`: no matches
  - `BZ3_CLIENT_DOMAIN_SRCS` scan in `cmake/targets/sources.cmake` + `cmake/targets/targets.cmake`: expected matches present
  - `src/client/game/` scan in `cmake/targets`: no matches
  - non-CMake stale reference discovered for CR-A4.2 follow-up: `m-bz3/src/ui/architecture.md:68` references `src/client/game/lifecycle.cpp`
- `2026-02-24`: `CR-A4.2` landed with test/docs path convergence and no compatibility artifacts.
- `2026-02-24`: CR-A4.2 validation results:
  - `m-overseer` lint before + after: pass
  - `m-bz3` lock claim + `abuild.py -c` + lock release: pass
  - `ctest --test-dir build-client-refactor-a1 -R "tank_.*|client_.*|server_.*runtime.*" --output-on-failure`: `18/19` pass, only known baseline fail (`server_join_runtime_contract_test`: missing key `feedback.server.runtime.joinRejectReasons.Default`)
  - `#include "client/game/` scan in `src` + `cmake` + `CMakeLists.txt`: no matches
  - `src/client/game/` path scan in `src` + `cmake` + `CMakeLists.txt`: no matches
  - `src/ui/architecture.md` stale path scan: no matches
  - `find src/client/game -maxdepth 1 -type f`: no output (directory empty; ready for `CR-A4.3` purge)
- `2026-02-24`: `CR-A4.3` landed with legacy directory purge and no compatibility artifacts.
- `2026-02-24`: CR-A4.3 validation results:
  - `m-overseer` lint before + after: pass
  - `m-bz3` lock claim + `abuild.py -c` + lock release: pass
  - `rmdir src/client/game`: pass
  - `test ! -d src/client/game`: pass
  - `ctest --test-dir build-client-refactor-a1 -R "tank_.*|client_.*|server_.*runtime.*" --output-on-failure`: `18/19` pass, only known baseline fail (`server_join_runtime_contract_test`: missing key `feedback.server.runtime.joinRejectReasons.Default`)
  - `#include "client/game/` scan in `src` + `cmake` + `CMakeLists.txt`: no matches
  - `src/client/game/` path scan in `src` + `cmake` + `CMakeLists.txt`: no matches
  - `src/client/game/` directory existence check: directory absent
- `2026-02-24`: `CR-A5` landed with namespace convergence and no compatibility artifacts.
- `2026-02-24`: CR-A5 validation results:
  - `m-overseer` lint before + after: pass
  - `m-bz3` lock claim + `abuild.py -c` + lock release: pass
  - `ctest --test-dir build-client-refactor-a1 -R "tank_.*|client_.*|server_.*runtime.*" --output-on-failure`: `18/19` pass, only known baseline fail (`server_join_runtime_contract_test`: missing key `feedback.server.runtime.joinRejectReasons.Default`)
  - `client::game::|namespace bz3::client::game` scan in `src` + `cmake` + `CMakeLists.txt`: no matches
  - `client::domain::|namespace bz3::client::domain` scan in domain/runtime/tests: expected matches present
  - `#include "client/game/` scan in `src` + `cmake` + `CMakeLists.txt`: no matches
  - `src/client/game/` path scan in `src` + `cmake` + `CMakeLists.txt`: no matches

## Open Questions
- Baseline test debt remains external to topology migration: `server_join_runtime_contract_test` still fails on missing key `feedback.server.runtime.joinRejectReasons.Default`.

## Handoff Checklist
- [x] `CR-A1` inventory + move map + staged slice map documented
- [x] `src/client/game/*` removed
- [x] CMake/test wiring updated
- [x] `CR-A2.1` runtime entrypoint relocation landed (`game.hpp`, `lifecycle.cpp`, `audio.cpp`)
- [x] No compatibility artifacts introduced or retained for CR-A2.1
- [x] `CR-A2.2` runtime tank-orchestration/query-context relocation landed
- [x] No compatibility artifacts introduced or retained for CR-A2.2
- [x] `CR-A2` runtime relocation landed
- [x] `CR-A3.1` utility/state header relocation landed (`math.hpp`, `score_state.hpp`, `shot_spawn.hpp`)
- [x] No compatibility artifacts introduced or retained for CR-A3.1
- [x] `CR-A3.2` collision mechanics relocation landed (`tank_collision_guardrails.*`, `tank_collision_query.*`, `tank_collision_probe_shape.*`, `tank_collision_resolution.*`, `tank_collision_step_stats.*`, `tank_camera_collision.*`)
- [x] No compatibility artifacts introduced or retained for CR-A3.2
- [x] `CR-A3.3` authority pilot/state-machine relocation landed (`tank_motion_authority_pilot.*`, `tank_motion_authority_state_machine.*`)
- [x] No compatibility artifacts introduced or retained for CR-A3.3
- [x] `CR-A3` domain relocation landed
- [x] `CR-A4.1` CMake source-list variable convergence landed (`BZ3_CLIENT_GAME_SRCS` -> `BZ3_CLIENT_DOMAIN_SRCS`)
- [x] No compatibility artifacts introduced or retained for CR-A4.1
- [x] `CR-A4.2` test/docs path convergence landed (stale non-CMake `src/client/game/*` references removed)
- [x] No compatibility artifacts introduced or retained for CR-A4.2
- [x] `CR-A4.3` legacy directory purge landed (`src/client/game/` removed)
- [x] No compatibility artifacts introduced or retained for CR-A4.3
- [x] `CR-A4` convergence + legacy path purge landed
- [x] `CR-A5` namespace convergence landed (`client::game::*` -> `client::domain::*`)
- [x] No compatibility artifacts introduced or retained for CR-A5
- [x] `client-topology-refactor-a` migration work closed in `m-bz3`
- [x] Validation commands run and summarized
- [x] Remaining naming debt and risks documented
