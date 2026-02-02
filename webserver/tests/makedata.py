#!/usr/bin/env python3
import os
import random
import sys


ADJECTIVES = [
    "Apex",
    "Amber",
    "Azure",
    "Blaze",
    "Crimson",
    "Drift",
    "Echo",
    "Frost",
    "Gale",
    "Halo",
    "Ion",
    "Jade",
    "Keen",
    "Lumen",
    "Magma",
    "Nimbus",
    "Onyx",
    "Pyre",
    "Quill",
    "Rift",
    "Solar",
    "Tempest",
    "Umber",
    "Vapor",
    "Warden",
    "Xenon",
    "Yarrow",
    "Zephyr",
]

NOUNS = [
    "Arc",
    "Bay",
    "Bluff",
    "Circuit",
    "Cove",
    "Delta",
    "Dock",
    "Field",
    "Forge",
    "Gate",
    "Harbor",
    "Hollow",
    "Isle",
    "Jetty",
    "Keep",
    "Lagoon",
    "Mesa",
    "Pass",
    "Quarry",
    "Reach",
    "Spire",
    "Trace",
    "Valley",
    "Ward",
    "Yard",
]


def _usage():
    return "usage: makedata.py <community-directory> -s <server-num> -u <user-num>"


def _parse_args():
    args = sys.argv[1:]
    if not args:
        raise SystemExit(_usage())
    directory = args.pop(0)
    server_num = None
    user_num = None
    while args:
        token = args.pop(0)
        if token == "-s":
            if not args:
                raise SystemExit(_usage())
            server_num = args.pop(0)
        elif token == "-u":
            if not args:
                raise SystemExit(_usage())
            user_num = args.pop(0)
        else:
            raise SystemExit(_usage())
    if server_num is None or user_num is None:
        raise SystemExit("-s <server-num> and -u <user-num> are mandatory")
    try:
        server_num = int(server_num)
        user_num = int(user_num)
    except ValueError:
        raise SystemExit("Counts must be integers.")
    if server_num < 0 or user_num < 0:
        raise SystemExit("Counts must be non-negative.")
    return directory, server_num, user_num


def _random_name(existing, prefix=""):
    while True:
        name = f"{random.choice(ADJECTIVES)}{random.choice(NOUNS)}"
        if prefix:
            name = f"{prefix}{name}"
        if name.lower() not in existing:
            existing.add(name.lower())
            return name


def _random_title(existing):
    while True:
        title = f"{random.choice(ADJECTIVES)} {random.choice(NOUNS)}"
        if title.lower() not in existing:
            existing.add(title.lower())
            return title


def _random_description():
    phrases = [
        "Fast lanes with risky cutbacks.",
        "Wide sightlines with tight choke points.",
        "Layered ramps with exposed midlines.",
        "Compact arena with aggressive flanks.",
        "High ground duels and quick resets.",
        "Open basin with narrow bridges.",
    ]
    return random.choice(phrases)


def _random_host(used_pairs):
    while True:
        host = f"10.{random.randint(0, 255)}.{random.randint(0, 255)}.{random.randint(1, 254)}"
        port = random.randint(5200, 6200)
        key = (host, port)
        if key not in used_pairs:
            used_pairs.add(key)
            return host, port


def main():
    directory, server_num, user_num = _parse_args()

    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)

    from karma import cli, auth, config, db

    cli.bootstrap(directory, _usage())
    db_path = db.default_db_path()
    if not os.path.exists(db_path):
        raise SystemExit("Database does not exist.")

    conn = db.connect(db_path)
    try:
        existing_users = {row["username"].lower() for row in db.list_users(conn)}
        existing_servers = {row["name"].lower() for row in db.list_servers(conn)}
        used_pairs = {(row["host"], row["port"]) for row in db.list_servers(conn)}

        new_user_ids = []
        for _ in range(user_num):
            username = _random_name(existing_users)
            email = f"{username.lower()}@karma.test"
            password = f"{username}#1"
            digest, salt = auth.new_password(password)
            db.add_user(conn, username, email, digest, salt)
            user_row = db.get_user_by_username(conn, username)
            if user_row:
                new_user_ids.append(user_row["id"])

        all_users = db.list_users(conn)
        user_ids = [row["id"] for row in all_users if not row["deleted"]]
        if not user_ids:
            raise SystemExit("No users available to own servers.")

        settings = config.get_config()
        overview_max = int(config.require_setting(settings, "pages.servers.overview_max_chars"))
        for _ in range(server_num):
            name = _random_title(existing_servers)
            host, port = _random_host(used_pairs)
            owner_user_id = random.choice(user_ids)
            overview = _random_description()
            if len(overview) > overview_max:
                overview = overview[:overview_max]
            record = {
                "name": name,
                "overview": overview,
                "description": _random_description(),
                "host": host,
                "port": port,
                "owner_user_id": owner_user_id,
                "screenshot_id": None,
                "max_players": None,
                "num_players": None,
                "last_heartbeat": None,
            }
            db.add_server(conn, record)
    finally:
        conn.close()

    print(f"Added {user_num} users and {server_num} servers.")


if __name__ == "__main__":
    main()
