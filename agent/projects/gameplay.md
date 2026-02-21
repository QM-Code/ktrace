# Gameplay Playable Loop (Localhost)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (new active bring-up track; prior migration track retired)`
- Immediate next task: dispatch first bounded implementation slice `GP-S1` (join/play defaults + remove non-parity actor tick drift).
- Validation gate:
  - `m-overseer`: `./scripts/lint-projects.sh`
  - `m-bz3`: `./abuild.py -c -d <bz3-build-dir>`, `./scripts/test-server-net.sh <bz3-build-dir>`, targeted `ctest` packet for touched contracts
  - `m-karma` (only when backend seam touched): `./abuild.py -c -d <karma-build-dir>`, `./scripts/test-engine-backends.sh <karma-build-dir>`

## Mission
Deliver a playable localhost loop where these six outcomes work with default runtime commands:

```bash
# server
bz3-server --port <port>

# client
bz3 --server <host:port>
```

Target outcomes:
0. client connects and can join (auto-assigned name when none provided, e.g. `player14`)
1. player drives tank in first-person play mode
2. player can shoot and kill other players
3. running score is server-authoritative
4. shots ricochet off buildings
5. tanks can jump and land on buildings

## Foundation References
- `m-overseer/agent/projects/archive/gameplay-retired-2026-02-21.md`
- `m-bz3/src/server/runtime/server_game.cpp`
- `m-bz3/src/server/runtime/shot_pilot_step.cpp`
- `m-bz3/src/server/domain/shot_system.cpp`
- `m-bz3/src/client/game/lifecycle.cpp`
- `m-bz3/src/client/net/connection/outbound.cpp`
- `m-karma/include/karma/physics/backend.hpp`
- `m-karma/src/physics/facade/player_controller.cpp`

## Why This Is Separate
Previous gameplay migration/phasing work (D2/D3/G4/G5/P6) was completed and archived.  
This track is a focused playable product loop bring-up across gameplay/runtime seams, not transport-contract migration.

## Comprehensive Findings (Current State)

### Outcome Status Matrix
| Outcome | Current posture | Evidence | Gap |
|---|---|---|---|
| `0` Join + auto name | **partial** | server fallback name is deterministic `player-<client_id>` when join name is empty: `m-bz3/src/server/runtime/server_game.cpp:208`; client normally sends configured username (`userDefaults.username`): `m-bz3/src/client/runtime/startup_options.cpp:23`, `m-bz3/data/client/config.json:8`; duplicate active names are rejected: `m-bz3/src/server/runtime/server_game.cpp:212` | default client behavior does not reliably produce auto-assigned unique names when no explicit name is passed |
| `1` First-person tank drive | **partial** | tank mode exists but default config disables it: `m-bz3/data/client/config.json:142`; startup reads it default false: `m-bz3/src/client/game/lifecycle.cpp:26`; observer/roaming remains fallback when no tank entity: `m-bz3/src/client/game/lifecycle.cpp:245` | default launch path lands in observer mode, not play mode |
| `2` Shoot + kill | **partial** | authoritative shot/damage/death pipeline exists (`D3/G4/G5` landed); server kill flow emits death + score event: `m-bz3/src/server/runtime/shot_pilot_step.cpp:128`; but client shot send currently uses zero pos/vel: `m-bz3/src/client/net/connection/outbound.cpp:74`; server applies incoming shot vectors directly: `m-bz3/src/server/runtime/event_rules.cpp:65` | gameplay shot spawn/velocity from local tank/camera is not wired |
| `3` Server-authoritative running score | **partial** | server mutates session score on kill: `m-bz3/src/server/runtime/shot_damage.cpp:58`; server broadcasts `SetScore`: `m-bz3/src/server/net/transport_event_source/events.cpp:27` | client receive seam is trace-only, not HUD/game-state presentation: `m-bz3/src/client/net/connection/inbound/session_events.cpp:53` |
| `4` Ricochet off buildings | **not implemented** | shot currently expires on first non-ignored physics hit: `m-bz3/src/server/domain/shot_system.cpp:163` | no bounce/reflection model or ricochet lifecycle state |
| `5` Jump + land on buildings | **not implemented** | `jump` input is declared: `m-bz3/src/ui/console/keybindings.cpp:15`; local tank motion does not consume jump action: `m-bz3/src/client/game/tank_motion.cpp:28`; tank/actor gravity currently disabled in key paths: `m-bz3/src/client/game/tank_entity.cpp:152`, `m-bz3/src/server/runtime/server_game.cpp:357` | no gameplay jump impulse, airtime state, landing rules, or vertical physics behavior |

