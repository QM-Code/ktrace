# Testing Modernization (`m-karma` + `m-bz3`)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (binary-first migration track active)`
- Immediate next task: Continue replacing ad-hoc/in-process tests with binary+demo fixture workflows and capture trace-based evidence.
- Validation gate: `cd m-overseer && ./agent/scripts/lint-projects.sh`

## Mission
Consolidate ad-hoc testing strategies using ad-hoc data in m-karma into demo client/server binary test using data from m-karma/data/.

End goal:
- ad-hoc test setup replaced by reproducible `demo/` fixtures and binary traces.
- few to no test binaries built; try to get as much testing as possible into <build-dir>/{client,server}

## Foundation References
- `m-overseer/agent/docs/testing.md`
- `m-overseer/agent/projects/testing.md`
- `m-karma/webserver/AGENTS.md`

## Owned Paths
- `m-overseer/agent/projects/testing.md`
- `m-karma/demo/servers/*`
- `m-karma/demo/users/*` (new clones only; no edits to existing fixtures)
- `m-karma/demo/communities/*` (new clones only; no edits to existing fixtures)
- `m-karma/src/*` config callsites touched by approved batches

## Interface Boundaries
- Inputs consumed:
  - runtime config contracts from `data/{client,server}/config.json`
  - community API behavior from `m-karma/webserver`
- Outputs/contracts exposed:
  - canonical config keys and required-read behavior
  - deterministic `demo/` fixtures and binary-driven verification steps
- Coordination constraints:
  - do not modify existing `demo/users/*`
  - do not modify existing `demo/communities/*`; create clones when needed

## Worked Example (Heartbeat Migration)
- Canonicalized heartbeat overlays:
  - `m-karma/demo/servers/test-heartbeat/config.json`
  - `m-bz3/demo/servers/test-heartbeat/config.json`
- Added cloned community fixtures:
  - `m-karma/demo/communities/test-heartbeat/`
  - `m-bz3/demo/communities/test-heartbeat/`
- Updated cloned DBs to include `127.0.0.1:11899` server registration for heartbeat acceptance.
- Added runbook docs:
  - `m-karma/demo/servers/test-heartbeat/README.md`
  - `m-bz3/demo/servers/test-heartbeat/README.md`
- Verified with traces and community heartbeat log entries.

## Key Extension Points
- Convert in-process config mutation tests to binary/fixture paths where practical:
  - `m-bz3/src/tests/community_heartbeat_integration_test.cpp`
  - selected flows in `m-bz3/src/tests/server_net_contract_test.cpp`
- Normalize remaining `demo/servers/*/config.json` overlays to canonical key contracts.

# Potential Replacement Targets:
- server_transport_contract_test (`m-karma`) / server_net_contract_test (`m-bz3`)
- client_transport_contract_test
- demo_protocol_contract_test 

## Non-Goals
- Do not remove fast unit/contract tests that still provide high signal.
- Do not rely on personal home config paths or ad-hoc `/tmp` fixture trees for reusable tests.
- Do not mutate existing community/user fixtures without explicit approval.

## Validation
```bash
cd m-overseer
./agent/scripts/lint-projects.sh
```

```bash
# Example heartbeat smoke (m-bz3)
python3 ../m-karma/webserver/bin/start.py demo/communities/test-heartbeat
timeout 10s ./build-test/bz3-server \
  --server-config demo/servers/test-heartbeat/config.json \
  --trace net.server,engine.server
tail -n 20 demo/communities/test-heartbeat/logs/heartbeat.log
```

## Trace Channels
- `net.server`
- `engine.server`
- `net.client`
- `engine.app`

## Build/Run Commands
```bash
cd m-karma && ./abuild.py -a mike -d build-test --install-sdk out/karma-sdk
cd m-bz3 && ./abuild.py -a mike -d build-test --karma-sdk ../m-karma/out/karma-sdk
```

## Current Status
- `2026-02-23`: heartbeat fixture migration completed as initial worked example.
- `2026-02-23`: cloned `test-heartbeat` communities created in both repos to keep existing fixtures untouched.
- `2026-02-23`: smoke run generated untracked artifact `m-bz3/demo/servers/test-heartbeat.zip` (cleanup policy pending).
- `2026-02-24`: contract baseline check passed in `m-karma` (`ctest -R 'client_transport_contract_test|server_transport_contract_test|demo_protocol_contract_test'`) with 3/3 passing.

### `2026-02-24` Binary Reproduction Analysis (`m-karma`)

Scope:
- `server_transport_contract_test` (`src/network/tests/contracts/server_transport_contract_test.cpp`)
- `client_transport_contract_test` (`src/network/tests/contracts/client_transport_contract_test.cpp`)
- `demo_protocol_contract_test` target (`src/demo/tests/protocol_contract_test.cpp`)

Reproduced with `client/server` binaries + `demo/` fixtures:
- Happy-path lifecycle (`connect -> join -> ping/pong -> leave -> disconnect -> session-complete`) using:
  - server: `demo/servers/test-players/config.json`
  - client: `demo/users/default/config.json`
  - note: client required `--strict-config=false` with current fixture.
- Join-reject flow (community auth enabled/no credential payload) using `demo/servers/test-heartbeat/config.json`.
- Runtime backend contract behavior:
  - server `server.network.TransportBackend:"auto"` and `"enet"` both bind/listen.
  - unregistered backend on server/client yields warning + app-level startup failure.
- Repeated-session behavior:
  - default `server.runtime.session.TerminateOnFirstTerminalEvent:true` exits after first terminal session.
  - overriding to `false` allowed 12/12 sequential client sessions to complete successfully.
- Protocol boundary from binary path:
  - oversized username (`--username` >64 chars) rejected client-side before join send.

Partially reproducible / not reproducible from stock binaries alone:
- Synthetic transport normalizer assertions (staged event ordering/per-peer normalization) are harness-only.
- Client reconnect interleave and timeout/disconnect race stress paths are harness-driven via loopback fixture control.
- Protocol malformed-packet decode cases (bad version, truncated join payload) and reject-encode invariants require codec-level or packet-crafting harnesses.

Notable constraints discovered:
- `demo/users/default/config.json` currently fails strict required config keys for client; binary repro required `--strict-config=false`.
- `test-heartbeat` fixture is useful for reject-path validation but rejects default client join without expected credential payload.
- Concurrent multi-client run reached server-side success for all sessions, while a subset of client wrappers timed out after `leave-request`; treat this as a binary runner/wait-state artifact until isolated with dedicated traces.

## Open Questions
- Which suites should remain in-process for speed, and which should move to binary fixture runs?
- Should generated artifacts such as `demo/servers/test-heartbeat.zip` be gitignored, auto-cleaned, or intentionally tracked?

## Handoff Checklist
- [ ] Candidate batch presented using the matrix template.
- [ ] Approved keys implemented in code and canonical JSON.
- [ ] Binary trace/log evidence captured for changed behavior.
- [ ] Existing `demo/users/*` and `demo/communities/*` remain untouched unless explicitly approved.
