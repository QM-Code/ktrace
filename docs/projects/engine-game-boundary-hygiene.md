# Engine-Game Boundary Hygiene (Content Sync Foundation)

## Project Snapshot
- Current owner: `specialist-engine-boundary-e2`
- Status: `in progress (E0.6/E1/E2/E3/E4 landed on build-a1; transfer sender/receiver extraction moved to engine network content modules with thin game wrappers)`
- Immediate next task: execute E5 by adding an optional engine default content-sync facade and hardening BZ3 adapter adoption paths.
- Validation gate: `./abuild.py -c -d <build-dir>` + `./scripts/test-server-net.sh <build-dir>` + `./scripts/test-engine-backends.sh <build-dir>` + `ctest --test-dir <build-dir> -R client_world_package_safety_integration_test --output-on-failure` + `./docs/scripts/lint-project-docs.sh`.

## Mission
Move game-agnostic file sync logic out of `src/game/` and into engine-owned contracts so game teams get a default, low-effort server-to-client content sync path.

Default-first target:
- 95% path: game opts into engine content sync and gets transfer/caching/delta/apply behavior with minimal glue.
- 5% path: game can override policy/metadata/protocol mapping without rewriting baseline mechanics.

## Foundation References
- `docs/foundation/policy/rewrite-invariants.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/architecture/core-engine-contracts.md`
- `docs/foundation/policy/decisions-log.md`
- `docs/projects/content-mount.md`
- `docs/archive/client-connection-split-retired-2026-02-17.md`

## Why This Is Separate
This extraction is cross-cutting and touches engine common/network plus game client/server adapters. Keeping it in a dedicated project prevents accidental protocol/gameplay drift while moving generic mechanics into engine-owned defaults.

## Owned Paths
- `docs/projects/engine-game-boundary-hygiene.md`
- `docs/projects/ASSIGNMENTS.md`
- `include/karma/common/content/*` (engine-owned content contracts)
- `src/engine/common/content/*` (engine-owned content implementations)
- `include/karma/network/content/*` (engine-owned transfer contracts)
- `src/engine/network/content/*` (engine-owned transfer implementations)
- legacy compatibility wrappers during migration:
- `include/karma/common/world_archive.hpp`
- `include/karma/common/world_content.hpp`
- `src/engine/common/world_archive.cpp`
- `src/engine/common/world_content.cpp`
- Coordinated adapter touchpoints:
- `src/game/client/net/connection/*`
- `src/game/client/net/world_package/*`
- `src/game/server/net/transport_event_source.cpp`
- `src/game/server/net/transport_event_source/*`
- `src/game/server/domain/world_session.*`
- `src/game/tests/client_world_package_safety_integration_test.cpp`

## Interface Boundaries
- Inputs consumed:
  - server world/content directory trees and archive bytes,
  - client cache identity/manifest hints,
  - transport send/receive callbacks from game protocol adapters.
- Outputs exposed:
  - engine-level content sync primitives (manifest/hash/cache/delta/atomic promote),
  - engine-level transfer state machines (sender retry/resume and receiver assembly/integrity),
  - optional engine default facade for package apply + mount activation.
- Coordinate before changing:
  - `src/game/net/protocol.hpp`
  - `src/game/net/protocol_codec.*`
  - `src/game/protos/messages.proto`
  - `src/engine/common/data_path_resolver.*`
  - `src/engine/common/config_store.*`

## Non-Goals
- Do not move BZ3 protocol schema or message enums into engine in this project.
- Do not move gameplay semantics (audio/game events/session rules) into engine.
- Do not alter gameplay-visible world/package behavior in extraction phases E1-E4.

## Directory + Naming Lock (Decision)
1. Use directory boundaries, not flat prefix piles:
- engine content-sync code goes under `include/karma/common/content/*` and `src/engine/common/content/*`.
- engine transfer state machines go under `include/karma/network/content/*` and `src/engine/network/content/*`.

2. Avoid adding new `content_*` files directly in flat `src/engine/common/` or `include/karma/common/`.
- Flat roots remain for broad engine-wide utilities.
- Content-sync code must stay grouped in `content/` subdirectories.

3. Prefer short file names inside the directory domain:
- `primitives`, `manifest`, `cache_store`, `archive`, `delta_builder`, `package_apply`, `types`, `catalog`.
- Do not re-introduce long prefix naming in a single flat directory.

4. Migration compatibility policy:
- legacy `world_*` headers/sources remain as thin wrappers during migration.
- wrappers forward to new `content/*` implementations and are removed only after all call sites migrate.