### Additional High-Impact Findings
1. Non-parity actor debug behavior is still active in server tick path and can corrupt gameplay expectations:
- `m-bz3/src/server/domain/actor_system.cpp:123` moves actors each tick via synthetic sinusoid.
- `m-bz3/src/server/domain/actor_system.cpp:126` continuously drains health.

2. `m-karma` backend is largely sufficient for baseline gameplay integration:
- backend already exposes raycast, gravity toggle, force/impulse, and velocity APIs: `m-karma/include/karma/physics/backend.hpp:95`.

3. `m-karma` seams still likely needed for full-quality parity:
- `RaycastHit` currently has no surface normal field (`body`, `position`, `distance`, `fraction` only): `m-karma/include/karma/physics/backend.hpp:79`.
- `PlayerController::isGrounded()` is intentionally deferred: `m-karma/src/physics/facade/player_controller.cpp:185`.

## Backend Readiness Verdict
- `m-karma` is **ready enough** for outcomes `0`-`3` and likely `5` with game-owned grounded rules.
- `m-bz3` remains the primary implementation surface for playable loop behavior.
- For robust ricochet (`4`), a minimal `m-karma` backend/query contract extension for hit normals is the likely clean path.

## Strategic Track Labels
- Primary track: `m-dev parity`
- Secondary track: `shared unblocker` (for backend seam additions required by gameplay contracts)

## Owned Paths
- `m-overseer/agent/projects/gameplay.md`
- `m-overseer/agent/projects/ASSIGNMENTS.md`
- `m-bz3/src/client/game/*`
- `m-bz3/src/client/net/*`
- `m-bz3/src/server/*`
- `m-bz3/src/tests/*`
- `m-karma/include/karma/physics/*` and backend impl/test paths only when explicitly required by a slice

## Interface Boundaries
- Keep gameplay semantics in `m-bz3` game/runtime/domain layers.
- Keep `m-karma` changes generic (no BZ3-specific rules).
- Preserve server authority for spawn, shot lifecycle, damage/death, and score mutation.
- No direct backend leakage into user-facing client game APIs.

## Non-Goals
- No broad rendering or UI-system refactor.
- No transport harness overreach beyond gameplay requirements.
- No reopening archived migration packets unless a direct regression is found.
- No feature scope expansion beyond outcomes `0`-`5`.

## Execution Plan (Bounded Slices)

### `GP-S1` (first)
- Track: `m-dev parity`
- Goal: default into playable join flow and remove non-parity actor tick drift.
- Scope:
  - ensure client join path can rely on server-side auto name assignment when explicit name is absent.
  - align fallback auto-name format to target style (`playerNN`).
  - default local flow to play mode/tank-enabled for network play launch.
  - remove synthetic server actor tick drift/health drain behavior from gameplay runtime.
  - keep spawn authority model deterministic.
- Intended files:
  - `m-bz3/src/client/runtime/startup_options.cpp`
  - `m-bz3/src/client/game/lifecycle.cpp`
  - `m-bz3/src/server/runtime/server_game.cpp`
  - `m-bz3/src/server/domain/actor_system.cpp`
  - `m-bz3/src/tests/server_runtime_lifecycle_contract_test.cpp`
  - `m-bz3/src/tests/server_runtime_event_rules_test.cpp`
  - `m-bz3/src/tests/client_runtime_cli_contract_test.cpp`
- Acceptance:
  - default `bz3 --server <host:port>` joins reliably without manual username override.
  - default post-join state enters play-mode path (not observer-only default).
  - no synthetic health drain or sinusoidal drift in authoritative actor tick.

### `GP-S2`
- Track: `m-dev parity`
- Goal: authoritative shot creation from actual tank/camera state and kill loop completion.
- Scope:
  - compute/send non-zero shot spawn position + velocity from local gameplay state.
  - preserve local prediction + authoritative reconciliation seams.
  - verify kill path remains server-authoritative.
- Intended files:
  - `m-bz3/src/client/game/*` (shot origin/aim seam)
  - `m-bz3/src/client/net/connection/outbound.cpp`
  - `m-bz3/src/server/runtime/event_rules.cpp` (validation only if needed)
  - `m-bz3/src/tests/client_shot_reconciliation_test.cpp`
  - `m-bz3/src/tests/server_runtime_shot_damage_integration_test.cpp`
- Acceptance:
  - shots are visibly emitted from tank context and can kill targets through existing authority path.

### `GP-S3`
- Track: `m-dev parity`
- Goal: complete running score gameplay surface.
- Scope:
  - wire `ServerMsg_SetScore` into client gameplay state/HUD.
  - keep server as sole score source of truth.
