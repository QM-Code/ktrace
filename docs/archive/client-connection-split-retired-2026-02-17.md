# Client Connection Split (Retired)

## Project Snapshot
- Current owner: `retired/archive`
- Status: `retired/completed (split delivered; follow-up execution moved to engine-game-boundary-hygiene)`
- Immediate next task: do not start new work here; continue active boundary extraction in `docs/projects/engine-game-boundary-hygiene.md`.
- Validation gate: historical record retained; active validation policy is tracked in `docs/projects/engine-game-boundary-hygiene.md`.

## Mission
Decompose the legacy client connection monolith into smaller, stable translation units with explicit boundaries between:
- connection lifecycle + message ingress/egress,
- world package apply/cache/manifest/delta internals,
- future engine-portable primitives vs game-specific policy.

## Foundation References
- `docs/foundation/policy/rewrite-invariants.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/architecture/core-engine-contracts.md`
- `docs/projects/content-mount.md`

## Why This Is Separate
`client_connection` was previously a high-churn hotspot that mixed transport lifecycle, protocol ingress dispatch, world package caching/apply behavior, and gameplay event callbacks in one file. Splitting it reduces merge pressure, review scope, and future engineization risk.

## Owned Paths
- `docs/archive/client-connection-split-retired-2026-02-17.md`
- `docs/projects/ASSIGNMENTS.md`
- `src/game/client/net/connection.hpp`
- `src/game/client/net/connection/*`
- `src/game/client/net/world_package/*`
- `src/game/CMakeLists.txt`
- `src/game/game.cpp`
- `src/game/tests/client_world_package_safety_integration_test.cpp`

## Interface Boundaries
- Inputs consumed:
  - `karma::network::ClientTransport` events.
  - `bz3::net::DecodeServerMessage` protocol payloads.
  - engine runtime config/data-path APIs used by world package apply.
- Outputs exposed:
  - `ClientConnection` public API in `src/game/client/net/connection.hpp`.
  - safe world package apply/cache behavior through `detail::*` bridge functions.
  - unchanged protocol wire behavior for client/server net tests.
- Coordinate before changing:
  - `src/game/net/protocol.hpp`
  - `src/game/net/protocol_codec.*`
  - `src/game/protos/messages.proto`
  - `docs/projects/content-mount.md`

## Non-Goals
- No protocol schema redesign.
- No behavior changes in join/init/world-transfer semantics beyond equivalent refactor.
- No broad engine move in this slice; extraction candidates are tracked but deferred.

## Completed Work (2026-02-17)
1. Replaced legacy file pair:
- removed `src/game/client/net/client_connection.hpp`
- removed `src/game/client/net/client_connection.cpp`
2. Introduced directory-based connection layout:
- `src/game/client/net/connection.hpp`
- `src/game/client/net/connection/core.cpp`
- `src/game/client/net/connection/outbound.cpp`
- `src/game/client/net/connection/internal.hpp`
3. Introduced directory-based world package layout:
- `src/game/client/net/world_package/primitives.cpp`
- `src/game/client/net/world_package/manifest.cpp`
- `src/game/client/net/world_package/cache.cpp`
- `src/game/client/net/world_package/delta.cpp`
- `src/game/client/net/world_package/apply.cpp`
- `src/game/client/net/world_package/internal.hpp`
4. Landed phase-2 inbound decomposition:
- added `src/game/client/net/connection/inbound/bootstrap.cpp`
- added `src/game/client/net/connection/inbound/dispatch.cpp`
- added `src/game/client/net/connection/inbound/world_transfer.cpp`
- added `src/game/client/net/connection/inbound/session_events.cpp`
- added `src/game/client/net/connection/inbound/transport_lifecycle.cpp`
- moved thin poll/decode/dispatch facade into `src/game/client/net/connection/inbound/dispatch.cpp`.
5. Updated include/build integration:
- `src/game/CMakeLists.txt` now compiles `connection/*`, `connection/inbound/*`, and `world_package/*` for `bz3` and `client_world_package_safety_integration_test`.
- `src/game/game.cpp` include migrated to `client/net/connection.hpp`.
- `src/game/tests/client_world_package_safety_integration_test.cpp` include migrated to `client/net/connection.hpp`.
6. Validation run:
- `./abuild.py -c -d build-a6` passed.
- `ctest --test-dir build-a6 -R client_world_package_safety_integration_test --output-on-failure` passed.

