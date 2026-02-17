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
- `docs/projects/engine-game-boundary-hygiene.md`

## Why This Is Separate
This is structural hygiene for engine network ownership and naming, not gameplay/netcode behavior work. Keeping it separate prevents accidental coupling with active protocol/content slices while allowing low-risk, behavior-neutral refactors.

## Owned Paths
- `docs/projects/engine-network-module-hygiene.md`
- `docs/projects/ASSIGNMENTS.md`
- `include/karma/network/*`
- `src/engine/network/*`
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
  - `docs/projects/engine-game-boundary-hygiene.md`

## Non-Goals
- Do not change wire protocol schema or message semantics.
- Do not change gameplay session authority decisions.
- Do not redesign reconnect/auth/heartbeat behavior in this project.
- Do not fold `network/content/*` extraction work from `engine-game-boundary-hygiene` into this track.

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
| `src/engine/network/tests/loopback_endpoint_alloc.*` | `src/engine/network/tests/support/loopback_endpoint_alloc.*` | test support |
| `src/engine/network/tests/loopback_enet_fixture.*` | `src/engine/network/tests/support/loopback_enet_fixture.*` | test support |
| `src/engine/network/tests/structured_log_event_sink.*` | `src/engine/network/tests/support/structured_log_event_sink.*` | test support |
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

6. N5 tests layout migration:
- move fixtures/sinks into `tests/support`,
- move contract tests into `tests/contracts`,
- keep `network_test_support` and contract test targets behavior-identical.

7. N6 wrapper retirement:
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

## Open Questions
- Should server community heartbeat remain under `network/community/*`, or move to `network/server/community/*` for stricter server-only ownership?
- Should compatibility wrappers preserve `karma::network` names indefinitely, or should the end-state namespace introduce scoped aliases (`karma::network::transport`, `karma::network::server::session`, etc.)?
- Should `transport_pump_normalizer.hpp` remain transport-internal only (`src/engine/network/transport/*`) with no public include exposure?

## Handoff Checklist
- [x] File taxonomy and target directories documented.
- [x] Current file inventory mapped to target locations.
- [x] Non-goals and boundary constraints documented.
- [ ] N0 scaffolding slice implemented and validated.
- [ ] ASSIGNMENTS row updated with active owner/status once execution starts.
