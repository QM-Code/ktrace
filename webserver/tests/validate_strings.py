#!/usr/bin/env python3
import json
import os
import sys


def _usage():
    return (
        "usage: validate_strings.py [--master <lang-code|path>] "
        "[--unused | --purge-unused] (--all | <lang-code> | <path-to-json>)\n"
        "       validate_strings.py [--master <lang-code|path>] --unused-master"
    )


def _load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def _missing_paths(source, target, prefix=""):
    missing = []
    if isinstance(source, dict):
        if not isinstance(target, dict):
            for key in source.keys():
                path = f"{prefix}{key}" if not prefix else f"{prefix}.{key}"
                missing.append(path)
            return missing
        for key, value in source.items():
            path = f"{prefix}{key}" if not prefix else f"{prefix}.{key}"
            if key not in target:
                missing.append(path)
                continue
            missing.extend(_missing_paths(value, target.get(key), path))
    return missing


def _extra_paths(source, target, prefix=""):
    extra = []
    if isinstance(target, dict):
        if not isinstance(source, dict):
            for key in target.keys():
                path = f"{prefix}{key}" if not prefix else f"{prefix}.{key}"
                extra.append(path)
            return extra
        for key, value in target.items():
            path = f"{prefix}{key}" if not prefix else f"{prefix}.{key}"
            if key not in source:
                extra.append(path)
                continue
            extra.extend(_extra_paths(source.get(key), value, path))
    return extra


def _remove_extra_keys(source, target):
    if not isinstance(target, dict):
        return
    if not isinstance(source, dict):
        target.clear()
        return
    for key in list(target.keys()):
        if key not in source:
            del target[key]
            continue
        _remove_extra_keys(source.get(key), target.get(key))
        if isinstance(target.get(key), dict) and not target[key]:
            if isinstance(source.get(key), dict) and source.get(key):
                continue
            del target[key]


def _all_paths(source, prefix=""):
    paths = []
    if isinstance(source, dict):
        for key, value in source.items():
            path = f"{prefix}{key}" if not prefix else f"{prefix}.{key}"
            paths.append(path)
            paths.extend(_all_paths(value, path))
    return paths


def _leaf_paths(source, prefix=""):
    paths = []
    if isinstance(source, dict):
        for key, value in source.items():
            path = f"{prefix}{key}" if not prefix else f"{prefix}.{key}"
            if isinstance(value, dict):
                paths.extend(_leaf_paths(value, path))
            else:
                paths.append(path)
    return paths


def _strings_dir():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    return os.path.join(root_dir, "strings")


def _resolve_target(arg):
    if os.path.isfile(arg):
        return os.path.abspath(arg)
    if arg.endswith(".json"):
        return os.path.abspath(arg)
    strings_dir = _strings_dir()
    candidate = os.path.join(strings_dir, f"{arg}.json")
    if os.path.isfile(candidate):
        return candidate
    return None


def _check_file(path, master_data):
    target = _load_json(path)
    missing = _missing_paths(master_data, target)
    return missing


def _check_unused(path, master_data):
    target = _load_json(path)
    extra = _extra_paths(master_data, target)
    return extra


def _code_roots():
    root_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
    return [
        os.path.join(root_dir, "karma"),
        os.path.join(root_dir, "bin"),
        os.path.join(root_dir, "tests"),
    ]


def _iter_py_files():
    for root in _code_roots():
        if not os.path.isdir(root):
            continue
        for dirpath, _, filenames in os.walk(root):
            for name in filenames:
                if name.endswith(".py"):
                    yield os.path.join(dirpath, name)


def _find_message_namespace(source):
    for line in source.splitlines():
        if "config.ui_text" not in line:
            continue
        if "messages." not in line:
            continue
        if "{key}" not in line:
            continue
        start = line.find("messages.")
        if start == -1:
            continue
        fragment = line[start + len("messages.") :]
        namespace = []
        for ch in fragment:
            if ch.isalnum() or ch == "_":
                namespace.append(ch)
            else:
                break
        if namespace:
            return "".join(namespace)
    return None


