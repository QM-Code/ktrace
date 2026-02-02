#!/usr/bin/env python3
import os
import random
import sys
import time
import json
import urllib.parse
import urllib.request


def _usage():
    return "usage: heartbeat.py <community-directory> [-w <seconds>]"


def _parse_args():
    args = sys.argv[1:]
    if not args:
        raise SystemExit(_usage())
    directory = args.pop(0)
    wait_seconds = 1.0
    while args:
        token = args.pop(0)
        if token == "-w":
            if not args:
                raise SystemExit(_usage())
            try:
                wait_seconds = float(args.pop(0))
            except ValueError:
                raise SystemExit("Wait time must be a number.")
            if wait_seconds < 0:
                raise SystemExit("Wait time must be non-negative.")
        else:
            raise SystemExit(_usage())
    return directory, wait_seconds


def _report_error(body):
    if not body:
        return
    try:
        payload = json.loads(body)
    except Exception:
        print(f"Details: {body.strip()}")
        return
    if isinstance(payload, dict):
        message = payload.get("message")
        error = payload.get("error")
        if message or error:
            detail = message or error
            print(f"Details: {detail}")
            return
    print(f"Details: {body.strip()}")


def main():
    directory, wait_seconds = _parse_args()

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)

    from karma import cli, config, db

    cli.bootstrap(directory, _usage())
    settings = config.get_config()
    server_settings = settings.get("server", {})
    host = server_settings.get("host")
    port = server_settings.get("port")
    if port is None:
        raise SystemExit("No server.port specified in config.json.")
    port = int(port)
    endpoint = f"http://{host}:{port}/api/heartbeat"

    with db.connect_ctx() as conn:
        servers = db.list_servers(conn)

    if not servers:
        print("No servers to heartbeat.")
        return

    random.shuffle(servers)
    selected = servers[: max(1, len(servers) // 2)]

    successes = 0
    failures = 0
    try:
        while True:
            for server in selected:
                max_players = random.randint(10, 32)
                num_players = random.randint(0, max_players)
                payload = {
                    "server": f"{server['host']}:{server['port']}",
                    "players": str(num_players),
                    "max": str(max_players),
                }
                print(f"Heartbeat -> {payload['server']} players={payload['players']} max={payload['max']}")
                query = urllib.parse.urlencode(payload)
                request = urllib.request.Request(f"{endpoint}?{query}", method="GET")
                try:
                    with urllib.request.urlopen(request, timeout=5) as response:
                        body = response.read().decode("utf-8", errors="replace")
                        if 200 <= response.status < 300:
                            successes += 1
                        else:
                            failures += 1
                            print(f"Heartbeat failed for {server['host']}:{server['port']}: HTTP {response.status}")
                            _report_error(body)
                except urllib.error.HTTPError as exc:
                    failures += 1
                    try:
                        body = exc.read().decode("utf-8", errors="replace")
                    except Exception:
                        body = ""
                    print(f"Heartbeat failed for {server['host']}:{server['port']}: HTTP {exc.code}")
                    _report_error(body)
                except Exception:
                    failures += 1
                    print(f"Heartbeat failed to connect to server {server['host']}:{server['port']}.")
                time.sleep(wait_seconds)
    except KeyboardInterrupt:
        print()
        print(f"Stopped after {successes} ok, {failures} failed.")


if __name__ == "__main__":
    main()
