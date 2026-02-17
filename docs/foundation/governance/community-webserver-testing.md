# Community/Webserver Test Governance

This is the long-lived reference for regular community/webserver integration testing.

For the canonical end-to-end auth validation workflow using demo virtual users/worlds/communities, use:
- `docs/foundation/governance/community-auth-demo-testing.md`

## Scope
Use this when touching:
- `src/webserver/*`
- `src/game/server/*` community heartbeat or auth handshake paths
- `src/game/client/*` community server list or community auth paths

## Unit Coverage Baseline
Current status (`2026-02-13`):
- `src/webserver/tests/unit/` contains 10 modules covering API handlers, router dispatch/guards, and account/router permission gates.
- Baseline command result: `93` passing tests.

Quick run:

```bash
python3 -m unittest discover -s src/webserver/tests/unit -p "test_*.py"
```

## When To Run
Required triggers:
1. Any change under `src/webserver/*`.
2. Any change to community-facing game integration paths in:
   - `src/game/server/*` (heartbeat/auth handshake/community endpoint use)
   - `src/game/client/*` (community list/auth polling paths)
3. Any change to community config/string assets used by webserver flows:
   - `src/webserver/config.json`
   - `src/webserver/karma/strings/*`
   - `demo/communities/*`

Required moments:
1. Before handoff/PR for any triggered change.
2. After rebasing/merging when triggered files changed upstream.

Periodic posture while webserver work is active:
1. Run unit gate + string validation at least once per active development day, even without local webserver code edits.

## Preconditions
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir>
```

Use isolated build dirs and `./abuild.py`-only workflow per `docs/foundation/policy/execution-policy.md`.

## Tier 1: Multi-Community Boot + Language Posture
Goal:
- prove multiple webservers can run concurrently on unique ports
- prove each instance is loading its own community config directory
- prove language posture is valid (explicit language and base-language fallback)

Recommended demo pair:
- `demo/communities/r55man-es` (explicit `server.language=es`)
- `demo/communities/r55man` (base-language fallback)

Procedure:
1. Start two webservers on different ports:

```bash
python3 ./src/webserver/bin/start.py ./demo/communities/r55man-es -p 8081
python3 ./src/webserver/bin/start.py ./demo/communities/r55man -p 8080
```

2. Validate each responds with its own community identity:

```bash
curl -fsS http://localhost:8081/api/info
curl -fsS http://localhost:8080/api/info
```

3. Validate string-shape integrity for all language packs:

```bash
python3 ./src/webserver/tests/validate_strings.py --all
```

Pass criteria:
- both instances run simultaneously without port/config cross-talk
- `/api/info` payloads reflect the expected community for each directory
- string validation passes

## Tier 2: Multi-Community + Multi-Server Heartbeat
Goal:
- prove multiple game-server instances can map to different communities
- prove each community active list reflects only its own heartbeating servers

Target topology:
- 2 community webservers
- 3 game servers per community (6 total)

Procedure:
1. Ensure each community DB has registered host:port rows for planned servers.
2. Start 3 `bz3-server` instances per community with explicit unique `--listen-port` and correct community target (via world/server config `community.server` or explicit `--community` override).
3. Query each community list from web API:

```bash
curl -fsS http://localhost:8080/api/servers/active
curl -fsS http://localhost:8081/api/servers/active
```

Pass criteria:
- each list shows only that community's active servers
- expected active count equals started-and-registered server count

## Tier 3: Multi-Client Poll + Connect
Goal:
- run 5-10 client sessions that poll community lists and attempt connect
- ensure at least one server handles >=3 concurrent clients in comprehensive runs

Current status (`2026-02-16`):
- fully scripted multi-client connect harness is not yet landed in `m-rewrite`
- use repeatable CLI/curl loops for poll+connect evidence capture.

Required posture until harness lands:
- run list-poll fanout as automated
- run connect coverage as manual/dev-driven smoke and record outcomes

## Tier 4: Auth Matrix Coverage
Goal:
- verify public-server auth outcomes:
  - registered username + valid password -> allow
  - registered username + invalid password -> reject
  - unregistered username (not currently in-session duplicate) -> allow

Current status (`2026-02-16`):
- demo-backed community auth flow is available and documented for manual/agent validation.
- canonical procedure, examples, and troubleshooting are in:
  - `docs/foundation/governance/community-auth-demo-testing.md`

## Evidence Capture
For each run, record:
1. commands used (with ports/community dirs/build dir)
2. active list outputs per community
3. server heartbeat/auth log snippets
4. failures and rerun outcomes

## Related Docs
- `docs/foundation/governance/testing-ci-governance.md`
- `docs/foundation/governance/community-auth-demo-testing.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/projects/webserver-unit-tests.md`
