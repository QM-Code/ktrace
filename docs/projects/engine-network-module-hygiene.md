# Engine Network Module Hygiene

## Project Snapshot
- Current owner: `unassigned`
- Status: `queued (taxonomy and migration plan drafted; execution not started)`
- Immediate next task: execute Phase N0 by locking module taxonomy + target directories and landing compatibility-wrapper scaffolding for one bounded transport header/source pair.
- Validation gate: `./abuild.py -c -d <build-dir>` + `./scripts/test-server-net.sh <build-dir>` + `ctest --test-dir <build-dir> -R 'client_transport_contract_test|server_transport_contract_test' --output-on-failure` + `./docs/scripts/lint-project-docs.sh`.

## Mission
Decompose and normalize `src/engine/network/` into clear submodules with directory-first ownership, eliminating flat prefix piles (`client_*`, `server_*`) and mixed concerns in one directory.

Primary objective:
- make module ownership obvious from paths, not filename prefixes,
- keep behavior/protocol semantics unchanged while reorganizing,
- reduce future merge pressure and make extraction paths clearer for follow-on projects.

## Foundation References
- `docs/foundation/policy/rewrite-invariants.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/architecture/core-engine-contracts.md`
- `docs/archive/engine-game-boundary-hygiene-retired-2026-02-18.md`

## Why This Is Separate
This is structural hygiene for engine network ownership and naming, not gameplay/netcode behavior work. Keeping it separate prevents accidental coupling with active protocol/content slices while allowing low-risk, behavior-neutral refactors.

## Owned Paths
- `docs/projects/engine-network-module-hygiene.md`
- `docs/projects/ASSIGNMENTS.md`
- `include/karma/network/*`
- `src/engine/network/*`
- `include/karma/common/content/primitives.hpp` (transfer-helper extraction only)
- `src/engine/common/content/primitives.cpp` (transfer-helper extraction only)
- `include/karma/common/curl_global.hpp` (compat wrapper during migration)
- `src/engine/common/curl_global.cpp` (compat wrapper during migration)
- `src/engine/CMakeLists.txt`
- `src/game/server/runtime.cpp` (include-path adjustments only when required by moved headers)
- `src/game/client/net/connection/*` (include-path adjustments only when required by moved headers)
- `src/game/server/net/transport_event_source/*` (include-path adjustments only when required by moved headers)
- `src/engine/network/tests/*` (path/target updates only, no contract semantic changes)

## Interface Boundaries
- Inputs consumed:
  - config reads from `karma::config::*`,
  - game runtime callbacks/hooks passed into engine network helpers,
  - ENet transport backend adapter internals.
- Outputs exposed:
  - stable engine transport/session/auth/heartbeat contracts used by game runtime.
- Coordinate before changing:
  - `src/game/net/protocol.hpp`
  - `src/game/net/protocol_codec.*`
  - `src/game/protos/messages.proto`
  - `docs/projects/gameplay-netcode.md`
  - `docs/archive/engine-game-boundary-hygiene-retired-2026-02-18.md`

## Non-Goals
- Do not change wire protocol schema or message semantics.
- Do not change gameplay session authority decisions.
- Do not redesign reconnect/auth/heartbeat behavior in this project.
- Do not fold `network/content/*` extraction work from `engine-game-boundary-hygiene` into this track.
- Do not move transport-agnostic content mechanics (`archive`, `manifest`, `cache_store`, `delta_builder`, `package_apply`, `sync_facade`) out of `common/content/*`.
- Do not move `data_path_resolver` host/port cache-path helpers into `network/*` in this project.

## Content Boundary Note
- `common/content/*` remains the home for transport-agnostic content mechanics:
  - manifest/hash/diff,
  - cache identity/store/prune,
  - archive build/extract,
  - delta build/apply,
  - file/package staging/promote helpers.
- `network/content/*` is reserved for transport-coupled state machines only:
  - transfer sender/receiver (chunking, retry/resume, stream-integrity gating).
- This project may reorganize network modules for clarity, but it must not pull generic content mechanics from `common/content/*` into `network/*`.

## Boundary Recheck (2026-02-18)
- Revalidated against current tree: `common/content/*` modules remain mostly transport-agnostic and should stay in `common/content/*`.
- No whole-file migrations from `common/content/*` to `network/*` are required by default.
- Additional goals adopted for this project:
  - extract transfer-only helper subset from `common/content/primitives` into:
    - `include/karma/network/content/transfer_integrity.hpp`
    - `src/engine/network/content/transfer_integrity.cpp`
  - move network-scoped curl initialization helper into:
    - `include/karma/network/http/curl_global.hpp`
    - `src/engine/network/http/curl_global.cpp`
- Explicit stay-put decision:
  - keep `common/content/{archive,manifest,cache_store,delta_builder,package_apply,sync_facade}` in `common/content/*`.
  - leave `EnsureUserWorldDirectoryForServer`/`EnsureUserWorldsDirectory` out of this project's network moves (evaluate separately under content/cache path ownership).

## Module Taxonomy Lock (Decision)
1. Transport contracts and backend adapters:
- `include/karma/network/transport/*`
- `src/engine/network/transport/*`

