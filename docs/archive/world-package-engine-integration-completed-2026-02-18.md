# World Package Engine Integration

## Project Snapshot
- Current owner: `overseer`
- Status: `archived/completed (W1-W5 complete: adapter compaction, header-surface minimization, regression hardening, and closeout finished)`
- Immediate next task: `none (archived)`.
- Validation gate: `./abuild.py -c -d <build-dir>` + `./scripts/test-server-net.sh <build-dir>` + `ctest --test-dir <build-dir> -R 'client_world_package_safety_integration_test|server_net_contract_test' --output-on-failure` + `./docs/scripts/lint-project-docs.sh`.

## Mission
Stabilize and simplify world-package integration by minimizing game-side adapter surface while preserving protocol semantics and engine ownership boundaries.

Primary objective:
- keep game protocol and gameplay policy in `src/game/*`,
- keep transport/content mechanics in engine `karma::content` and `karma::network::content`,
- reduce `src/game/client/net/world_package/*` to a clear, maintainable adapter layer.

## Foundation References
- `docs/foundation/policy/rewrite-invariants.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/architecture/core-engine-contracts.md`
- `docs/archive/content-mount-completed-2026-02-18.md`
- `docs/archive/engine-game-boundary-hygiene-retired-2026-02-18.md`

## Why This Is Separate
Content mount and boundary-hygiene extraction are already archived as completed tracks. This project is a focused follow-up to improve maintainability and handoff resilience for remaining world-package adapters without reopening completed engineization scope.

## Owned Paths
- `docs/archive/world-package-engine-integration-completed-2026-02-18.md`
- `docs/projects/ASSIGNMENTS.md`
- `src/game/client/net/world_package/*`
- `src/game/client/net/connection/internal.hpp` (interface-only coordination when required)
- `src/game/client/net/connection/inbound/*` (include/interface touch only when required)
- `src/game/client/net/connection/outbound.cpp` (include/interface touch only when required)
- `src/game/CMakeLists.txt`
- `src/game/tests/client_world_package_safety_integration_test.cpp`
- `include/karma/common/content/*` (only if a missing seam blocks adapter simplification)
- `src/engine/common/content/*` (only if a missing seam blocks adapter simplification)

## Interface Boundaries
- Inputs consumed:
  - server init/transfer metadata via `bz3::net::ServerMessage`,
  - engine content sync/apply/cache primitives via `karma::content::*`,
  - connection-side detail bridge contract from `src/game/client/net/connection/internal.hpp`.
- Outputs exposed:
  - stable game-side adapter functions consumed by client connection bootstrap/transfer paths,
  - no protocol schema or gameplay-policy changes by default.
- Coordinate before changing:
  - `src/game/net/protocol.hpp`
  - `src/game/net/protocol_codec/*`
  - `src/game/protos/messages.proto`
  - `docs/projects/gameplay-migration.md`

## Non-Goals
- Do not change wire protocol schema, protobuf fields, or message semantics.
- Do not change gameplay authority rules.
- Do not redesign transfer retry/resume/integrity policy in this track.
- Do not reopen archived content-mount behavior contracts.
- Do not move game-only protocol mapping into engine.
- Do not include server transport-event-source extraction (`src/game/server/net/transport_event_source/*`) in this project.
- Do not include community HTTP listing extraction (`src/game/client/community_server_list.cpp`) in this project.
- Do not include client credential/auth payload extraction (`src/game/client/runtime/*`) in this project.

## Execution Plan
1. W0 inventory and scope lock:
- document current `world_package/*` responsibilities and callsites,
- verify adapter-only intent and baseline validation.

2. W1 adapter compaction:
- collapse wrapper-heavy world-package helper implementations into a reduced file layout (target: shared bridge unit + apply adapter),
- keep public/detail signatures stable for connection callsites.

3. W2 header surface minimization:
- reduce `world_package/internal.hpp` to active adapter contract only,
- keep non-exported helper details local to `.cpp` units.

