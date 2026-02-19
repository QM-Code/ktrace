# Community Auth Demo Testing

This document is the canonical method for validating community-backed authentication using demo fixtures:
- community webserver state from `demo/communities/*`,
- server world overlays from `demo/worlds/*`,
- virtual client users from `demo/users/*`.

## Scope
Use this workflow when changing:
- community auth handshake paths (`src/engine/network/server_preauth.cpp`, server runtime wiring),
- client credential resolution/auth payload generation,
- demo community/world/user fixture behavior for auth.

## Preconditions
From `m-rewrite/`:

```bash
./abuild.py -c -d build-dev
```

Policy reminders:
- Use demo fixture roots (not personal `~/.config/bz3`).
- Use long-form CLI flags (short flags are intentionally removed except `-h`).

## Fixture Mapping
- Community: `demo/communities/r55man`
- Server world overlay: `demo/worlds/r55man-1/config.json`
- Virtual user config: `demo/users/r55man/.config/bz3/config.json`

This triad is the baseline, because:
- world overlay already enables community heartbeat,
- the user config already contains `communityCredentials` for the r55man community endpoint,
- the community DB already contains the server/host registration needed for heartbeat and auth.

Quick alignment check (recommended before running):

```bash
python3 - <<'PY'
import json, sqlite3
world = json.load(open('demo/worlds/r55man-1/config.json', encoding='utf-8'))
user = json.load(open('demo/users/r55man/.config/bz3/config.json', encoding='utf-8'))
print('world community.server =', world.get('community', {}).get('server'))
print('user credential keys   =', list((user.get('communityCredentials') or {}).keys()))
conn = sqlite3.connect('demo/communities/r55man/data/karma.db')
rows = conn.execute("select host,port,name from servers where host='192.168.1.6' and port=11899").fetchall()
print('registered host:port   =', rows)
conn.close()
PY
```

## Canonical Flow
Use three terminals.

1. Start community webserver:

```bash
./src/webserver/bin/start.py demo/communities/r55man
```

Expected:
- `Community server listening on http://0.0.0.0:8080`

2. Start game server with world overlay and trace:

```bash
./build-dev/bz3-server \
  --server-config demo/worlds/r55man-1/config.json \
  --trace net.server,engine.server
```

Expected trace signals:
- `Community heartbeat enabled: target='http://...:8080/' ...`
- recurring `HeartbeatClient: sent heartbeat to http://...:8080`

3. Start client with virtual user config and connect:

```bash
timeout 15s ./build-dev/bz3 \
  --user-config demo/users/r55man/.config/bz3/config.json \
  --server 192.168.1.6:11899 \
  --trace net.client,engine.app
```

Expected client signals:
- `ClientConnection: connected to ...`
- `ClientConnection: sent join request ... auth_payload_present=1`
- `ClientConnection: join accepted by ...`

Expected server signals:
- `Community pre-auth accepted client_id=... username='r55man'`
- session connect mapping lines for that user.

## Evidence and Verification
Collect all three:

1. Server trace evidence:
- heartbeat send lines (`HeartbeatClient: sent heartbeat ...`)
- pre-auth result (`Community pre-auth accepted ...` or rejection reason)

2. Community API log evidence:

```bash
tail -n 30 demo/communities/r55man/logs/api.log
```

Expected for successful auth run:
- `POST /api/auth` with `200`.

3. Community DB heartbeat freshness (because `/api/heartbeat` is intentionally excluded from `api.log` by default):

```bash
python3 - <<'PY'
import sqlite3, time
conn = sqlite3.connect('demo/communities/r55man/data/karma.db')
conn.row_factory = sqlite3.Row
row = conn.execute(
    'select host,port,last_heartbeat,updated_at,num_players,max_players from servers where host=? and port=?',
    ('192.168.1.6', 11899),
).fetchone()
print(dict(row) if row else None)
print('now', int(time.time()))
conn.close()
PY
```

Pass condition:
- `last_heartbeat` near current time and updating while server is running.

## Common Pitfalls
1. Heartbeat not visible in `api.log`:
- Not a failure by itself.
- Base webserver config routes `/api/heartbeat` to `file: null`.
- Verify via server trace + DB `last_heartbeat`.

2. Wrong credential entry selected:
- Client credential lookup depends on `--server` endpoint matching keys in `communityCredentials`.
- `localhost:...` can pick `LAN` credentials.
- `192.168.1.6:...` can pick community-specific entry (with `passwordHash`).

3. Auth rejected with `username/email and password are required`:
- Community `/api/auth` in normal mode (`debug.auth=false`) requires `passhash`.
- If client sends plaintext-only payload, this can fail.
- Use matching credential endpoint or provide explicit credentials that resolve to a valid hash path.

4. Heartbeat rejected (`host_not_found` / `port_mismatch`):
- Community DB must contain the server `host:port` registration used by `network.ServerAdvertiseHost` and listen port.

## Optional Negative Test
Deliberately force a reject:

```bash
timeout 12s ./build-dev/bz3 \
  --user-config demo/users/r55man/.config/bz3/config.json \
  --server localhost:11899 \
  --username r55man \
  --password wrong-password \
  --trace net.client,engine.app
```

Expected:
- server join rejected,
- `POST /api/auth` non-200 in `api.log`,
- server trace includes community pre-auth rejection reason.

## Teardown
Stop long-running processes (`Ctrl+C`) for both:
- `bz3-server`
- `start.py` webserver

Optional process sanity check:

```bash
ps -ef | rg -n '(bz3-server|src/webserver/bin/start.py|waitress)' | rg -v rg || true
```

## Related Docs
- `docs/foundation/governance/community-webserver-testing.md`
- `docs/foundation/governance/testing-ci-governance.md`
- `docs/foundation/policy/execution-policy.md`