## Current File Coverage
Connection:
- `src/game/client/net/connection/core.cpp`: constructor/start/shutdown/transport lifecycle helpers.
- `src/game/client/net/connection/outbound.cpp`: join/leave/player-action outbound encodes + reliable send.
- `src/game/client/net/connection/inbound/dispatch.cpp`: poll loop + payload decode + top-level server-message dispatch.
- `src/game/client/net/connection/inbound/bootstrap.cpp`: `JoinResponse` + `Init` bootstrap and world-package startup checks.
- `src/game/client/net/connection/inbound/world_transfer.cpp`: `WorldTransferBegin/Chunk/End` state machine and chunk-integrity/apply path.
- `src/game/client/net/connection/inbound/session_events.cpp`: session snapshot and gameplay event callbacks (`spawn/death/shot`).
- `src/game/client/net/connection/inbound/transport_lifecycle.cpp`: reconnect/disconnect lifecycle and join replay.

World package:
- `src/game/client/net/world_package/primitives.cpp`: path/hash/chunk bounds/path normalization primitives.
- `src/game/client/net/world_package/manifest.cpp`: manifest hash/read/persist/verify/diff logging.
- `src/game/client/net/world_package/cache.cpp`: cache identity read/write/validation and pruning.
- `src/game/client/net/world_package/delta.cpp`: delta-apply-over-base flow.
- `src/game/client/net/world_package/apply.cpp`: package apply orchestration and detail bridge exports.

## Generic vs Game-Specific Extraction Candidates
Generic candidates for `src/engine/` (deferred, separate project gate):
- chunk-stream integrity/resume utilities (`IsChunkInTransferBounds`, chunk-chain hashing flow),
- filesystem atomic-staging/promote helpers used for package apply/cache hygiene,
- generic content-package cache pruning primitives.

Game-specific (should remain in `src/game/`):
- `bz3::net::ServerMessageType` dispatch policy and protocol contract handling,
- world identity/manifest semantics bound to bz3 gameplay content model,
- audio event callback policy tied to gameplay events.

## Validation
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir>
ctest --test-dir <build-dir> -R client_world_package_safety_integration_test --output-on-failure
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `net.client`
- `net.server`
- `config`

## Current Status
- `2026-02-17`: Legacy `client_connection.*` removed; `connection/` + `world_package/` directory split landed.
- `2026-02-17`: Build integration and includes updated to new paths.
- `2026-02-17`: Inbound decomposition landed; handlers moved into `src/game/client/net/connection/inbound/*.cpp` with thin dispatch facade at `src/game/client/net/connection/inbound/dispatch.cpp`.
- `2026-02-17`: Cleanup rename landed: `src/game/client/net/connection/inbound.cpp` -> `src/game/client/net/connection/inbound/dispatch.cpp`.
- `2026-02-17`: `client_world_package_safety_integration_test` remained green after inbound split in `build-a6`.
- `2026-02-17`: engineization follow-up project drafted at `docs/projects/engine-game-boundary-hygiene.md`.
- `2026-02-17`: project retired from active board and archived at `docs/archive/client-connection-split-retired-2026-02-17.md`.

## Open Questions
- None in this project; extracted boundary work is now tracked in `docs/projects/engine-game-boundary-hygiene.md`.

## Handoff Checklist
- [x] Legacy `client_connection.*` replaced with directory-based split.
- [x] `world_package` decomposition completed under dedicated directory.
- [x] Inbound phase-2 decomposition implemented.
- [x] Build/test wiring updated for new source layout.
- [x] Cross-project docs updated for renamed paths.
- [x] Engine extraction candidates triaged into dedicated follow-up project (`engine-game-boundary-hygiene.md`).
