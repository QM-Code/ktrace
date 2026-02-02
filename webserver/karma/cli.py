import json
import os
import sys


def ensure_community_dir(path, usage):
    if not path:
        raise SystemExit(usage)
    if not os.path.isdir(path):
        raise SystemExit(f"Community directory not found: {path}")
    config_path = os.path.join(path, "config.json")
    if not os.path.isfile(config_path):
        raise SystemExit(f"Missing community config.json in {path}")


def bootstrap(directory, usage):
    ensure_community_dir(directory, usage)
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    if root_dir not in sys.path:
        sys.path.insert(0, root_dir)
    from karma import config
    config.set_community_dir(directory)
    return root_dir


def validate_json_file(path):
    if not os.path.isfile(path):
        raise SystemExit(f"JSON file not found: {path}")
    try:
        with open(path, "r", encoding="utf-8") as handle:
            json.load(handle)
    except UnicodeDecodeError:
        raise SystemExit(f"Invalid JSON file encoding (expected UTF-8): {path}")
    except json.JSONDecodeError as exc:
        raise SystemExit(f"Invalid JSON file: line {exc.lineno} column {exc.colno}")