## Extraction Inventory (What Is Actually Generic)
0. Existing engine `world_*` modules -> content-domain placement (high confidence):
- Source: `src/engine/common/world_archive.cpp`, `src/engine/common/world_content.cpp`, `include/karma/common/world_archive.hpp`, `include/karma/common/world_content.hpp`.
- Target decomposition:
  - archive build/extract behavior -> `include/karma/common/content/archive.hpp`, `src/engine/common/content/archive.cpp`.
  - content catalog/layer merge behavior -> `include/karma/common/content/catalog.hpp`, `src/engine/common/content/catalog.cpp`.
  - shared bytes/types aliases currently mixed into world header -> `include/karma/common/content/types.hpp`.
- Migration rule:
  - keep `world_archive.*` / `world_content.*` as compatibility forwarding wrappers while game code transitions.
  - do not leave duplicate independent implementations.

1. Hashing and path safety primitives (high confidence):
- Source: `src/game/client/net/world_package/primitives.cpp`, `src/game/server/domain/world_session.cpp`, `src/game/tests/client_world_package_safety_integration_test.cpp`.
- Generic behaviors:
  - FNV-1a byte/string hashing, hex formatting, chunk-chain hashing.
  - relative-path normalization/safety checks.
  - chunk bounds and buffered chunk match checks.
- Proposed engine target:
  - `include/karma/common/content/primitives.hpp`
  - `src/engine/common/content/primitives.cpp`

2. Manifest summary, hash, and diff planning (high confidence):
- Source: `src/game/client/net/world_package/manifest.cpp`, `src/game/server/net/transport_event_source/common.cpp`, `src/game/server/net/transport_event_source/join.cpp`, `src/game/server/domain/world_session.cpp`.
- Generic behaviors:
  - compute directory manifest summary.
  - compute deterministic manifest hash.
  - compute cached-vs-incoming diff (added/modified/removed/reused bytes).
- Proposed engine target:
  - `include/karma/common/content/manifest.hpp`
  - `src/engine/common/content/manifest.cpp`

3. Package cache identity/store and pruning (high confidence):
- Source: `src/game/client/net/world_package/cache.cpp`.
- Generic behaviors:
  - cache identity read/write/validation.
  - manifest persistence for cached package.
  - retention pruning over world/revision/package directories.
- Proposed engine target:
  - `include/karma/common/content/cache_store.hpp`
  - `src/engine/common/content/cache_store.cpp`

4. Atomic staging/promote and delta apply file operations (high confidence):
- Source: `src/game/client/net/world_package/primitives.cpp`, `src/game/client/net/world_package/delta.cpp`, `src/game/client/net/world_package/apply.cpp`.
- Generic behaviors:
  - staging directory creation/cleanup.
  - atomic activate with rollback.
  - apply delta archive over cached base and remove-path handling.
- Proposed engine target:
  - `include/karma/common/content/package_apply.hpp`
  - `src/engine/common/content/package_apply.cpp`

5. Server delta archive assembly (high confidence):
- Source: `src/game/server/net/transport_event_source/common.cpp` (`BuildWorldDeltaArchive`, `kDeltaRemovedPathsFile`, `kDeltaMetaFile` handling).
- Generic behaviors:
  - build delta staging tree from manifest diff.
  - include remove-path metadata.
  - archive staging payload bytes.
- Proposed engine target:
  - `include/karma/common/content/delta_builder.hpp`
  - `src/engine/common/content/delta_builder.cpp`

6. Transfer sender state machine with retry/resume (medium confidence):
- Source: `src/game/server/net/transport_event_source/send.cpp` (`sendWorldPackageChunked` and begin/chunk/end sequencing).
- Generic behaviors:
  - chunk sizing, retry budget, resume offsets/chunk-index continuity.
  - begin/chunk/end transfer lifecycle.
- Proposed engine target:
  - `include/karma/network/content/transfer_sender.hpp`
  - `src/engine/network/content/transfer_sender.cpp`

7. Transfer receiver state machine with integrity checks (medium confidence):
- Source: `src/game/client/net/connection/inbound/world_transfer.cpp`.
- Generic behaviors:
  - begin/chunk/end validation.
  - restart compatibility checks.
  - payload/chunk-chain hash checks before apply.
- Proposed engine target:
  - `include/karma/network/content/transfer_receiver.hpp`
  - `src/engine/network/content/transfer_receiver.cpp`

## What Stays In `src/game/` (By Design)
1. Protocol mapping and message contracts:
- `bz3::net::ServerMessageType::*` dispatch.
- encode/decode calls in `src/game/net/protocol_codec.*`.
- protobuf schema ownership in `src/game/protos/messages.proto`.

