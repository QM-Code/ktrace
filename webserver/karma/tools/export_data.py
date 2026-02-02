import argparse
import json
import os

from karma import db


def export_data():
    with db.connect_ctx() as conn:
        users = conn.execute(
            """
            SELECT id,
                   username,
                   email,
                   language,
                   code,
                   password_hash,
                   password_salt,
                   is_admin,
                   is_locked,
                   locked_at,
                   deleted,
                   deleted_at
              FROM users
             ORDER BY LOWER(username)
            """
        ).fetchall()
        admin_rows = conn.execute(
            """
            SELECT user_admins.owner_user_id,
                   user_admins.trust_admins,
                   users.username AS admin_username
              FROM user_admins
              JOIN users ON users.id = user_admins.admin_user_id
             ORDER BY LOWER(users.username)
            """
        ).fetchall()
        admin_lookup = {}
        for row in admin_rows:
            owner_id = row["owner_user_id"]
            admin_lookup.setdefault(owner_id, []).append(
                {
                    "username": row["admin_username"],
                    "trust_admins": bool(row["trust_admins"]),
                }
            )

        user_payload = []
        for user in users:
            entry = {
                "username": user["username"],
                "email": user["email"],
                "language": user["language"],
                "code": user["code"],
                "password_hash": user["password_hash"],
                "password_salt": user["password_salt"],
                "is_admin": bool(user["is_admin"]),
                "is_locked": bool(user["is_locked"]),
                "locked_at": user["locked_at"],
                "deleted": bool(user["deleted"]),
                "deleted_at": user["deleted_at"],
            }
            admins = admin_lookup.get(user["id"])
            if admins:
                entry["admin_list"] = admins
            user_payload.append(entry)

        servers = conn.execute(
            """
            SELECT servers.name,
                   servers.code,
                   servers.overview,
                   servers.description,
                   servers.host,
                   servers.port,
                   users.username AS owner_username,
                   users.code AS owner_code,
                   servers.screenshot_id
              FROM servers
              JOIN users ON users.id = servers.owner_user_id
             ORDER BY LOWER(servers.name)
            """
        ).fetchall()
        server_payload = []
        for server in servers:
            entry = {
                "name": server["name"],
                "code": server["code"],
                "overview": server["overview"],
                "description": server["description"],
                "host": server["host"],
                "port": server["port"],
                "owner": server["owner_username"],
                "owner_code": server["owner_code"],
                "screenshot_id": server["screenshot_id"],
            }
            server_payload.append(entry)

        return {"users": user_payload, "servers": server_payload}


def main():
    parser = argparse.ArgumentParser(description="Export users and servers from the karma database.")
    parser.add_argument("path", help="Path to JSON file to write (use - for stdout)")
    args = parser.parse_args()
    payload = export_data()
    output = json.dumps(payload, indent=2)
    if args.path == "-":
        print(output)
        return
    path = os.path.abspath(args.path)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(output + "\n")


if __name__ == "__main__":
    main()
