# Server Network

## Project Snapshot
- Current owner: `codex`
- Status: `in progress` (disconnect integration harness now covers rapid reconnect/leave ordering churn)
- Immediate next task: add one cross-peer reconnect race edge case (new join while prior disconnect drains).
- Validation gate: `./scripts/test-server-net.sh`

## Mission
Own server-side networking/event-source/runtime behavior for join/spawn/shot/leave flows and keep protocol contracts stable.

## Owned Paths
- `m-rewrite/src/game/server/*`
- `m-rewrite/src/game/server/net/*`
- `m-rewrite/src/game/net/*`
- `m-rewrite/src/game/tests/transport_*`
- `m-rewrite/src/game/tests/server_*`

## Interface Boundaries
- Inputs: client protocol messages.
- Outputs: runtime events and server->client protocol messages.
- Coordinate before changing:
  - `m-rewrite/src/game/protos/messages.proto`
  - `m-rewrite/src/game/net/protocol.hpp`
  - `m-rewrite/src/game/net/protocol_codec.*`
  - `m-rewrite/src/game/CMakeLists.txt`

## Current Test Suite
1. `server_net_contract_test`
- protocol codec roundtrips for join/init/create-shot paths,
- scripted event-source parsing/validation.

2. `server_runtime_event_rules_test`
- runtime-event rule invariants without transport-backend dependency,
- unknown leave/spawn/create_shot ignored,
- known-client gates for spawn/shot,
- join add/idempotency and leave removal behavior.

3. `transport_loopback_integration_test`
- transport loopback join and packet delivery with runtime processing,
- includes env probe and clean skip when unavailable.

4. `transport_multiclient_loopback_test`
- multi-client loopback connection + runtime event coverage,
- includes env probe and clean skip when unavailable.

5. `transport_disconnect_lifecycle_integration_test`
- disconnect/explicit-leave lifecycle correctness and stale-id suppression,
- validates no duplicate leave emission across explicit leave + transport disconnect,
- validates rapid reconnect/leave churn ordering (`join` precedes single `leave` per churn client id).

6. `transport_environment_probe_test`
- validates transport loopback environment preconditions used by loopback integration tests.

7. `client_world_package_safety_integration_test`
- validates world-transfer safety boundaries while server networking is active.

## How To Run
From `m-rewrite/`:

```bash
./scripts/test-server-net.sh
```

Equivalent explicit commands:

```bash
cmake --build build-dev --target \
  transport_environment_probe_test \
  server_net_contract_test \
  server_runtime_event_rules_test \
  transport_loopback_integration_test \
  transport_multiclient_loopback_test \
  transport_disconnect_lifecycle_integration_test \
  client_world_package_safety_integration_test

ctest --test-dir build-dev -R "server_net_contract_test|server_runtime_event_rules_test|transport_environment_probe_test|transport_loopback_integration_test|transport_multiclient_loopback_test|transport_disconnect_lifecycle_integration_test|client_world_package_safety_integration_test" --output-on-failure
```

## Result Interpretation Rules
1. `server_*` failures are actionable contract/runtime regressions.
2. transport loopback tests may `SKIP` when loopback prerequisites are unavailable.
3. Unexpected transport loopback failure in a healthy environment is actionable.

## Validation
From `m-rewrite/`:

```bash
./scripts/test-server-net.sh
```

## Trace Channels
- `engine.server`
- `net.server`
- `net.client`

## Archive Reference
Full legacy material preserved at:
- `docs/archive/server-network-legacy-2026-02-09.md`

## Handoff Checklist
- [x] Server/network behavior changes are explicit and justified.
- [x] Protocol/schema files unchanged (`messages.proto`, `protocol.hpp`, `protocol_codec.*`).
- [x] Assigned build + wrapper validation commands run, with results recorded.
- [x] Trace signal quality preserved.

## Status/Handoff Notes (2026-02-10)
- Added one reconnect/leave ordering edge slice in runtime integration harness:
  - file: `m-rewrite/src/game/tests/transport_disconnect_lifecycle_integration_test.cpp`
  - new churn phase runs rapid leave + double-disconnect + reconnect cycles and asserts:
    - unique client ids across reconnect churn
    - single leave emission per churn client id
    - per-client event ordering (`ClientJoin` before `ClientLeave`) under rapid churn
    - stale explicit-leave payload id cannot hijack the active reconnect client identity
- Protocol/schema scope intentionally unchanged:
  - no edits to `messages.proto`, `protocol.hpp`, `protocol_codec.*`
- Validation run in assigned build profile:
  - `cd m-rewrite && ./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio`
  - Result: success (`[100%] Built target bz3`)
  - `cd m-rewrite && ./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio`
  - Result: PASS (7/7), including `transport_disconnect_lifecycle_integration_test`.
