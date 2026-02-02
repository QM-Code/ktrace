#!/usr/bin/env python3
import getpass
import hashlib
import json
import os
import secrets
import sys


def _ensure_community_dir(path, messages):
    if not path:
        raise SystemExit("usage: initialize.py <community-directory>")
    if not os.path.isdir(path):
        prompt = f"{messages['create_dir_prefix']} {path} {messages['create_dir_suffix']}"
        answer = input(f"{prompt} ").strip().lower()
        if answer not in ("", "y", "yes"):
            raise SystemExit(messages["cancelled"])
        os.makedirs(path, exist_ok=True)
        return True
    if os.listdir(path):
        raise SystemExit(messages["dir_not_empty"])
    return False


def _cleanup_paths(paths):
    for path in reversed(paths):
        if os.path.isdir(path):
            try:
                os.rmdir(path)
            except OSError:
                pass
        else:
            try:
                os.remove(path)
            except OSError:
                pass


def _ensure_dir(path):
    if os.path.isdir(path):
        return False
    os.makedirs(path, exist_ok=True)
    return True


def _prompt_text(label, default_value):
    response = input(f"{label} [{default_value}]: ").strip()
    return response or default_value


def _format(template, **values):
    text = str(template or "")
    for key, value in values.items():
        text = text.replace(f"{{{key}}}", str(value))
    return text


def _require_message(messages, key):
    value = messages.get(key)
    if not isinstance(value, str) or not value:
        raise SystemExit(f"Missing scripts.initialize.{key} in strings/en.json.")
    return value


def _load_init_messages(settings):
    init_strings = (settings.get("scripts") or {}).get("initialize", {})
    keys = [
        "create_dir_prefix",
        "create_dir_suffix",
        "cancelled",
        "init_complete",
        "dir_not_empty",
        "language_prompt",
        "language_tab_hint",
        "language_list_header",
        "language_list_item",
        "language_invalid",
        "language_codes_header",
        "language_code_item",
        "community_name",
        "default_community_name",
        "port",
        "admin_username",
        "default_admin_user",
        "admin_password",
        "port_invalid_integer",
        "port_invalid_range",
        "password_required",
    ]
    return {key: _require_message(init_strings, key) for key in keys}


def _prompt_port(label, default_value, messages):
    while True:
        response = input(f"{label} [{default_value}]: ").strip()
        if not response:
            return default_value
        try:
            port = int(response)
        except ValueError:
            print(messages["port_invalid_integer"])
            continue
        if 1 <= port <= 65535:
            return port
        print(messages["port_invalid_range"])


def _prompt_language(default_language, available_languages, labels, messages):
    prompt_text = _format(messages["language_prompt"], default=default_language)
    tab_hint = messages.get("language_tab_hint")
    if tab_hint:
        prompt_text = f"{prompt_text} {tab_hint}"
    while True:
        response = input(f"{prompt_text} ").strip()
        if not response:
            return default_language
        if response == "?":
            print(messages["language_list_header"])
            for code in available_languages:
                label = labels.get(code, code)
                print(_format(messages["language_list_item"], code=code, label=label))
            continue
        language = response.strip().lower()
        if len(language) != 2 or not language.isalpha():
            print(messages["language_invalid"])
            print(messages["language_codes_header"])
            for code in available_languages:
                label = labels.get(code, code)
                print(_format(messages["language_code_item"], code=code, label=label))
            continue
        if language not in available_languages:
            print(messages["language_invalid"])
            print(messages["language_codes_header"])
            for code in available_languages:
                label = labels.get(code, code)
                print(_format(messages["language_code_item"], code=code, label=label))
            continue
        return language