2. Game policy decisions:
- when gameplay/session flow triggers sync.
- how world identity/revision defaults are chosen for BZ3 gameplay.
- game-side audio/gameplay callbacks after join/bootstrap.

3. Thin adapters:
- client adapter maps protocol payloads -> engine transfer receiver inputs.
- server adapter maps engine transfer sender outputs -> protocol begin/chunk/end messages.

## Proposed Engine API Shape (95% Path)
1. `karma::content` package layer (implemented under `karma/common/content/*`):
- package identity, manifest, cache store, diff planning, delta build/apply, atomic activate.

2. `karma::network::content` transfer layer (implemented under `karma/network/content/*`):
- protocol-agnostic transfer sender and receiver state machines.
- caller supplies send/receive adapters and transfer metadata.

3. Optional engine facade:
- one orchestration entrypoint for “sync server files to client cache + mount overlay”.
- pluggable policy hooks for games needing custom transfer/apply strategy.

## Execution Plan
1. E0 contract/skeleton:
- Add engine headers/source stubs + data structs + no-op tests using directory-first content layout (`common/content`, `network/content`).
- Keep existing game behavior unchanged.

2. E0.5 game-server split prerequisite:
- Decompose `src/game/server/net/transport_event_source.cpp` into `src/game/server/net/transport_event_source/{common,core,receive,join,events,send}.cpp` with a thin factory entrypoint at `src/game/server/net/transport_event_source.cpp`.
- Keep behavior unchanged; this is a refactor-only step to reduce context hotspots before engine extraction.

3. E0.6 world-module normalization prerequisite:
- Relocate engine `world_archive/world_content` functionality into `common/content/{archive,catalog,types}`.
- Keep compatibility wrappers at legacy `world_*` paths to avoid wide churn during transition.
- Keep behavior and API semantics unchanged in this phase.

4. E1 primitives extraction:
- Move hash/path/chunk checks and eliminate duplicate implementations in:
  - `src/game/client/net/world_package/primitives.cpp`
  - `src/game/server/domain/world_session.cpp`
  - `src/game/tests/client_world_package_safety_integration_test.cpp`

5. E2 manifest/cache extraction:
- Move manifest compute/hash/diff and cache identity/persist/prune to engine.
- Keep game wrappers thin and behavior-identical.

6. E3 delta and package-apply extraction:
- Move delta archive build/apply and staging/rollback utilities to engine.
- Preserve current world-package safety gates.

7. E4 transfer state-machine extraction:
- Move sender/receiver transfer logic to engine network.
- Keep game protocol adapters as wrappers.

8. E5 default facade and adoption hardening:
- Add optional engine default content sync facade for new games.
- Update BZ3 adapters to consume facade.

## Validation
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir>
./scripts/test-server-net.sh <build-dir>
./scripts/test-engine-backends.sh <build-dir>
ctest --test-dir <build-dir> -R client_world_package_safety_integration_test --output-on-failure
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `net.server`
- `net.client`
- `content.mount`
- `config`

## Risks
1. Over-extraction risk:
- pulling protocol-specific assumptions into engine can hard-couple engine APIs to BZ3 wire semantics.

2. Regression risk:
- package apply and delta edge cases are safety-critical; extraction must preserve strict validation/rollback semantics.

3. API creep risk:
- exposing too many knobs in engine facade can make default path harder, not easier.

## Current Status
- `2026-02-17`: project created as explicit follow-up to `client-connection-split`.
- `2026-02-17`: extraction inventory completed from current game client/server code paths.
- `2026-02-17`: phased plan drafted for zero-regression engineization.
- `2026-02-17`: server-side prerequisite split landed: `transport_event_source.cpp` decomposed into `transport_event_source/{common,core,receive,join,events,send}.cpp` with factory shim retained.
- `2026-02-17`: validation passed after split (`./scripts/test-server-net.sh build-a6`, `./docs/scripts/lint-project-docs.sh`).
- `2026-02-17`: directory-first placement locked for this project (`common/content`, `network/content`); legacy engine `world_*` modules explicitly scheduled for compatibility-wrapper migration into `content/{archive,catalog,types}`.
- `2026-02-17`: E0.6 landed: moved engine `world_archive/world_content` implementations to `include/karma/common/content/{types,archive,catalog}.hpp` + `src/engine/common/content/{archive,catalog}.cpp`; legacy `world_*` headers/sources now forward to `karma::content` implementations.
- `2026-02-17`: E1 landed: extracted shared hash/path/chunk primitives to `include/karma/common/content/primitives.hpp` + `src/engine/common/content/primitives.cpp`; replaced duplicate logic in `src/game/client/net/world_package/primitives.cpp`, `src/game/server/domain/world_session.cpp`, and `src/game/tests/client_world_package_safety_integration_test.cpp`.
- `2026-02-17`: validation passed for E0.6/E1 on `build-a1`:
  - `./abuild.py -c -d build-a1`
  - `./scripts/test-server-net.sh build-a1`
  - `./scripts/test-engine-backends.sh build-a1`
  - `ctest --test-dir build-a1 -R client_world_package_safety_integration_test --output-on-failure`
  - `./docs/scripts/lint-project-docs.sh`
