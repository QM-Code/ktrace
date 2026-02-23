# Testing Policy

This document mostly contains an overview of where testing data lies and how to use it.
Individual sessions will use their own specific test criteria.

## Tested branches

- Only two branches get built and tested, m-karma and m-bz3.
- These will be referred to generically as <branch> throughout this document.

## Constraints

- Never attempt to read data outside the project root.
- Never attempt to use system home config directories (e.g. ~/.config/xyz) for data.

## Testing Data

- Always use "virtual users" under <root>/<branch>/demo/users/* 

- Testing data is stored in the <branch>/demo/ directory.
- Each of m-karma and m-bz3 have their own demo directories.
- Demo directories may be used to create reusable sample data.
- The following demo directories are defined:
  - `<branch>/demo/communities/*`
  - `<branch>/demo/users/*`
  - `<branch>/demo/worlds/*`
- Each subdirectory of `demo` will have its own set of subdirectories, each of which represent one community, user, or world.

## Canonical End-to-End Testing

- This should never need to be run during normal sessions
- It is here as a sort of 'proof that the basics work' kind of thing, since it touches most subsystems.
- It is not a guarantee that everything is actually working correctly, as game logic and rendering could be a mess and it would not be detected by this testing.
- Nevertheless, the option to perform a full clean test build and perform the following "end-to-end test" should be presented to the user during the initial greeting prompt.

### End-to-End Testing Procedure:

1. Community webserver:
- m-karma: `./src/webserver/bin/start.py demo/communities/<community>`
- m-bz3: `../m-karma/src/webserver/bin/start.py demo/communities/<community>`

2. Game server:
- m-karma: `./<build-dir>/server --server-config demo/worlds/<world>/config.json --trace net.server,engine.server`
- m-bz3: `./<build-dir>/bz3-server --server-config demo/worlds/<world>/config.json --trace net.server,engine.server`

3. Client connect:
- m-karma: `timeout 15s ./<build-dir>/client --user-config demo/users/<user>/.config/bz3/config.json --server <host>:<port> --trace net.client,engine.app`
- m-bz3: `timeout 15s ./<build-dir>/bz3 --user-config demo/users/<user>/.config/bz3/config.json --server <host>:<port> --trace net.client,engine.app`

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
