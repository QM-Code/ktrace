#!/usr/bin/env python3

import json
import os
import sys


LOCAL_CONFIG_FILENAME = ".kbuild.json"


def fail(message: str, *, exit_code: int = 2) -> None:
    print(f"Error: {message}", file=sys.stderr)
    raise SystemExit(exit_code)


def fail_invalid_local_root(root_token: str) -> None:
    fail(
        "could not bootstrap from ./.kbuild.json.\n"
        f"Value specified for kbuild.root is invalid: {json.dumps(root_token)}\n"
        "Run './kbuild.py --kbuild-root <path>' to set kbuild library directory."
    )


def enforce_script_directory() -> str:
    repo_root = os.path.abspath(os.path.dirname(__file__))
    cwd = os.path.abspath(os.getcwd())
    repo_root_cmp = os.path.normcase(os.path.realpath(repo_root))
    cwd_cmp = os.path.normcase(os.path.realpath(cwd))
    if cwd_cmp != repo_root_cmp:
        fail("kbuild.py must be run from the directory it is in. Run `./kbuild.py` from that directory.")
    return repo_root


def _load_json_object(path: str, *, display_name: str) -> dict[str, object]:
    try:
        with open(path, "r", encoding="utf-8") as handle:
            payload = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"could not parse ./{display_name}: {exc}")
    if not isinstance(payload, dict):
        fail(f"{display_name} must be a JSON object")
    return payload


def _write_json_object(path: str, payload: dict[str, object]) -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(payload, handle, indent=2)
        handle.write("\n")


def _write_local_root(repo_root: str, root_token: str) -> None:
    local_path = os.path.join(repo_root, LOCAL_CONFIG_FILENAME)
    payload: dict[str, object] = {}
    if os.path.isfile(local_path):
        payload = _load_json_object(local_path, display_name=LOCAL_CONFIG_FILENAME)
    elif os.path.exists(local_path):
        fail("./.kbuild.json exists but is not a regular file")

    kbuild_raw = payload.get("kbuild")
    if not isinstance(kbuild_raw, dict):
        kbuild_raw = {}

    kbuild_raw["root"] = root_token
    payload["kbuild"] = kbuild_raw
    _write_json_object(local_path, payload)


def load_config_root_token(repo_root: str) -> str:
    local_path = os.path.join(repo_root, LOCAL_CONFIG_FILENAME)
    if not os.path.isfile(local_path):
        fail(
            "missing required local config file './.kbuild.json'.\n"
            "Run './kbuild.py --kbuild-root <path>' first."
        )

    payload = _load_json_object(local_path, display_name=LOCAL_CONFIG_FILENAME)
    kbuild_raw = payload.get("kbuild")
    if not isinstance(kbuild_raw, dict):
        fail("kbuild.root is required in .kbuild.json. Run './kbuild.py --kbuild-root <path>' first.")

    root_raw = kbuild_raw.get("root")
    if not isinstance(root_raw, str) or not root_raw.strip():
        fail("kbuild.root is required in .kbuild.json. Run './kbuild.py --kbuild-root <path>' first.")

    return root_raw.strip()


def resolve_root(repo_root: str, root_token: str) -> str:
    if os.path.isabs(root_token):
        root_abs = os.path.abspath(root_token)
    else:
        root_abs = os.path.abspath(os.path.join(repo_root, root_token))

    if not os.path.isdir(root_abs):
        raise ValueError(f"kbuild.root resolves to '{root_abs}' but does not exist")

    return root_abs


def load_core_runner(root_abs: str):
    libs_dir = os.path.join(root_abs, "libs")
    package_init = os.path.join(libs_dir, "kbuild", "__init__.py")
    if not os.path.isfile(package_init):
        raise ValueError(f"required shared library package is missing: {package_init}")

    if libs_dir not in sys.path:
        sys.path.insert(0, libs_dir)

    try:
        from kbuild import run as run_core
    except Exception as exc:  # pragma: no cover
        raise ValueError(f"failed to load shared kbuild library from {libs_dir}: {exc}") from exc

    return run_core


def main() -> int:
    repo_root = enforce_script_directory()
    raw_args = list(sys.argv[1:])

    if raw_args[:1] == ["--kbuild-root"]:
        if len(raw_args) == 1:
            fail("missing value for '--kbuild-root'", exit_code=1)
        if len(raw_args) != 2:
            fail(
                "--kbuild-root cannot be combined with other options. "
                "Run './kbuild.py --kbuild-root <path>' by itself.",
                exit_code=1,
            )
        root_token = raw_args[1].strip()
        if not root_token:
            fail("--kbuild-root requires a non-empty value", exit_code=1)
        try:
            root_abs = resolve_root(repo_root, root_token)
            load_core_runner(root_abs)
        except ValueError as exc:
            fail(f"--kbuild-root path is not a valid kbuild directory: {exc}", exit_code=1)
        _write_local_root(repo_root, root_token)
        print(f"Updated ./.kbuild.json with kbuild.root='{root_token}'", flush=True)
        return 0

    if "--kbuild-root" in raw_args:
        fail(
            "--kbuild-root cannot be combined with other options. "
            "Run './kbuild.py --kbuild-root <path>' by itself.",
            exit_code=1,
        )

    root_token = load_config_root_token(repo_root)
    try:
        root_abs = resolve_root(repo_root, root_token)
        run_core = load_core_runner(root_abs)
    except ValueError:
        fail_invalid_local_root(root_token)

    return run_core(
        repo_root=repo_root,
        argv=raw_args,
        kbuild_root=root_abs,
        program_name=os.path.basename(sys.argv[0]),
    )


if __name__ == "__main__":
    raise SystemExit(main())
