# Gameplay Migration

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (G2 + D1 landed + D1 hardening landed; D2 next)`
- Immediate next task: execute D2 movement-replication slice by wiring client `PlayerLocation` intent and server-authoritative tank drive state updates.
- Validation gate: `./docs/scripts/lint-project-docs.sh` for planning/doc slices; for code slices, run `./scripts/test-server-net.sh <build-dir>` with assigned build-dir args.

## Mission
Port game-specific behavior from `m-dev` into `m-rewrite` under rewrite-owned architecture boundaries:
- keep engine lifecycle/subsystems engine-owned,
- keep gameplay rules/protocol semantics game-owned,
- avoid engine/backend leakage across the boundary during migration.

## Foundation References
- `docs/foundation/policy/rewrite-invariants.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/architecture/core-engine-contracts.md`
- `docs/projects/gameplay-netcode.md`
- `docs/projects/ui-integration.md`

## Why This Is Separate
This migration is cross-cutting and risk-heavy:
- UI/HUD/console migration depends on `ui-integration.md` completion.
- Gameplay-rule migration spans multiple legacy `m-dev` locations and requires strict extraction discipline to avoid boundary violations.

## Owned Paths
- `docs/projects/gameplay-migration.md`
- `docs/projects/ASSIGNMENTS.md`
- `src/game/*` (new rewrite-owned gameplay implementations)
- `include/karma/*` and `src/engine/*` only when required contract seams are explicitly approved and coordinated

## Interface Boundaries
- Inputs consumed:
  - read-only reference behavior from `../m-dev/src/game/ui/*`
  - read-only reference behavior from `../m-dev/src/game/client/*`
  - read-only reference behavior from `../m-dev/src/game/server/*`
  - read-only reference behavior from `../m-dev/src/engine/*` only for extracting misplaced gameplay semantics
- Outputs exposed:
  - rewrite-owned gameplay implementations in `m-rewrite/src/game/*`
  - explicit seam-change proposals when engine contracts are insufficient
- Coordinate before changing:
  - `src/game/protos/messages.proto`
  - `src/game/net/protocol.hpp`
  - `src/game/net/protocol_codec.cpp`
  - `src/engine/CMakeLists.txt`
  - `docs/foundation/architecture/core-engine-contracts.md`

## Non-Goals
- Do not mirror `m-dev` file layout.
- Do not place gameplay-rule semantics in `src/engine/*`.
- Do not expose backend APIs/types directly in `src/game/*`.
- Do not expand unrelated renderer/audio/physics scope while migrating gameplay rules.

## Migration Tracks
1. Track A (blocked until UI completion): HUD/console/chat/scoreboard presentation migration from `m-dev/src/game/ui/*` after `docs/projects/ui-integration.md` closeout.
2. Track B (active now): gameplay-rule extraction and migration (shots, hit attribution, scoring/scoreboard triggers, round state) from `m-dev` into rewrite game-owned modules.
3. Track C (shared unblocker): drivable-tank baseline and movement replication seam before expanding hit/score semantics.

Strategic alignment:
- Primary track label: `m-dev parity`.
- Secondary label when applicable: `shared unblocker` for seam work needed by both gameplay migration and netcode parity.

## Execution Plan
1. G1 discovery ledger: map current shot/hit/scoreboard/round semantics from `m-dev` paths to rewrite target paths and boundary category.
2. G2 shot lifecycle migration: move shot creation/ownership/lifecycle semantics into explicit rewrite game-owned modules.
3. D1 drivable-tank baseline slice: stand up rewrite-owned local tank drive controller + visible tank entity + follow camera in client gameplay loop.
4. D2 movement replication slice: add client `PlayerLocation` intent + server authoritative movement/event handling seam for tank state.
5. G3 hit attribution migration: migrate authoritative hit resolution and attacker/victim attribution rules.
6. G4 scoring and scoreboard trigger migration: migrate kill/score events and scoreboard state transitions.
7. G5 round lifecycle migration: migrate round start/end/spawn/respawn control flow.
8. G6 UI migration track: once `ui-integration.md` closes, port HUD/console/chat/scoreboard UI behaviors from `m-dev/src/game/ui/*`.

## Migration Ledger (G1 Populated)
| Domain | Legacy source path(s) in `m-dev` | Rewrite target path(s) | Boundary type | Status |
|---|---|---|---|---|
| Shots | `src/game/client/player.cpp`, `src/game/client/shot.cpp`, `src/game/client/shot.hpp`, `src/game/server/game.cpp`, `src/game/server/shot.cpp`, `src/game/server/shot.hpp`, `src/game/net/messages.hpp`, plus engine leakage in `src/engine/graphics/backends/bgfx/backend.cpp` and `src/engine/graphics/backends/diligent/backend.cpp` (`shot.glb` special-casing) | Existing: `src/game/client/net/client_connection.*`, `src/game/server/runtime_event_rules.cpp`, `src/game/server/runtime.cpp`, `src/game/net/protocol_codec.*`; Landed in G2: `src/game/server/domain/shot_system.hpp`, `src/game/server/domain/shot_system.cpp`, `src/game/server/domain/shot_types.hpp` | game-owned with explicit engine-leak cleanup seam | `G2 landed` |
| Tank locomotion baseline | `src/game/client/player.cpp` (movement intent, controller, camera follow), `src/game/client/actor.hpp` | Landed in D1: `src/game/client/domain/tank_drive_controller.hpp`, `src/game/client/domain/tank_drive_controller.cpp`, `src/game/game.cpp`; proof in `src/game/tests/tank_drive_controller_test.cpp` | game-owned over existing engine render/input contracts | `D1 landed` |
| Hit attribution | `src/game/server/shot.cpp` (`hits`), `src/game/server/game.cpp` (victim/killer resolution), `src/game/server/plugin.cpp` (`killPlayer`) | New: `src/game/server/domain/combat_system.hpp`, `src/game/server/domain/combat_system.cpp`; integration in `src/game/server/runtime.cpp` + `src/game/server/server_game.cpp` | game-owned | `queued (G3)` |
| Scoreboard/scoring | `src/game/server/client.cpp` (`setScore`), `src/game/server/game.cpp` (authoritative +/-), `src/game/client/actor.hpp`, `src/game/client/game.cpp` (scoreboard assembly), UI consumption in `src/game/ui/core/system.cpp` | New: `src/game/server/domain/score_system.hpp`, `src/game/server/domain/score_system.cpp`; component extension in `src/game/server/domain/components.hpp`; client/UI bridge in `src/game/game.cpp` once `ui-integration` closes | game-owned semantics over engine UI contracts | `queued (G4)` |
| Round lifecycle | `src/game/server/client.cpp` (`trySpawn`, `die`), `src/game/server/world_session.cpp` (`pickSpawnLocation`), `src/game/server/game.cpp` (spawn request consume), `src/game/client/player.cpp` (spawn request when dead) | Existing scaffold: `src/game/server/runtime_event_rules.cpp`, `src/game/server/runtime.cpp`, `src/game/server/domain/world_session.*`; New: `src/game/server/domain/spawn_system.hpp`, `src/game/server/domain/spawn_system.cpp` | game-owned | `partially scaffolded; queued (G5)` |
| HUD/console/chat | `src/game/ui/*` | `src/game/*` + existing UI hooks | game-owned over engine UI contracts | blocked on `ui-integration` |

## G1 Findings (Boundary Risks)
1. Shot gameplay rules are split across client render/input, server authority loop, and protocol message flow, so a direct file-for-file port would reintroduce coupling.
2. `m-dev/src/engine/graphics/backends/bgfx/backend.cpp` and `m-dev/src/engine/graphics/backends/diligent/backend.cpp` contain game-asset special-casing (`shot.glb` -> `"shot"` theme slot), which is a game concern and must not be reintroduced into rewrite engine backends.
3. Rewrite already has a partial shots foundation (`local_shot_id` transport + server `next_global_shot_id` assignment), but no rewrite-owned server shot lifecycle system yet.
4. Scoreboard/scoring in `m-dev` is tightly coupled to actor state updates and UI rendering, so migration must split semantic state updates (gameplay track) from presentation (UI track).

## G2 Slice (Landed 2026-02-13)
Goal:
- Introduce a rewrite-owned server `ShotSystem` that owns active-shot lifecycle state and integrates with existing runtime event flow, without migrating hit/scoring rules yet.

Scope:
- Add:
  - `src/game/server/domain/shot_types.hpp`
  - `src/game/server/domain/shot_system.hpp`
  - `src/game/server/domain/shot_system.cpp`
- Wire in:
  - `src/game/server/runtime.cpp` (register create-shot events into `ShotSystem`, tick system each frame, emit remove-shot notifications on expiry)
  - `src/game/server/server_game.cpp` (host/update hook if needed for domain ownership consistency)
- Preserve existing transport/protocol contracts in:
  - `src/game/server/net/event_source.hpp`
  - `src/game/server/net/transport_event_source.cpp`
  - `src/game/net/protocol_codec.*`
- No hit attribution, no score mutation, no UI changes in G2.

Acceptance:
1. Runtime still accepts `ClientCreateShot` events and emits `CreateShot` broadcasts.
2. Active shots are tracked in one rewrite-owned server domain system.
3. Expired shots produce deterministic remove-shot emits.
4. `./scripts/test-server-net.sh <build-dir>` remains green for assigned build dirs.

Implementation landed:
- Added `src/game/server/domain/shot_types.hpp`.
- Added `src/game/server/domain/shot_system.hpp`.
- Added `src/game/server/domain/shot_system.cpp`.
- Wired shot lifecycle + expiry remove broadcasts in `src/game/server/runtime.cpp`.
- Extended server event-source contract with remove-shot callback in `src/game/server/net/event_source.hpp`.
- Implemented remove-shot transport broadcast in `src/game/server/net/transport_event_source.cpp`.
- Added protobuf encode helper `EncodeServerRemoveShot` in `src/game/net/protocol_codec.*`.
- Added remove-shot round-trip coverage in `src/game/tests/server_net_contract_test.cpp`.

## D1 Slice (Landed 2026-02-14)
Goal:
- Prove in-game drivable tank behavior in rewrite client before further shot/hit extraction, while staying inside rewrite-owned game paths.

Scope landed:
- Added `src/game/client/domain/tank_drive_controller.hpp`.
- Added `src/game/client/domain/tank_drive_controller.cpp`.
- Integrated local tank entity lifecycle + drive controls + follow camera in `src/game/game.cpp`.
- Added deterministic movement contract test `src/game/tests/tank_drive_controller_test.cpp`.
- Wired build/test target in `src/game/CMakeLists.txt`.

Acceptance:
1. A visible local tank entity is spawned from `assets.models.playerModel` when gameplay starts.
2. Arrow-key movement (`moveForward/moveBackward/moveLeft/moveRight`) drives tank pose through rewrite-owned game logic.
3. Follow camera tracks tank pose in gameplay mode.
4. Movement math is proven by `tank_drive_controller_test` in both assigned build dirs.

## Validation
From `m-rewrite/`:

```bash
# planning/doc-only slices
./docs/scripts/lint-project-docs.sh

# code-touch slices in this project
./scripts/test-server-net.sh <build-dir>
./<build-dir>/src/game/tank_drive_controller_test
```

## Trace Channels
- `net.client`
- `net.server`
- `input.events`
- `engine.server`

## Build/Run Commands
```bash
./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio
./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio
./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio
./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio
```

## First Session Checklist
1. Read `AGENTS.md`, then `docs/foundation/policy/execution-policy.md`, then this file.
2. Build G1 extraction ledger from `m-dev` by domain and boundary category.
3. Propose one smallest safe code slice (movement/authority seam or shots/hits) with owned rewrite paths only.
4. Execute required validation for touched scope.
5. Update this file and `docs/projects/ASSIGNMENTS.md` in the same handoff.

## Active Specialist Packet (D2)
```text
Execution root:
- Standalone mode: if you are already in `m-rewrite` repo root, use unprefixed paths below.
- Integration mode: if you start at workspace root (`bz3-rewrite/`), run `cd m-rewrite` first.
- If you stay at integration workspace root, prefix every path below with `m-rewrite/`.

Read in order:
1) AGENTS.md
2) docs/foundation/policy/execution-policy.md
3) docs/projects/AGENTS.md
4) docs/projects/ASSIGNMENTS.md
5) docs/projects/gameplay-migration.md
6) docs/projects/gameplay-netcode.md

Take ownership of: docs/projects/gameplay-migration.md

Goal:
- Implement D2 movement replication slice: wire client movement intent (`PlayerLocation`) to rewrite server-authoritative movement state updates.

Scope:
- Add/extend movement protocol/event handling in:
  - src/game/net/protocol_codec.*
  - src/game/server/net/event_source.hpp
  - src/game/server/net/transport_event_source.cpp
  - src/game/server/runtime_event_rules.*
  - src/game/server/runtime.cpp
- Keep existing shot contracts stable:
  - src/game/server/domain/shot_system.*
- In this slice: migrate movement intent ingestion + authoritative position update seam only.
- Do not redesign hit/scoring/UI mapping yet.

Strategic alignment (required):
- Track: shared unblocker.
- This slice unblocks gameplay parity by establishing a clean movement authority seam before deeper shot/hit migration.

Constraints:
- Stay within owned paths and interface boundaries in docs/projects/gameplay-migration.md.
- No unrelated subsystem changes.
- Preserve engine/game and backend exposure boundaries from AGENTS.md.
- Treat `KARMA-REPO` as capability reference only (never structure/layout template).
- Use bzbuild.py only. Do not run raw cmake -S/-B directly.
- Treat missing/unbootstrapped local `./vcpkg` as a hard blocker for delegated build/test execution.
- Use only assigned build dirs:
  - build-sdl3-bgfx-jolt-rmlui-miniaudio
  - build-sdl3-bgfx-physx-rmlui-miniaudio

Validation (required):
- ./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio
- ./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio
- ./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio
- ./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio
- ./build-sdl3-bgfx-jolt-rmlui-miniaudio/src/game/tank_drive_controller_test
- ./build-sdl3-bgfx-physx-rmlui-miniaudio/src/game/tank_drive_controller_test
- ./docs/scripts/lint-project-docs.sh

Docs updates (required):
- Update docs/projects/gameplay-migration.md Project Snapshot + status/handoff checklist.
- Update docs/projects/ASSIGNMENTS.md row (owner/status/next-task/last-update).

Handoff must include:
- files changed
- exact commands run + results
- remaining risks/open questions
```

## Current Status
- `2026-02-13`: project created for m-dev game-specific migration planning and sequencing.
- `2026-02-13`: Track A (UI/HUD/console) marked blocked pending `docs/projects/ui-integration.md` completion.
- `2026-02-13`: Track B (gameplay semantics) selected as immediate active lane with shots-first extraction strategy.
- `2026-02-13`: G1 discovery completed; migration ledger now has concrete `m-dev` source-to-rewrite target mapping for shots/hits/scoring/round lifecycle.
- `2026-02-13`: identified engine/game leak in `m-dev` renderer backends (`shot.glb` special-casing), marked as explicit anti-pattern for rewrite migration.
- `2026-02-13`: G2 implemented in rewrite server runtime/domain: active-shot tracking + deterministic expiry/remove broadcast path are now rewrite-owned.
- `2026-02-13`: G2 validation passed in both assigned build dirs (`bzbuild.py -c` + `test-server-net.sh <build-dir>` on jolt/physx).
- `2026-02-14`: D1 landed: rewrite client now has local drivable tank baseline (`tank_drive_controller` + in-game tank entity + follow camera).
- `2026-02-14`: D1 movement proof passed in both assigned build dirs via `src/game/tank_drive_controller_test`.
- `2026-02-14`: D1 hardening landed: reduced movement stutter via substep+visual smoothing, added FPS/chase camera modes (FPS default), and added startup-world collision blocking via engine-public geometry contract (`include/karma/geometry/mesh_loader.hpp`) with boundary-safe include usage in game paths.

## Open Questions
- Should D2 keep player movement state as transport-side events first, or immediately establish a server-domain movement system owning authoritative player pose?
- Should D2 map `ClientMsg_PlayerLocation` directly onto existing server session entities, or introduce a dedicated movement component/system seam first?
- Should owner-local remove semantics (`local id for owner`, `global id for others`) stay transport-only, or be hoisted into explicit game-domain removal policy before scoreboard migration?

## Handoff Checklist
- [x] Migration tracks and boundaries documented.
- [x] G1 ledger populated with concrete source->target mappings.
- [x] G2 implementation slice accepted.
- [x] D1 drivable-tank baseline slice accepted.
- [x] D1 hardening follow-up slice accepted.
- [x] Next implementation packet drafted (D2).