- `2026-02-17`: E2 landed: extracted manifest/hash/diff + cache identity/store/prune logic to `include/karma/common/content/{manifest,cache_store}.hpp` and `src/engine/common/content/{manifest,cache_store}.cpp`; migrated `src/game/client/net/world_package/{manifest,cache}.cpp`, `src/game/server/domain/world_session.cpp`, `src/game/server/net/transport_event_source/common.cpp`, and `src/game/tests/client_world_package_safety_integration_test.cpp` to thin wrappers/adapter usage.
- `2026-02-17`: E2 validation on `build-a1`:
  - `./abuild.py -c -d build-a1` *(fails in unrelated renderer track at `renderer_shadow_sandbox` link: missing `UpdateBgfxDirectionalShadowCache` / `UpdateBgfxPointShadowCache`)*
  - `./scripts/test-server-net.sh build-a1` *(pass)*
  - `./scripts/test-engine-backends.sh build-a1` *(pass)*
  - `ctest --test-dir build-a1 -R client_world_package_safety_integration_test --output-on-failure` *(pass)*
  - `./docs/scripts/lint-project-docs.sh` *(pass)*
- `2026-02-17`: E3 landed: extracted server delta archive build and client package apply/staging/promote operations to `include/karma/common/content/{delta_builder,package_apply}.hpp` and `src/engine/common/content/{delta_builder,package_apply}.cpp`; migrated `src/game/client/net/world_package/{primitives,apply,delta}.cpp` and `src/game/server/net/transport_event_source/common.cpp` to thin wrapper/adapters over engine content APIs.
- `2026-02-17`: E3 validation on `build-a1`:
  - `./abuild.py -c -d build-a1` *(pass)*
  - `./scripts/test-server-net.sh build-a1` *(pass)*
  - `./scripts/test-engine-backends.sh build-a1` *(pass)*
  - `ctest --test-dir build-a1 -R client_world_package_safety_integration_test --output-on-failure` *(pass)*
  - `./docs/scripts/lint-project-docs.sh` *(pass)*
- `2026-02-17`: E4 landed: extracted transfer sender/receiver state-machine logic to `include/karma/network/content/{transfer_sender,transfer_receiver}.hpp` and `src/engine/network/content/{transfer_sender,transfer_receiver}.cpp`; migrated `src/game/server/net/transport_event_source/send.cpp` and `src/game/client/net/connection/inbound/world_transfer.cpp` to thin protocol adapters over engine transfer APIs.
- `2026-02-17`: E4 validation on `build-a1`:
  - `./abuild.py -c -d build-a1` *(pass)*
  - `./scripts/test-server-net.sh build-a1` *(pass)*
  - `./scripts/test-engine-backends.sh build-a1` *(pass)*
  - `ctest --test-dir build-a1 -R client_world_package_safety_integration_test --output-on-failure` *(pass)*
  - `./docs/scripts/lint-project-docs.sh` *(pass)*

## Open Questions
- Should `world::*` namespace be retained as compatibility shims only during migration, with target APIs moving to `karma::content`?
- Should transfer integrity remain FNV-based for parity, or should engine default to stronger hash options while keeping compatibility mode?
- Should engine default facade own mount/runtime-layer updates directly, or expose those as callbacks for game-owned final activation?

## Handoff Checklist
- [x] Candidate inventory documented with concrete source paths.
- [x] Engine target seams proposed with phased rollout.
- [x] Game-vs-engine boundary clarified for protocol and gameplay policy.
- [x] E0.5 server transport split prerequisite landed and validated.
- [x] E0.6 world-module normalization implemented with compatibility wrappers.
- [x] E1 primitive extraction implemented and validated.
- [x] E2 manifest/cache extraction implemented with engine-owned modules and thin game wrappers.
- [x] E3 delta/package-apply extraction implemented with engine-owned modules and thin game wrappers.
- [x] E4 transfer sender/receiver extraction implemented with engine-owned modules and thin game wrappers.