4. W3 seam promotion (only if needed):
- if compaction exposes missing engine seams, extract narrowly scoped helpers into `karma::content::*` with no protocol coupling.

5. W4 regression hardening:
- strengthen targeted tests and traces for cache-hit/no-payload, full transfer, delta transfer, and rollback safety paths.

6. W5 closeout:
- snapshot final adapter boundary and archive this track when stable.

## Validation
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir>
./scripts/test-server-net.sh <build-dir>
ctest --test-dir <build-dir> -R 'client_world_package_safety_integration_test|server_net_contract_test' --output-on-failure
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `net.client`
- `net.server`
- `engine.server`
- `config`

## Build/Run Commands
```bash
./abuild.py -c -d <build-dir>
./scripts/test-server-net.sh <build-dir>
```

## First Session Checklist
1. Read `AGENTS.md`, then `docs/foundation/policy/execution-policy.md`, then this file.
2. Confirm overlap boundaries with `docs/projects/gameplay-migration.md`.
3. Execute only W0/W1 bounded adapter-compaction slice.
4. Run required validation with assigned build dir.
5. Update this file and `docs/projects/ASSIGNMENTS.md` in the same handoff.

## Current Status
- `2026-02-18`: project created to preserve a durable execution plan for world-package adapter simplification and engine integration follow-up.
- `2026-02-18`: boundaries and phased plan documented; ready for W0 inventory + W1 compaction start.
- `2026-02-18`: W1 completed: consolidated game-side world-package helper implementations into `src/game/client/net/world_package/bridge.cpp`, removed `primitives.cpp`/`manifest.cpp`/`cache.cpp`/`delta.cpp`, and updated `src/game/CMakeLists.txt` source wiring for `bz3` and `client_world_package_safety_integration_test`.
- `2026-02-18`: W1 validation passed on `build-a3`:
  - `./abuild.py -c -d build-a3`
  - `./scripts/test-server-net.sh build-a3`
  - `ctest --test-dir build-a3 -R 'client_world_package_safety_integration_test|server_net_contract_test' --output-on-failure`
  - `./docs/scripts/lint-project-docs.sh`
- `2026-02-18`: W2 completed: trimmed `src/game/client/net/world_package/internal.hpp` to active shared declarations/constants only, moved non-apply `detail::*` bindings from `apply.cpp` into `bridge.cpp`, and removed dead `ExtractWorldArchiveAtomically` adapter surface.
- `2026-02-18`: W2 validation passed on `build-a3`:
  - `./abuild.py -c -d build-a3`
  - `./scripts/test-server-net.sh build-a3`
  - `ctest --test-dir build-a3 -R 'client_world_package_safety_integration_test|server_net_contract_test' --output-on-failure`
  - `./docs/scripts/lint-project-docs.sh`
- `2026-02-18`: W4 completed: added explicit regression coverage for cache-hit/no-payload init and successful delta-transfer apply in `src/game/tests/client_world_package_safety_integration_test.cpp`; rollback-safety coverage remained in place.
- `2026-02-18`: W4 validation passed on `build-a3`:
  - `./abuild.py -c -d build-a3`
  - `./scripts/test-server-net.sh build-a3`
  - `ctest --test-dir build-a3 -R 'client_world_package_safety_integration_test|server_net_contract_test' --output-on-failure`
  - `./docs/scripts/lint-project-docs.sh`
- `2026-02-18`: W5 completed: project file archived to `docs/archive/world-package-engine-integration-completed-2026-02-18.md` and active assignment row removed from `docs/projects/ASSIGNMENTS.md`.

## Open Questions
- None blocking closeout for this track.

## Handoff Checklist
- [x] Scoped files updated with behavior preserved.
- [x] Required validation commands executed and summarized.
- [x] Project snapshot/plan/checklist updated.
- [x] `docs/projects/ASSIGNMENTS.md` active row removed at archive closeout.
- [x] Risks/open questions captured.