def _collect_used_keys():
    used = set()
    helper_map = {
        "_label": "ui_text.labels",
        "_action": "ui_text.actions",
        "_title": "ui_text.titles",
        "_section": "ui_text.sections",
        "_status": "ui_text.status",
        "_confirm": "ui_text.confirm",
        "_confirm_text": "ui_text.confirmations",
        "_template": "ui_text.templates",
        "_header_text": "ui_text.header",
        "_nav": "ui_text.nav",
    }
    for path in _iter_py_files():
        with open(path, "r", encoding="utf-8") as handle:
            source = handle.read()
        msg_namespace = _find_message_namespace(source)
        for line in source.splitlines():
            line = line.strip()
            if "config.ui_text" in line:
                parts = line.split("config.ui_text", 1)[1]
                if "(" in parts:
                    start = parts.find("(") + 1
                    quote = parts[start:start + 1]
                    if quote in ("'", '"'):
                        end = parts.find(quote, start + 1)
                        if end > start:
                            key = parts[start + 1:end]
                            if key.startswith("ui_text."):
                                used.add(key)
                            else:
                                used.add(f"ui_text.{key}")
            if "config.require_setting" in line and "ui_text." in line:
                parts = line.split("config.require_setting", 1)[1]
                if "\"" in parts or "'" in parts:
                    quote = '"' if '"' in parts else "'"
                    start = parts.find(quote)
                    end = parts.find(quote, start + 1)
                    if start != -1 and end > start:
                        key = parts[start + 1:end]
                        if key.startswith("ui_text."):
                            used.add(key)
            for helper, prefix in helper_map.items():
                token = f"{helper}("
                if token in line:
                    start = line.find(token) + len(token)
                    if start < len(line) and line[start] in ("'", '"'):
                        quote = line[start]
                        end = line.find(quote, start + 1)
                        if end > start:
                            key = line[start + 1:end]
                            used.add(f"{prefix}.{key}")
            if msg_namespace and "_msg(" in line:
                start = line.find("_msg(") + len("_msg(")
                if start < len(line) and line[start] in ("'", '"'):
                    quote = line[start]
                    end = line.find(quote, start + 1)
                    if end > start:
                        key = line[start + 1:end]
                        used.add(f"ui_text.messages.{msg_namespace}.{key}")
        if os.path.basename(path) == "initialize.py":
            lines = source.splitlines()
            for idx, line in enumerate(lines):
                if line.strip().startswith("keys = ["):
                    for item in lines[idx + 1:]:
                        stripped = item.strip()
                        if stripped.startswith("]"):
                            break
                        if stripped.startswith(("\"", "'")):
                            quote = stripped[0]
                            end = stripped.find(quote, 1)
                            if end > 1:
                                key = stripped[1:end]
                                used.add(f"scripts.initialize.{key}")
    with_prefixes = set()
    for key in used:
        with_prefixes.add(key)
        parts = key.split(".")
        for i in range(1, len(parts)):
            with_prefixes.add(".".join(parts[:i]))
    return with_prefixes


def main():
    args = sys.argv[1:]
    if not args:
        raise SystemExit(_usage())

    strings_dir = _strings_dir()
    master_path = os.path.join(strings_dir, "en.json")
    if not os.path.isfile(master_path):
        raise SystemExit("strings/en.json not found.")

    if args[0] == "--master":
        if len(args) < 2:
            raise SystemExit(_usage())
        resolved = _resolve_target(args[1])
        if not resolved:
            raise SystemExit(_usage())
        master_path = resolved
        args = args[2:]
        if not args:
            raise SystemExit(_usage())

    show_unused = False
    purge_unused = False
    check_unused_master = False
    if args[0] == "--unused-master":
        check_unused_master = True
        args = args[1:]
    if args and args[0] in ("--unused", "--purge-unused"):
        show_unused = True
        purge_unused = args[0] == "--purge-unused"
        args = args[1:]
        if not args and not check_unused_master:
            raise SystemExit(_usage())

    master_data = _load_json(master_path)
    master_name = os.path.basename(master_path)

    if check_unused_master:
        master_leaf = set(_leaf_paths(master_data))
        used = _collect_used_keys()
        unused = sorted(path for path in master_leaf if path not in used)
        if unused:
            print(f"Unused keys in {master_name}:")
            for key in unused:
                print(f"  - {key}")
            raise SystemExit(1)
        print("No unused master keys found.")
        return

    if args[0] == "--all":
        paths = []
        for name in os.listdir(strings_dir):
            if name.endswith(".json") and name != master_name:
                paths.append(os.path.join(strings_dir, name))
        if not paths:
            print("No language files found.")
            return
        failed = False
        for path in sorted(paths):
            missing = _check_file(path, master_data)
            if missing:
                failed = True
                print(f"{os.path.basename(path)} missing keys:")
                for key in missing:
                    print(f"  - {key}")
            if show_unused:
                extra = _check_unused(path, master_data)
                if extra:
                    if not purge_unused:
                        failed = True
                        print(f"{os.path.basename(path)} unused keys:")
                        for key in extra:
                            print(f"  - {key}")
                    else:
                        data = _load_json(path)
                        _remove_extra_keys(master_data, data)
                        with open(path, "w", encoding="utf-8") as handle:
                            json.dump(data, handle, ensure_ascii=False, indent=2)
                            handle.write("\n")
        if failed:
            raise SystemExit(1)
        if purge_unused:
            print("Unused keys removed.")
        elif show_unused:
            print("No unused keys found.")
        else:
            print("All language files include required keys.")
        return

    target_path = _resolve_target(args[0])
    if not target_path:
        raise SystemExit(_usage())

    missing = _check_file(target_path, master_data)
    if missing:
        print(f"Missing keys in {target_path}:")
        for key in missing:
            print(f"  - {key}")
        raise SystemExit(1)
    if show_unused:
        extra = _check_unused(target_path, master_data)
        if extra:
            if purge_unused:
                data = _load_json(target_path)
                _remove_extra_keys(master_data, data)
                with open(target_path, "w", encoding="utf-8") as handle:
                    json.dump(data, handle, ensure_ascii=False, indent=2)
                    handle.write("\n")
                print("Unused keys removed.")
            else:
                print(f"Unused keys in {target_path}:")
                for key in extra:
                    print(f"  - {key}")
                raise SystemExit(1)
        else:
            print("No unused keys found.")
    else:
        print("All required keys present.")


if __name__ == "__main__":
    main()
