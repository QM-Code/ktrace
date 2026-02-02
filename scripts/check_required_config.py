#!/usr/bin/env python3
import os
import re
import sys


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
SRC_DIR = os.path.join(ROOT, "src")
VALIDATION_FILE = os.path.join(ROOT, "src", "engine", "common", "config_validation.cpp")

REQUIRED_READ_PATTERNS = [
    re.compile(r'ReadRequired(?:Bool|UInt16|Float|String)Config\\(\"([^\"]+)\"\\)'),
    re.compile(r'ui::config::GetRequired(?:Bool|Float|String)\\(\"([^\"]+)\"\\)'),
]

VALIDATION_KEY_PATTERN = re.compile(r'\\{\"([^\"]+)\",\\s*RequiredType::\\w+\\}')


def _iter_cpp_files():
    for root, _, filenames in os.walk(SRC_DIR):
        for name in filenames:
            if name.endswith((".cpp", ".hpp", ".h", ".cc")):
                yield os.path.join(root, name)


def _collect_required_reads():
    found = set()
    for path in _iter_cpp_files():
        with open(path, "r", encoding="utf-8") as handle:
            text = handle.read()
        for pattern in REQUIRED_READ_PATTERNS:
            for match in pattern.findall(text):
                found.add(match)
    return found


def _collect_validation_keys():
    if not os.path.isfile(VALIDATION_FILE):
        raise SystemExit(f"Missing validation file: {VALIDATION_FILE}")
    with open(VALIDATION_FILE, "r", encoding="utf-8") as handle:
        text = handle.read()
    return set(VALIDATION_KEY_PATTERN.findall(text))


def main():
    required_reads = _collect_required_reads()
    validation_keys = _collect_validation_keys()

    missing = sorted(required_reads - validation_keys)
    extra = sorted(validation_keys - required_reads)

    if not missing and not extra:
        print("Required config list matches ReadRequired* and ui::config::GetRequired* usage.")
        return 0

    if missing:
        print("Missing in config_validation.cpp:")
        for key in missing:
            print(f"  {key}")
    if extra:
        print("Unused in config_validation.cpp:")
        for key in extra:
            print(f"  {key}")
    return 1


if __name__ == "__main__":
    sys.exit(main())
