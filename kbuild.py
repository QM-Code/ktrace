#!/usr/bin/env python3

import json
import os
import sys


WRAPPER_API = "1"


def fail(message: str, *, exit_code: int = 2) -> None:
    print(f"Error: {message}", file=sys.stderr)
    raise SystemExit(exit_code)


def enforce_script_directory() -> str:
    repo_root = os.path.abspath(os.path.dirname(__file__))
    cwd = os.path.abspath(os.getcwd())
    repo_root_cmp = os.path.normcase(os.path.realpath(repo_root))
    cwd_cmp = os.path.normcase(os.path.realpath(cwd))
    if cwd_cmp != repo_root_cmp:
        fail("kbuild.py must be run from the directory it is in. Run `./kbuild.py` from that directory.")
    return repo_root


def parse_bootstrap_root_arg(args: list[str]) -> tuple[str | None, list[str]]:
    root_override: str | None = None
    passthrough: list[str] = []

    i = 0
    while i < len(args):
        arg = args[i]
        if arg == "--kbuild-root":
            i += 1
            if i >= len(args):
                fail("missing value for '--kbuild-root'", exit_code=1)
            value = args[i].strip()
            if not value:
                fail("--kbuild-root requires a non-empty value", exit_code=1)
            if root_override is not None:
                fail("--kbuild-root cannot be specified more than once", exit_code=1)
            root_override = value
        else:
            passthrough.append(arg)
        i += 1

    return root_override, passthrough


def load_config_root_token(repo_root: str) -> str:
    config_path = os.path.join(repo_root, "kbuild.json")
    if not os.path.isfile(config_path):
        fail(
            "'kbuild.json' does not exist and --kbuild-root was not provided. "
            "For bootstrap, run: ./kbuild.py --kbuild-root <path> --create-config"
        )

    try:
        with open(config_path, "r", encoding="utf-8") as handle:
            payload = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        fail(f"could not parse ./kbuild.json: {exc}")

    if not isinstance(payload, dict):
        fail("kbuild.json must be a JSON object")

    kbuild_raw = payload.get("kbuild")
    if not isinstance(kbuild_raw, dict):
        fail("kbuild.rootdir is required in kbuild.json unless --kbuild-root is provided")

    root_raw = kbuild_raw.get("rootdir")
    if not isinstance(root_raw, str) or not root_raw.strip():
        fail("kbuild.rootdir is required in kbuild.json unless --kbuild-root is provided")

    api_raw = kbuild_raw.get("api")
    if api_raw is not None:
        if not isinstance(api_raw, str) or not api_raw.strip():
            fail("kbuild.api must be a non-empty string when defined")
        if api_raw.strip() != WRAPPER_API:
            fail(
                f"kbuild.api mismatch: config={api_raw.strip()} wrapper={WRAPPER_API}. "
                "Update one side so they match."
            )

    return root_raw.strip()


def resolve_rootdir(repo_root: str, root_token: str) -> str:
    if os.path.isabs(root_token):
        root_abs = os.path.abspath(root_token)
    else:
        root_abs = os.path.abspath(os.path.join(repo_root, root_token))

    if not os.path.isdir(root_abs):
        fail(
            f"kbuild.rootdir resolves to '{root_abs}' but does not exist. "
            "Check for a typo or create the directory."
        )

    return root_abs


def main() -> int:
    repo_root = enforce_script_directory()
    raw_args = sys.argv[1:]

    root_override, passthrough_args = parse_bootstrap_root_arg(raw_args)
    root_token = root_override if root_override is not None else load_config_root_token(repo_root)
    root_abs = resolve_rootdir(repo_root, root_token)

    libs_dir = os.path.join(root_abs, "libs")
    if not os.path.isdir(libs_dir):
        fail(f"kbuild.rootdir points to '{root_abs}', but required library directory is missing: {libs_dir}")

    if libs_dir not in sys.path:
        sys.path.insert(0, libs_dir)

    try:
        from kbuild import run as run_core
    except Exception as exc:  # pragma: no cover
        fail(f"failed to load shared kbuild library from {libs_dir}: {exc}")

    return run_core(
        repo_root=repo_root,
        argv=passthrough_args,
        kbuild_root=root_abs,
        program_name=os.path.basename(sys.argv[0]),
        bootstrap_root_override=root_override,
    )


if __name__ == "__main__":
    raise SystemExit(main())
