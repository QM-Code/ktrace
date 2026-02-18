# Webserver Unit Tests (Execution Track)

## Project Snapshot
- Current owner: `codex`
- Status: `completed/maintenance (handler mutation-flow coverage, including permission/CSRF guardrails, landed)`
- Immediate next task: maintenance-only: when webserver handler internals change, land matching unit-test updates in the same slice.
- Validation gate:
  - `python3 ./src/webserver/tests/validate_strings.py --all`
  - `python3 -m unittest discover -s src/webserver/tests/unit -p "test_*.py"`

## Mission
Create and grow durable, fast unit-test coverage for webserver handler logic so community/auth behavior can be validated without relying only on manual browser checks.

## Foundation References
- `docs/foundation/governance/community-webserver-testing.md`
- `docs/foundation/governance/testing-ci-governance.md`
- `docs/foundation/policy/execution-policy.md`

## Why This Is Separate
Webserver handler behavior (auth, heartbeat, active-list shaping) can be tested mostly independently from renderer/physics/content work, and needs quick-turn regression feedback during server/client integration.

## Owned Paths
- `src/webserver/tests/*`
- `src/webserver/karma/handlers/*`
- `src/webserver/karma/db.py`
- `src/webserver/karma/config.py`

## Interface Boundaries
- Inputs consumed from game/server side:
  - heartbeat payload contract (`server`, `players`, `max`, optional `newport`)
  - auth payload contract (`/api/auth`, `/api/user_registered`)
- Outputs/contracts exposed to game/client side:
  - `/api/servers/active` payload shape and sorting
  - `/api/info` community metadata
  - auth success/failure response codes and error payload shape

## Non-Goals
- Do not redesign API contracts in this project.
- Do not block on GUI wiring.
- Do not move long-lived governance details out of `docs/foundation/`.

## Validation
From `m-rewrite/`:

```bash
python3 ./src/webserver/tests/validate_strings.py --all
python3 -m unittest discover -s src/webserver/tests/unit -p "test_*.py"
```

Integration checks should follow `docs/foundation/governance/community-webserver-testing.md`.

## First Unit-Test Targets
1. `/api/servers/active`
   - active/inactive filtering
   - count fields (`active_count`, `inactive_count`)
   - sorting by player count
2. `/api/heartbeat`
   - registered host:port updates heartbeat and player counts
   - unregistered host or port mismatch returns expected error/status
3. `/api/auth` and `/api/user_registered`
   - registered valid credentials
   - registered invalid credentials
   - unregistered username behavior for handshake consumers

## Current Status
- `2026-02-13`: project created; governance test matrix anchored in foundation doc.
- `2026-02-13`: landed `unittest` harness and first `/api/servers/active` test suite in `src/webserver/tests/unit/test_api_servers_active.py` (active/inactive counts, owner filter, active sort by player count).
- `2026-02-13`: landed `/api/heartbeat` unit tests in `src/webserver/tests/unit/test_api_heartbeat.py` (registered host:port success update + registered-host port mismatch rejection).
- `2026-02-13`: landed `/api/auth` and `/api/user_registered` unit tests in `src/webserver/tests/unit/test_api_auth.py` (registered-valid passhash, registered-invalid passhash, unregistered user rejection, and user-registered salt lookup behavior).
- `2026-02-13`: expanded `src/webserver/tests/unit/test_api_auth.py` with edge-case coverage for missing credentials/username and method restrictions (`GET` debug gate, unsupported methods for both auth endpoints).
- `2026-02-13`: expanded `src/webserver/tests/unit/test_api_auth.py` with `world`-scoped local-admin behavior coverage (owner, direct admin, trusted delegation allowed; untrusted delegation denied; missing world fallback).
- `2026-02-13`: added comprehensive `/api/admins` contract coverage in `src/webserver/tests/unit/test_api_admins.py` (success payload shaping, method constraints, and host/port/owner error cases).
- `2026-02-13`: added `/api/info` + `/api/health` contract coverage in `src/webserver/tests/unit/test_api_info_health.py` (success and method restrictions).
- `2026-02-13`: added `/api/server` lookup coverage in `src/webserver/tests/unit/test_api_server.py` (name/code/id/token lookup paths, active flag behavior, and errors).
- `2026-02-13`: added `/api/user` lookup coverage in `src/webserver/tests/unit/test_api_user.py` (name/code lookup, active/inactive counts, sort/truncation shaping, and errors).
- `2026-02-13`: expanded `src/webserver/tests/unit/test_api_servers_active.py` with method/status/owner-not-found error cases plus overview truncation assertions.
- `2026-02-13`: expanded `src/webserver/tests/unit/test_api_heartbeat.py` for full heartbeat validation/error coverage (`method`, `debug GET gate`, `missing/invalid fields`, `host_not_found`, `newport` conflict/success).
- `2026-02-13`: expanded `src/webserver/tests/unit/test_api_auth.py` with email lookup, locked/deleted rejection, community-admin flag, and `debug.auth=true` GET acceptance.
- `2026-02-13`: added `src/webserver/tests/unit/test_router_dispatch.py` covering API route dispatch, token-path rewriting, static/upload traversal rejection, and static/upload cache-header serving.
- `2026-02-13`: added `src/webserver/tests/unit/test_account_handler.py` covering account handler GET renders, POST CSRF enforcement, logout cookie clearing, and register/login success redirects.
- `2026-02-13`: added `src/webserver/tests/unit/test_router_users_permissions.py` covering users-management route permission gates and tokenized `/users/*` dispatch rewriting.
- `2026-02-18`: added `src/webserver/tests/unit/test_handler_mutation_flows.py` with mutation-flow DB assertions for:
  - `users.py`: create user (including root-admin grant path), lock user, and self settings edit (email/language/password).
  - `user_profile.py`: add/trust/remove admin mutation actions.
  - `server_edit.py`: edit server metadata and delete server flows.
- `2026-02-18`: expanded `src/webserver/tests/unit/test_handler_mutation_flows.py` with negative mutation-path coverage:
  - `users.py`: invalid-CSRF create rejection and non-admin lock rejection with no DB mutation.
  - `user_profile.py`: invalid-CSRF add-admin rejection and non-manager add-admin rejection with no DB mutation.
  - `server_edit.py`: invalid-CSRF edit/delete rejection and non-owner delete rejection with no DB mutation.
- `2026-02-18`: current suite inventory is 11 test modules under `src/webserver/tests/unit/` with `108` passing tests (`python3 -m unittest discover -s src/webserver/tests/unit -p "test_*.py"`).

## Open Questions
- Should the harness standardize on `unittest` only, or allow `pytest` if already available?
- Should handler tests run against temporary sqlite DB fixtures or shared seeded test snapshots?

## Handoff Checklist
- [x] Unit-test harness directory exists and runs.
- [x] At least one webserver handler test is committed.
- [x] Validation commands and outcomes recorded.
- [x] Follow-up slices queued in `docs/projects/ASSIGNMENTS.md`.