- Intended files:
  - `m-bz3/src/client/net/connection/inbound/session_events.cpp`
  - `m-bz3/src/client/game/*` (state + HUD binding)
  - `m-bz3/src/tests/*score*` and/or runtime contract tests
- Acceptance:
  - score changes reflect in client HUD/state only from authoritative server messages.

### `GP-S4`
- Track: `shared unblocker` then `m-dev parity`
- Goal: ricochet behavior for shots against buildings.
- Scope:
  - add bounded shot ricochet state in server shot domain (bounce count/energy/termination).
  - add/use hit-normal query seam for reflection.
  - if needed, extend `m-karma` raycast hit contract to include normal vector.
- Intended files:
  - `m-bz3/src/server/domain/shot_system.*`
  - `m-bz3/src/server/runtime/shot_pilot_step.cpp`
  - `m-bz3/src/tests/server_runtime_shot_physics_integration_test.cpp`
  - optional `m-karma/include/karma/physics/backend.hpp` + backend implementations/tests
- Acceptance:
  - deterministic ricochet occurs on building hits with bounded bounce lifecycle and removal rules.

### `GP-S5`
- Track: `m-dev parity`
- Goal: jump + landing on building geometry.
- Scope:
  - consume jump input in tank motion with server-authoritative state.
  - enable vertical physics path (gravity/impulse) for tank actor semantics.
  - define landing/grounded contract (game-owned; optionally backend-assisted).
- Intended files:
  - `m-bz3/src/client/game/tank_motion.cpp`
  - `m-bz3/src/client/game/tank_entity.cpp`
  - `m-bz3/src/server/runtime/server_game.cpp`
  - `m-bz3/src/server/runtime/event_loop.cpp`
  - `m-bz3/src/tests/*tank*` + lifecycle/physics integration tests
  - optional `m-karma` grounded/query seam if strictly required
- Acceptance:
  - tank can jump, arc, collide, and land on buildings with deterministic server authority.

## Validation
```bash
# m-bz3 required per implementation slice
cd m-bz3
export ABUILD_AGENT_NAME=specialist-gameplay-loop
./abuild.py --claim-lock -d <bz3-build-dir>
./abuild.py -c -d <bz3-build-dir>
./scripts/test-server-net.sh <bz3-build-dir>

# targeted ctest packet (expand per slice scope)
ctest --test-dir <bz3-build-dir> -R "server_net_contract_test|server_runtime_event_rules_test|server_runtime_lifecycle_contract_test|server_runtime_shot_damage_integration_test|server_runtime_shot_physics_integration_test|server_round_phase_contract_test|client_runtime_cli_contract_test|client_shot_reconciliation_test" --output-on-failure

# m-karma only when backend/query contracts change
cd ../m-karma
export ABUILD_AGENT_NAME=specialist-gameplay-loop
./abuild.py --claim-lock -d <karma-build-dir>
./abuild.py -c -d <karma-build-dir>
./scripts/test-engine-backends.sh <karma-build-dir>
./abuild.py --release-lock -d <karma-build-dir>
```

### Manual Localhost Exit Criteria (required before retirement)
1. Start server:
```bash
./<build-dir>/bz3-server --port 11899
```
2. Start two clients:
```bash
./<build-dir>/bz3 --server 127.0.0.1:11899
./<build-dir>/bz3 --server 127.0.0.1:11899
```
3. Verify outcomes `0`-`5` end-to-end in one runtime session with trace evidence.

## Current Status
- `2026-02-21`: New active project doc created for playable localhost loop bring-up after migration-track retirement.
- `2026-02-21`: Baseline findings captured: join/name fallback mismatch, observer-by-default startup, shot send vector gap, score display seam missing, ricochet/jump not implemented, and residual non-parity actor tick behavior.
- `2026-02-21`: Backend readiness clarified: `m-karma` largely sufficient for baseline gameplay, with likely raycast-normal seam needed for robust ricochet.

## Open Questions
- Should fallback auto-name be deterministic by `client_id` (`player14`) or pseudo-random unique (`player83`)?
- Should spawn become automatic on successful join, or remain explicit input with immediate UX guidance?
- For ricochet, do we require physically-correct normal reflection first packet, or accept a temporary simplified bounce model?

## Handoff Checklist
- [x] Comprehensive findings documented with concrete code evidence.
- [ ] `GP-S1` landed and validated.
- [ ] `GP-S2` landed and validated.
- [ ] `GP-S3` landed and validated.
- [ ] `GP-S4` landed and validated.
- [ ] `GP-S5` landed and validated.
- [ ] Manual localhost end-to-end verification for outcomes `0`-`5`.
- [ ] Archive this project after completion and update `ASSIGNMENTS.md`.
