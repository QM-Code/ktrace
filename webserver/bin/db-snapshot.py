#!/usr/bin/env python3
import json
import os
import sys
import zipfile
from datetime import datetime


def _require_message(messages, key):
    value = messages.get(key)
    if not isinstance(value, str) or not value:
        raise SystemExit(f"Missing scripts.db_snapshot.{key} in strings/en.json.")
    return value


def _load_messages(settings):
    script_strings = (settings.get("scripts") or {}).get("db_snapshot", {})
    keys = [
        "usage",
        "exporting",
        "wrote_zip",
    ]
    return {key: _require_message(script_strings, key) for key in keys}


def _parse_args(messages):
    args = sys.argv[1:]
    if not args:
        raise SystemExit(messages["usage"])
    directory = args.pop(0)
    output_path = None
    zip_output = False
    while args:
        token = args.pop(0)
        if token == "-z":
            zip_output = True
        elif output_path is None:
            output_path = token
        else:
            raise SystemExit(messages["usage"])
    return directory, output_path, zip_output


def main():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)

    from karma import cli, config
    from karma import db
    from karma.tools import export_data as tool

    base_language = config.normalize_language(
        (config.get_base_config().get("server") or {}).get("language") or "en"
    )
    base_settings = config.get_config(language=base_language)
    messages = _load_messages(base_settings)
    directory, output_path, zip_output = _parse_args(messages)

    cli.bootstrap(directory, messages["usage"])
    messages = _load_messages(config.get_config())

    payload = tool.export_data()
    output = json.dumps(payload, indent=2)
    if output_path == "-":
        print(output)
        return

    path = output_path
    if not path:
        timestamp = datetime.now().strftime("%Y%m%d-%H%M%S")
        database_dir = os.path.dirname(db.default_db_path())
        path = os.path.join(database_dir, f"snapshot-{timestamp}.json")
        print(messages["exporting"].format(path=path))
    path = os.path.abspath(path)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8") as handle:
        handle.write(output + "\n")

    if zip_output:
        zip_path = f"{os.path.splitext(path)[0]}.zip"
        with zipfile.ZipFile(zip_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            archive.write(path, arcname=os.path.basename(path))
        os.remove(path)
        print(messages["wrote_zip"].format(path=zip_path))


if __name__ == "__main__":
    main()
