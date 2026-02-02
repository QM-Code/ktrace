#!/usr/bin/env python3
import os
import sys


def _require_message(messages, key):
    value = messages.get(key)
    if not isinstance(value, str) or not value:
        raise SystemExit(f"Missing scripts.db_restore.{key} in strings/en.json.")
    return value


def _load_messages(settings):
    script_strings = (settings.get("scripts") or {}).get("db_restore", {})
    keys = [
        "usage",
        "json_required",
        "db_exists",
        "imported_users",
        "imported_servers",
    ]
    return {key: _require_message(script_strings, key) for key in keys}


def _parse_args(messages):
    directory = ""
    json_path = ""
    args = sys.argv[1:]
    if not args:
        raise SystemExit(messages["usage"])
    if args:
        directory = args[0]
        args = args[1:]
    while args:
        token = args.pop(0)
        if token == "-f":
            if not args:
                raise SystemExit(messages["json_required"])
            json_path = args.pop(0)
        else:
            raise SystemExit(messages["usage"])
    if not json_path:
        raise SystemExit(messages["json_required"])
    return directory, json_path


def main():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)

    from karma import cli, config, db
    from karma.tools import import_data as tool

    base_language = config.normalize_language(
        (config.get_base_config().get("server") or {}).get("language") or "en"
    )
    base_settings = config.get_config(language=base_language)
    messages = _load_messages(base_settings)
    directory, json_path = _parse_args(messages)

    cli.bootstrap(directory, messages["usage"])
    messages = _load_messages(config.get_config())
    db_path = db.default_db_path()
    if os.path.exists(db_path):
        raise SystemExit(messages["db_exists"])

    path = os.path.abspath(json_path)
    cli.validate_json_file(path)
    users_ok, users_skip, servers_ok, servers_skip = tool.import_data(
        path, allow_merge=False, overwrite=False
    )
    print(messages["imported_users"].format(count=users_ok, skipped=users_skip))
    print(messages["imported_servers"].format(count=servers_ok, skipped=servers_skip))


if __name__ == "__main__":
    main()
