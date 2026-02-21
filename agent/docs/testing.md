# Specialist Community Runbook

Purpose:
- provide one concise workflow for community webserver + auth validation using demo fixtures.

Use this runbook when touching:
- community auth handshake,
- webserver API/router/auth logic,
- client/server community integration behavior,
- demo community/user/world fixture flows.

## Fixture Roots (Canonical)
- communities: `demo/communities/*`
- users: `demo/users/*`
- worlds: `demo/worlds/*`

Baseline triad:
- `demo/communities/r55man`
- `demo/worlds/r55man-1/config.json`
- `demo/users/r55man/.config/bz3/config.json`

## Precondition
From target repo root:
- `./abuild.py -c -d <build-dir>`

## Canonical End-to-End Auth Flow (3 terminals)
1. Community webserver:
- `./src/webserver/bin/start.py demo/communities/r55man`

2. Game server:
- `./<build-dir>/bz3-server --server-config demo/worlds/r55man-1/config.json --trace net.server,engine.server`

3. Client connect:
- `timeout 15s ./<build-dir>/bz3 --user-config demo/users/r55man/.config/bz3/config.json --server 192.168.1.6:11899 --trace net.client,engine.app`

Expected evidence:
- server heartbeat trace lines,
- successful pre-auth/join trace lines,
- community API auth request success in logs.

## Core Verification Commands
- API log tail:
  - `tail -n 30 demo/communities/r55man/logs/api.log`
- DB heartbeat freshness check (use local sqlite query) to confirm active heartbeat updates.

## Webserver Unit/String Gate (when webserver scope touched)
- `python3 ./src/webserver/tests/validate_strings.py --all`
- `python3 -m unittest discover -s src/webserver/tests/unit -p "test_*.py"`

## Common Pitfalls
- Heartbeat endpoint may be absent from API log depending on logging config; verify via server trace + DB timestamps.
- Credential key selection depends on exact `--server` endpoint key in user config.
- Rejected auth often indicates wrong credential form or endpoint mismatch.

## Negative Test (Optional)
Force wrong credentials and verify reject path + evidence logging.

## Handoff Evidence Minimum
- commands used,
- pass/fail result per command,
- relevant trace/log snippets summarized,
- failures + rerun outcomes.
