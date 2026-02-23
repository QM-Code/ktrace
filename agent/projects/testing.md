# Testing Modernization (`m-karma` + `m-bz3`)

## Project Snapshot
- Current owner: `unassigned`
- Status: `in progress`
- Immediate next task: continue the key-audit loop for `m-karma` server startup and convert the next approved batch to required JSON keys.
- Validation gate: `cd m-overseer && ./agent/scripts/lint-projects.sh`

## Mission (Primary Focus)
Primary focus is a binary-first config migration workflow:
- walk real startup/runtime paths in shipped binaries,
- find hardcoded/default/fallback config behavior,
- move those values into canonical JSON (`data/{client,server}/config.json`),
- convert callsites to required reads where appropriate,
- validate through binary runs and trace/log evidence using tracked `demo/` fixtures.

This aligns testing modernization with config canonicalization: the same flow that removes defaults from code also strengthens real-world fixture-based validation.

## Scope Order (Authoritative)
1. `m-karma` server binary
2. `m-karma` client binary
3. `m-bz3` server binary
4. `m-bz3` client binary

End goal:
- all primary config files migrated to canonical key paths/casing,
- hardcoded/default/fallback runtime values removed from targeted paths,
- ad-hoc test setup replaced by reproducible `demo/` fixtures and binary traces.

## Workflow Template (Authoritative)
1. Pick current binary scope from the order above.
2. Trace startup/runtime path and list fallback/default/hardcoded values.
3. Present candidate batch in the matrix format below.
4. Confirm naming/location decisions.
5. Implement batch in code and JSON.
6. Keep `m-karma` and `m-bz3` canonical config baselines aligned where contracts are shared.
7. Validate with build + binary run + trace/log evidence from `demo/`.
8. Record output and next batch in this project track.

## Candidate Matrix Template
Use this exact output format when presenting each batch.

| Proposed key | Value being set | Conversion notes |
|---|---|---|
| `server.example.Path` | `123` (clamped min `1`) | `keeping as is` / `converting case` / `creating for hardcoded value` |

Rules:
- no index column,
- include explicit `server.` or `client.` prefix in proposed key,
- include value qualifications (clamps, bounds, sentinels),
- keep conversion notes short and implementation-oriented.

## Foundation References
- `m-overseer/agent/docs/testing.md`
- `m-overseer/agent/projects/hardcoded-fallback-audit-2026-02-23.md`
- `m-karma/webserver/AGENTS.md`
- `m-bz3/demo/README.md`

## Owned Paths
- `m-overseer/agent/projects/testing.md`
- `m-karma/demo/servers/*`
- `m-bz3/demo/servers/*`
- `m-karma/demo/communities/*` (new clones only; no edits to existing fixtures)
- `m-bz3/demo/communities/*` (new clones only; no edits to existing fixtures)
- `m-karma/src/*` and `m-bz3/src/*` config callsites touched by approved batches

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
- Continue server-runtime fallback removal in:
  - `m-bz3/src/server/runtime/run.cpp`
  - `m-bz3/src/server/runtime/config.cpp`
- Continue client-runtime fallback removal in:
  - `m-karma/src/demo/client/runtime.cpp`
  - `m-bz3/src/client/runtime/*`
- Convert in-process config mutation tests to binary/fixture paths where practical:
  - `m-bz3/src/tests/community_heartbeat_integration_test.cpp`
  - selected flows in `m-bz3/src/tests/server_net_contract_test.cpp`
- Normalize remaining `demo/servers/*/config.json` overlays to canonical key contracts.

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
- `2026-02-23`: project rewritten to prioritize binary-first config-audit workflow.
- `2026-02-23`: heartbeat fixture migration completed as initial worked example.
- `2026-02-23`: cloned `test-heartbeat` communities created in both repos to keep existing fixtures untouched.
- `2026-02-23`: smoke run generated untracked artifact `m-bz3/demo/servers/test-heartbeat.zip` (cleanup policy pending).

## Open Questions
- Which suites should remain in-process for speed, and which should move to binary fixture runs?
- Should generated artifacts such as `demo/servers/test-heartbeat.zip` be gitignored, auto-cleaned, or intentionally tracked?

## Handoff Checklist
- [ ] Candidate batch presented using the matrix template.
- [ ] Approved keys implemented in code and canonical JSON.
- [ ] Binary trace/log evidence captured for changed behavior.
- [ ] Existing `demo/users/*` and `demo/communities/*` remain untouched unless explicitly approved.
