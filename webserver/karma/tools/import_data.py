import argparse
import json
import os
import time

from karma import auth, config, db


def _load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _normalize(value):
    return str(value).strip() if value is not None else ""


def _parse_int(value):
    if value is None or value == "":
        return None
    try:
        return int(str(value))
    except ValueError:
        return None


def _admin_entry(admin_entry):
    if isinstance(admin_entry, str):
        return _normalize(admin_entry), False
    if isinstance(admin_entry, dict):
        return _normalize(admin_entry.get("username")), bool(admin_entry.get("trust_admins"))
    return "", False


def _resolve_existing_user(conn, username, email):
    user = db.get_user_by_username(conn, username) if username else None
    email_user = db.get_user_by_email(conn, email) if email else None
    if user and email_user and user["id"] != email_user["id"]:
        return None
    return user or email_user


def import_data(path, allow_merge=False, overwrite=False):
    payload = _load_json(path)
    users = payload.get("users", [])
    servers = payload.get("servers", [])

    if not isinstance(users, list) or not isinstance(servers, list):
        raise SystemExit("Expected 'users' and 'servers' arrays in the JSON payload.")

    db_path = db.default_db_path()
    db_exists = os.path.exists(db_path)
    if db_exists and not allow_merge and not overwrite:
        raise SystemExit(
            "Database already exists. Use --merge to merge into the existing database, "
            "--overwrite to replace it, or delete it first."
        )
    if overwrite and db_exists:
        os.remove(db_path)
    db.init_db(db_path)
    inserted_users = 0
    skipped_users = 0
    inserted_servers = 0
    skipped_servers = 0
    with db.connect_ctx(db_path) as conn:
        admin_username = config.require_setting(config.get_config(), "server.admin_user")
        admin_names = set()
        for entry in users:
            if not isinstance(entry, dict):
                skipped_users += 1
                continue
            username = _normalize(entry.get("username"))
            email = _normalize(entry.get("email")).lower()
            password = _normalize(entry.get("password"))
            password_hash = _normalize(entry.get("password_hash"))
            password_salt = _normalize(entry.get("password_salt"))
            is_admin = bool(entry.get("is_admin_manual") or entry.get("is_admin"))
            is_locked = bool(entry.get("is_locked"))
            is_deleted = bool(entry.get("deleted"))
            locked_at = _normalize(entry.get("locked_at")) or None
            deleted_at = _normalize(entry.get("deleted_at")) or None
            language = _normalize(entry.get("language")) or None
            if not username or not email:
                skipped_users += 1
                continue
            if not password and (not password_hash or not password_salt):
                skipped_users += 1
                continue
            if " " in username:
                skipped_users += 1
                continue
            existing_user = _resolve_existing_user(conn, username, email)
            if existing_user:
                user_id = existing_user["id"]
                if password_hash and password_salt:
                    db.set_user_password(conn, user_id, password_hash, password_salt)
                elif password:
                    digest, salt = auth.new_password(password)
                    db.set_user_password(conn, user_id, digest, salt)
                db.set_user_locked(conn, user_id, is_locked, locked_at=locked_at)
                db.set_user_deleted(conn, user_id, is_deleted, deleted_at=deleted_at)
                if language is not None:
                    db.update_user_language(conn, user_id, language)
                if is_admin and username.lower() != admin_username.lower():
                    admin_names.add(username.lower())
                inserted_users += 1
                continue
            if db.get_user_by_username(conn, username) or db.get_user_by_email(conn, email):
                skipped_users += 1
                continue
            if password_hash and password_salt:
                digest = password_hash
                salt = password_salt
            else:
                digest, salt = auth.new_password(password)
            code = entry.get("code")
            if code is not None:
                code = str(code).strip() or None
            db.add_user(
                conn,
                username,
                email,
                digest,
                salt,
                is_admin=False,
                is_admin_manual=False,
                language=language,
                code=code,
            )
            user_row = db.get_user_by_username(conn, username)
            if user_row:
                db.set_user_locked(conn, user_row["id"], is_locked, locked_at=locked_at)
                db.set_user_deleted(conn, user_row["id"], is_deleted, deleted_at=deleted_at)
            if is_admin and username.lower() != admin_username.lower():
                admin_names.add(username.lower())
            inserted_users += 1

        user_rows = conn.execute("SELECT id, username, code, deleted FROM users").fetchall()
        user_ids = {
            row["username"].lower(): row["id"]
            for row in user_rows
            if not row["deleted"]
        }
        user_codes = {
            (row["code"] or "").lower(): row["id"]
            for row in user_rows
            if row["code"] and not row["deleted"]
        }

        for entry in users:
            if not isinstance(entry, dict):
                continue
            owner_id = user_ids.get(_normalize(entry.get("username")).lower())
            if not owner_id:
                continue
            admins = entry.get("admin_list") or []
            if not isinstance(admins, list):
                continue
            for admin_entry in admins:
                admin_name, trust_admins = _admin_entry(admin_entry)
                admin_id = user_ids.get(admin_name.lower())
                if not admin_id or admin_id == owner_id:
                    continue
                db.add_user_admin(conn, owner_id, admin_id, trust_admins=trust_admins)

        root_user = db.get_user_by_username(conn, admin_username)
        if root_user:
            for admin_name in sorted(admin_names):
                admin_id = user_ids.get(admin_name)
                if admin_id:
                    db.add_user_admin(conn, root_user["id"], admin_id)

        for entry in servers:
            if not isinstance(entry, dict):
                skipped_servers += 1
                continue
            name = _normalize(entry.get("name"))
            host = _normalize(entry.get("host"))
            port = _parse_int(entry.get("port"))
            if not name or not host or port is None:
                skipped_servers += 1
                continue
            owner = _normalize(entry.get("owner"))
            owner_key = owner.lower() if owner else ""
            owner_code = _normalize(entry.get("owner_code"))
            owner_code_key = owner_code.lower() if owner_code else ""
            owner_id = user_codes.get(owner_code_key) if owner_code else None
            if owner_id is None:
                owner_id = user_ids.get(owner_key) if owner else None
            if owner_id is None:
                owner_id = user_ids.get(admin_username.lower())
            if owner_id is None:
                skipped_servers += 1
                continue
            if db.get_server_by_name(conn, name):
                skipped_servers += 1
                continue
            code = entry.get("code")
            if code is not None:
                code = str(code).strip() or None
            record = {
                "name": name,
                "code": code,
                "overview": entry.get("overview") or None,
                "description": entry.get("description") or None,
                "host": host,
                "port": port,
                "owner_user_id": owner_id,
                "screenshot_id": entry.get("screenshot_id"),
            }
            db.add_server(conn, record)
            inserted_servers += 1
        if root_user:
            db.recompute_admin_flags(conn, root_user["id"])

    return inserted_users, skipped_users, inserted_servers, skipped_servers


def main():
    parser = argparse.ArgumentParser(description="Import users and servers into the karma database.")
    parser.add_argument("path", help="Path to JSON file (e.g. test-data.json)")
    parser.add_argument(
        "-m",
        "--merge",
        action="store_true",
        help="Merge into an existing database (default is to abort if database exists).",
    )
    parser.add_argument(
        "-o",
        "--overwrite",
        action="store_true",
        help="Overwrite the existing database before importing.",
    )
    args = parser.parse_args()
    path = os.path.abspath(args.path)
    users_ok, users_skip, servers_ok, servers_skip = import_data(
        path, allow_merge=args.merge, overwrite=args.overwrite
    )
    print(f"Imported {users_ok} users; skipped {users_skip}.")
    print(f"Imported {servers_ok} servers; skipped {servers_skip}.")


if __name__ == "__main__":
    main()
