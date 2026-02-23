# Disconnect Reasons Migration (`m-dev` -> `m-karma`/`m-bz3`)

## Project Snapshot
- Current owner: `unassigned`
- Status: `in progress`
- Immediate next task: inventory all disconnect/reject/kick reason semantics in `m-dev` and map them to canonical JSON keys in `m-karma` + `m-bz3`.
- Validation gate: `cd m-overseer && ./agent/scripts/lint-projects.sh`

## Mission (Primary Focus)
Bring over disconnect/reject/kick reason behavior from `m-dev` and integrate it with a JSON-first config contract so hardcoded reason strings are removed from runtime code.

Primary outcomes:
- parity with `m-dev` reason semantics where still relevant,
- canonical reason strings in distro JSON (`data/{client,server}/config.json`),
- callsites converted from literals to required config reads,
- `m-karma` and `m-bz3` kept in sync for shared contracts.

## Scope
1. `m-dev` source inventory (authoritative reference behavior)
2. `m-karma` server/client reason paths
3. `m-bz3` server/client reason paths
4. shared schema + validation + tests

## Source Baseline (`m-dev`)
Known reason behavior to port or reconcile:
- server join rejection reasons (`Protocol version mismatch.`, `Name already in use.`, `Join request required.`, `Join request mismatch.`)
- admin/plugin kick/disconnect reason flow (`disconnect_player` / `kick_player` with reason)
- client-side mapping from reason text to user-facing status/dialog messages
- explicit transport disconnect actions following reason emission

Reference files:
- `m-dev/src/game/server/game.cpp`
- `m-dev/src/game/net/backends/enet/server_backend.cpp`
- `m-dev/src/game/server/plugin.cpp`
- `m-dev/src/game/client/main.cpp`

## Canonical Config Direction
Use required server/client namespaced keys for reason text and disconnect code policy.

Initial canonical keys already introduced:
- `server.runtime.joinRejectReasons.*`
- `server.network.disconnectCodes.*`

Next expected expansion:
- `server.runtime.disconnectReasons.*` (server-originated explicit reasons)
- `server.runtime.kickReasons.*` (admin/plugin/moderation flows)
- `client.runtime.disconnectMessages.*` (client presentation mapping)

## Implementation Rules
- no hardcoded user-facing reason strings in runtime code paths once migrated,
- reason literals move to required JSON keys,
- transport/protocol numeric reason codes stay stable and shared (JSON-backed in distro defaults, validated at startup),
- `m-bz3/data/{client,server}/config.json` mirrors `m-karma` for shared keys.

## Workflow
1. Audit one runtime path (server first, then client) and list literals + fallback behavior.
2. Present candidate batch as matrix.
3. Approve key naming and destination (`server` vs `client` config).
4. Implement required reads and JSON values in `m-karma`.
5. Mirror shared keys in `m-bz3`.
6. Update/extend tests to assert reason payloads and behavior.

## Candidate Matrix Template
| Proposed key | Value being set | Conversion notes |
|---|---|---|
| `server.runtime.disconnectReasons.ProtocolMismatch` | `"Protocol version mismatch."` | `creating for hardcoded value` |

Rules:
- no index column,
- include `server.` or `client.` prefix,
- include value notes if behavior-specific,
- keep notes short.

## Immediate Backlog
1. Complete `m-dev` reason inventory with keep/rename/drop decisions.
2. Finish converting remaining hardcoded reason literals in:
   - `m-karma/src/demo/server/runtime.cpp`
   - `m-karma/src/network/server/session/hooks.cpp`
   - `m-bz3/src/client/net/connection/inbound/bootstrap.cpp`
3. Add shared validation coverage for new required keys.
4. Add contract tests for:
   - join rejection reason payloads,
   - disconnect/kick reason propagation,
   - client-facing message mapping behavior.

## Validation
```bash
cd m-overseer
./agent/scripts/lint-projects.sh
```

```bash
cd m-karma
cmake --build build-test --target karma_demo_server demo_protocol_contract_test -- -j4
ctest --test-dir build-test -R demo_protocol_contract_test --output-on-failure
```

```bash
cd m-bz3
cmake --build build-test --target server_net_contract_test -- -j4
ctest --test-dir build-test -R server_net_contract_test --output-on-failure
```

## Handoff Checklist
- [ ] `m-dev` reason inventory captured and triaged.
- [ ] Approved key matrix completed for next batch.
- [ ] `m-karma` callsites converted to required JSON keys.
- [ ] Shared keys mirrored in `m-bz3` JSON.
- [ ] Contract tests updated for reason behavior.