2. Network config mapping/policy helpers:
- `include/karma/network/config/*`
- `src/engine/network/config/*`

3. Server admission/session orchestration helpers:
- `include/karma/network/server/auth/*`
- `src/engine/network/server/auth/*`
- `include/karma/network/server/session/*`
- `src/engine/network/server/session/*`

4. Community registration heartbeat flow:
- `include/karma/network/community/*`
- `src/engine/network/community/*`

5. Test support utilities:
- `src/engine/network/tests/support/*`

6. Contract test executables:
- `src/engine/network/tests/contracts/*`

7. HTTP network utilities:
- `include/karma/network/http/*`
- `src/engine/network/http/*`

Compatibility rule:
- During migration, keep legacy flat headers as forwarding wrappers until all callsites move.
- Avoid duplicate independent implementations during wrapper period.

## Current -> Target Mapping
| Current Path | Target Path | Classification |
|---|---|---|
| `include/karma/network/client_transport.hpp` | `include/karma/network/transport/client.hpp` | transport |
| `src/engine/network/client_transport.cpp` | `src/engine/network/transport/client.cpp` | transport |
| `include/karma/network/server_transport.hpp` | `include/karma/network/transport/server.hpp` | transport |
| `src/engine/network/server_transport.cpp` | `src/engine/network/transport/server.cpp` | transport |
| `src/engine/network/transport_pump_normalizer.hpp` | `src/engine/network/transport/pump_normalizer.hpp` | transport-internal |
| `include/karma/network/transport_config_mapping.hpp` | `include/karma/network/config/transport_mapping.hpp` | config/policy |
| `src/engine/network/transport_config_mapping.cpp` | `src/engine/network/config/transport_mapping.cpp` | config/policy |
| `include/karma/network/client_reconnect_policy.hpp` | `include/karma/network/config/reconnect_policy.hpp` | config/policy |
| `src/engine/network/client_reconnect_policy.cpp` | `src/engine/network/config/reconnect_policy.cpp` | config/policy |
| `include/karma/network/server_preauth.hpp` | `include/karma/network/server/auth/preauth.hpp` | server auth |
| `src/engine/network/server_preauth.cpp` | `src/engine/network/server/auth/preauth.cpp` | server auth |
| `include/karma/network/server_session_hooks.hpp` | `include/karma/network/server/session/hooks.hpp` | server session |
| `src/engine/network/server_session_hooks.cpp` | `src/engine/network/server/session/hooks.cpp` | server session |
| `include/karma/network/server_join_runtime.hpp` | `include/karma/network/server/session/join_runtime.hpp` | server session |
| `src/engine/network/server_join_runtime.cpp` | `src/engine/network/server/session/join_runtime.cpp` | server session |
| `include/karma/network/server_session_runtime.hpp` | `include/karma/network/server/session/leave_runtime.hpp` | server session |
| `src/engine/network/server_session_runtime.cpp` | `src/engine/network/server/session/leave_runtime.cpp` | server session |
| `include/karma/network/community_heartbeat.hpp` | `include/karma/network/community/heartbeat.hpp` | community |
| `src/engine/network/community_heartbeat.cpp` | `src/engine/network/community/heartbeat.cpp` | community |
| `include/karma/network/heartbeat_client.hpp` | `include/karma/network/community/heartbeat_client.hpp` | community |
| `src/engine/network/heartbeat_client.cpp` | `src/engine/network/community/heartbeat_client.cpp` | community |
| `include/karma/network/content/transfer_sender.hpp` | `include/karma/network/content/transfer_sender.hpp` | content transfer (already grouped; keep in place) |
| `src/engine/network/content/transfer_sender.cpp` | `src/engine/network/content/transfer_sender.cpp` | content transfer (already grouped; keep in place) |
| `include/karma/network/content/transfer_receiver.hpp` | `include/karma/network/content/transfer_receiver.hpp` | content transfer (already grouped; keep in place) |
| `src/engine/network/content/transfer_receiver.cpp` | `src/engine/network/content/transfer_receiver.cpp` | content transfer (already grouped; keep in place) |
| `include/karma/common/content/primitives.hpp` (transfer helper subset) | `include/karma/network/content/transfer_integrity.hpp` | content transfer integrity |
| `src/engine/common/content/primitives.cpp` (transfer helper subset) | `src/engine/network/content/transfer_integrity.cpp` | content transfer integrity |
| `include/karma/common/curl_global.hpp` | `include/karma/network/http/curl_global.hpp` | network http |
| `src/engine/common/curl_global.cpp` | `src/engine/network/http/curl_global.cpp` | network http |
| `src/engine/network/tests/loopback_endpoint_alloc.cpp` | `src/engine/network/tests/support/loopback_endpoint_alloc.cpp` | test support |
| `src/engine/network/tests/loopback_endpoint_alloc.hpp` | `src/engine/network/tests/support/loopback_endpoint_alloc.hpp` | test support |
| `src/engine/network/tests/loopback_enet_fixture.cpp` | `src/engine/network/tests/support/loopback_enet_fixture.cpp` | test support |
| `src/engine/network/tests/loopback_enet_fixture.hpp` | `src/engine/network/tests/support/loopback_enet_fixture.hpp` | test support |
| `src/engine/network/tests/loopback_transport_fixture.hpp` | `src/engine/network/tests/support/loopback_transport_fixture.hpp` | test support |
| `src/engine/network/tests/structured_log_event_sink.cpp` | `src/engine/network/tests/support/structured_log_event_sink.cpp` | test support |
| `src/engine/network/tests/structured_log_event_sink.hpp` | `src/engine/network/tests/support/structured_log_event_sink.hpp` | test support |
| `src/engine/network/tests/client_transport_contract_test.cpp` | `src/engine/network/tests/contracts/client_transport_contract_test.cpp` | contract test |
| `src/engine/network/tests/server_transport_contract_test.cpp` | `src/engine/network/tests/contracts/server_transport_contract_test.cpp` | contract test |