def main():
    if len(sys.argv) != 2:
        raise SystemExit("usage: initialize.py <community-directory>")
    directory = sys.argv[1]

    created_paths = []
    try:
        root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
        if root_dir not in sys.path:
            sys.path.insert(0, root_dir)
        config_path = os.path.join(root_dir, "config.json")
        if not os.path.isfile(config_path):
            raise SystemExit(f"Missing config.json at {config_path}")

        with open(config_path, "r", encoding="utf-8") as handle:
            config = json.load(handle)

        from karma import config as config_lib

        if "server" not in config:
            raise SystemExit("Missing server in config.json. Add it to config.json.")
        if "port" not in config.get("server", {}):
            raise SystemExit("Missing server.port in config.json. Add it to config.json.")
        if "language" not in config.get("server", {}):
            raise SystemExit("Missing server.language in config.json. Add it to config.json.")
        if "database" not in config:
            raise SystemExit("Missing database in config.json. Add it to config.json.")
        if "database_directory" not in config.get("database", {}):
            raise SystemExit("Missing database.database_directory in config.json. Add it to config.json.")
        if "database_file" not in config.get("database", {}):
            raise SystemExit("Missing database.database_file in config.json. Add it to config.json.")
        if "uploads" not in config:
            raise SystemExit("Missing uploads in config.json. Add it to config.json.")
        if "upload_directory" not in config.get("uploads", {}):
            raise SystemExit("Missing uploads.upload_directory in config.json. Add it to config.json.")

        server_defaults = config.get("server", {})
        base_language = config_lib.normalize_language(server_defaults.get("language") or "en")
        default_settings = config_lib.get_config(language=base_language)
        messages = _load_init_messages(default_settings)

        available_languages = config_lib.get_available_languages()
        language_labels = default_settings.get("languages", {})
        language = _prompt_language(base_language, available_languages, language_labels, messages)

        settings = config_lib.get_config(language=language)
        messages = _load_init_messages(settings)

        created_dir = _ensure_community_dir(directory, messages)
        if created_dir:
            created_paths.append(directory)

        community_name = _prompt_text(messages["community_name"], messages["default_community_name"])
        server_port = _prompt_port(messages["port"], int(server_defaults.get("port")), messages)
        admin_user = _prompt_text(messages["admin_username"], messages["default_admin_user"])

        try:
            admin_password = getpass.getpass(f"{messages['admin_password']}: ")
        except KeyboardInterrupt:
            raise
        if not admin_password:
            print(messages["password_required"])
            while True:
                try:
                    admin_password = getpass.getpass(f"{messages['admin_password']}: ")
                except KeyboardInterrupt:
                    raise
                if admin_password:
                    break
                print(messages["password_required"])

        data_dir = config.get("database", {}).get("database_directory")
        uploads_dir = config.get("uploads", {}).get("upload_directory")
        if not os.path.isabs(data_dir):
            data_dir = os.path.normpath(os.path.join(directory, data_dir))
        if not os.path.isabs(uploads_dir):
            uploads_dir = os.path.normpath(os.path.join(directory, uploads_dir))
        if _ensure_dir(data_dir):
            created_paths.append(data_dir)
        if _ensure_dir(uploads_dir):
            created_paths.append(uploads_dir)

        salt = secrets.token_hex(16)
        password_hash = hashlib.pbkdf2_hmac(
            "sha256", admin_password.encode("utf-8"), salt.encode("utf-8"), 100_000
        ).hex()
        session_secret = secrets.token_hex(32)

        community_config = {
            "server": {
                "community_name": community_name,
                "language": language,
                "port": server_port,
                "admin_user": admin_user,
                "session_secret": session_secret,
            }
        }
        community_config_path = os.path.join(directory, "config.json")
        with open(community_config_path, "w", encoding="utf-8") as handle:
            json.dump(community_config, handle, indent=2)
            handle.write("\n")
        created_paths.append(community_config_path)

        from karma import cli
        from karma import db

        cli.bootstrap(directory, "usage: initialize.py <community-directory>")
        db.init_db(db.default_db_path())
        conn = db.connect(db.default_db_path())
        try:
            admin_row = db.get_user_by_username(conn, admin_user)
            if admin_row:
                db.set_user_password(conn, admin_row["id"], password_hash, salt)
                db.set_user_admin(conn, admin_row["id"], True)
            else:
                admin_email = f"{admin_user.lower()}@local"
                db.add_user(conn, admin_user, admin_email, password_hash, salt, is_admin=True)
        finally:
            conn.close()

        print(_format(messages["init_complete"], directory=directory, config_path=community_config_path))
    except KeyboardInterrupt:
        _cleanup_paths(created_paths)
        print()
        print(messages["cancelled"])


if __name__ == "__main__":
    main()