## Execution Plan
1. N0 taxonomy + scaffolding:
- add target subdirectories in source/include and CMake source lists,
- move one bounded transport contract pair with forwarding wrappers to validate migration pattern.

2. N1 transport module migration:
- move `client_transport`, `server_transport`, and `transport_pump_normalizer`,
- keep legacy wrapper headers in `include/karma/network/` during transition.

3. N2 config/policy migration:
- move `transport_config_mapping` + `client_reconnect_policy` under `network/config`,
- keep compatibility wrappers for legacy include paths.

4. N3 server auth/session migration:
- move `server_preauth` to `network/server/auth`,
- move join/leave/session hooks helpers to `network/server/session`.

5. N4 community heartbeat migration:
- move heartbeat files to `network/community`,
- preserve API behavior while clarifying server-oriented ownership.

6. N4.5 content transfer-integrity helper extraction:
- introduce `include/karma/network/content/transfer_integrity.hpp` and `src/engine/network/content/transfer_integrity.cpp`.
- move transfer-coupled helper subset from `common/content/primitives` (chunk bounds, buffered-chunk match, chunk-chain helpers) behind compatibility forwarding wrappers during migration.
- keep generic hash/path helpers in `common/content/primitives`.

7. N4.6 curl-global relocation:
- move curl-global init helper from `common/curl_global.*` to `network/http/curl_global.*`.
- keep temporary compatibility forwarding header/source at legacy `common/curl_global.*` path until wrapper retirement.

8. N5 tests layout migration:
- move fixtures/sinks into `tests/support`,
- move contract tests into `tests/contracts`,
- keep `network_test_support` and contract test targets behavior-identical.

9. N6 wrapper retirement:
- once all callsites are updated, remove legacy flat wrappers,
- finalize docs and board status.

## Validation
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir>
./scripts/test-server-net.sh <build-dir>
ctest --test-dir <build-dir> -R 'client_transport_contract_test|server_transport_contract_test' --output-on-failure
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `net.server`
- `net.client`
- `engine.server`
- `config`

## Build/Run Commands
```bash
./abuild.py -c -d <build-dir>
./scripts/test-server-net.sh <build-dir>
```

## First Session Checklist
1. Read `AGENTS.md`, `docs/foundation/policy/execution-policy.md`, and this project file.
2. Confirm no overlap with active `engine-game-boundary-hygiene` ownership scope.
3. Execute only N0 (one bounded move + compatibility wrapper).
4. Run required validation.
5. Update this file and `docs/projects/ASSIGNMENTS.md` in the same handoff.

## Current Status
- `2026-02-17`: project created to address flat `src/engine/network` layout, mixed concerns, and prefix-heavy naming.
- `2026-02-17`: directory-first taxonomy and current->target mapping drafted.
- `2026-02-17`: execution not started; N0 scaffolding is next.
- `2026-02-18`: mapping table refreshed against current `src/engine/network` inventory; added explicit accounting rows for `network/content/transfer_{sender,receiver}` and `tests/loopback_transport_fixture.hpp`.
- `2026-02-18`: boundary recheck promoted to concrete goals: add `network/content/transfer_integrity` from transfer-only primitive subset and relocate `curl_global` to `network/http`.

## Open Questions
- Should server community heartbeat remain under `network/community/*`, or move to `network/server/community/*` for stricter server-only ownership?
- Should compatibility wrappers preserve `karma::network` names indefinitely, or should the end-state namespace introduce scoped aliases (`karma::network::transport`, `karma::network::server::session`, etc.)?
- Should `transport_pump_normalizer.hpp` remain transport-internal only (`src/engine/network/transport/*`) with no public include exposure?

## Handoff Checklist
- [x] File taxonomy and target directories documented.
- [x] Current file inventory mapped to target locations.
- [x] Non-goals and boundary constraints documented.
- [x] Additional network move goals recorded with concrete target names/paths.
- [ ] N0 scaffolding slice implemented and validated.
- [ ] N4.5 transfer-integrity helper extraction implemented and validated.
- [ ] N4.6 curl-global relocation implemented and validated.
- [ ] ASSIGNMENTS row updated with active owner/status once execution starts.
