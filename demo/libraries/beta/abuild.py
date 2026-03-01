#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys


SDK_COMPONENT = "BetaSDK"
SDK_CONFIG_PATH = ("lib", "cmake", "KTraceSDK", "KTraceSDKConfig.cmake")


def usage(exit_code: int = 1) -> None:
    prog = os.path.basename(sys.argv[0])
    print(f"Usage: {prog} <options>", file=sys.stderr)
    print("  -a, --agent         required agent/session owner name", file=sys.stderr)
    print("  -d, --directory     build directory under build/ (example: build/test/)", file=sys.stderr)
    print("  --ktrace-sdk <dir>  required KTraceSDK install prefix", file=sys.stderr)
    print("  --install-sdk <dir> optional BetaSDK install prefix (default: <build-dir>/sdk)", file=sys.stderr)
    print("  --no-configure      skip cmake configure step", file=sys.stderr)
    raise SystemExit(exit_code)


def fail(message: str) -> None:
    print(f"Error: {message}", file=sys.stderr)
    usage(1)


def run(cmd: list[str], *, env: dict[str, str] | None = None) -> None:
    subprocess.run(cmd, check=True, env=env)


def resolve_prefix(path_arg: str, base_dir: str) -> str:
    if os.path.isabs(path_arg):
        return os.path.abspath(path_arg)
    return os.path.abspath(path_arg)


def validate_build_dir_layout(build_dir: str) -> None:
    normalized = build_dir.replace("\\", "/")
    while normalized.startswith("./"):
        normalized = normalized[2:]
    parts = [part for part in normalized.split("/") if part not in ("", ".")]
    if len(parts) < 2 or parts[0] != "build" or any(part == ".." for part in parts):
        fail("build directory must be under 'build/' (example: build/test/)")


def clean_install_prefix(prefix: str) -> None:
    if os.path.isfile(prefix):
        print(f"Error: install prefix is a file, expected directory: {prefix}", file=sys.stderr)
        raise SystemExit(2)

    os.makedirs(prefix, exist_ok=True)
    for entry in ("include", "lib", "bin", "share"):
        path = os.path.join(prefix, entry)
        if os.path.isdir(path):
            shutil.rmtree(path)
        elif os.path.exists(path):
            os.remove(path)


def validate_ktrace_sdk(prefix: str) -> None:
    config = os.path.join(prefix, *SDK_CONFIG_PATH)
    if os.path.isfile(config):
        return
    print(
        "Error: invalid --ktrace-sdk prefix; missing package config.\n"
        f"Expected:\n  {config}",
        file=sys.stderr,
    )
    raise SystemExit(2)


def read_cache_value(cache_path: str, key: str) -> str:
    if not os.path.isfile(cache_path):
        return ""

    needle = f"{key}:"
    try:
        with open(cache_path, "r", encoding="utf-8") as cache:
            for line in cache:
                if line.startswith(needle):
                    return line.split("=", 1)[1].strip()
    except OSError:
        return ""

    return ""


def infer_triplet_from_installed_dir(installed_dir: str) -> str:
    try:
        entries = sorted(
            entry
            for entry in os.listdir(installed_dir)
            if os.path.isdir(os.path.join(installed_dir, entry)) and entry != "vcpkg"
        )
    except OSError:
        return ""

    if len(entries) == 1:
        return entries[0]
    return ""


def resolve_vcpkg_context(ktrace_sdk_prefix: str, repo_root: str) -> tuple[str, str]:
    candidate_build_dirs: list[str] = []
    if os.path.basename(ktrace_sdk_prefix.rstrip("/\\")) == "sdk":
        candidate_build_dirs.append(os.path.abspath(os.path.dirname(ktrace_sdk_prefix)))
    candidate_build_dirs.append(os.path.abspath(os.path.join(ktrace_sdk_prefix, os.pardir)))

    unique_candidates: list[str] = []
    seen: set[str] = set()
    for candidate in candidate_build_dirs:
        if candidate in seen:
            continue
        unique_candidates.append(candidate)
        seen.add(candidate)

    for build_candidate in unique_candidates:
        installed_dir = os.path.join(build_candidate, "installed")
        if not os.path.isdir(installed_dir):
            continue
        cache_path = os.path.join(build_candidate, "CMakeCache.txt")
        triplet = read_cache_value(cache_path, "VCPKG_TARGET_TRIPLET")
        if triplet and os.path.isdir(os.path.join(installed_dir, triplet)):
            return os.path.abspath(installed_dir), triplet

        inferred = infer_triplet_from_installed_dir(installed_dir)
        if inferred:
            return os.path.abspath(installed_dir), inferred

    env_installed = os.environ.get("VCPKG_INSTALLED_DIR", "").strip()
    env_triplet = os.environ.get("VCPKG_TARGET_TRIPLET", "").strip()
    if env_installed and env_triplet:
        env_installed_abs = resolve_prefix(env_installed, repo_root)
        if os.path.isdir(os.path.join(env_installed_abs, env_triplet)):
            return env_installed_abs, env_triplet

    print(
        "Error: could not infer vcpkg installed tree/triplet from --ktrace-sdk.\n"
        f"KTrace SDK:\n  {ktrace_sdk_prefix}",
        file=sys.stderr,
    )
    raise SystemExit(2)


def is_local_vcpkg_bootstrapped(vcpkg_root: str) -> bool:
    candidates = [
        os.path.join(vcpkg_root, "vcpkg"),
        os.path.join(vcpkg_root, "vcpkg.exe"),
        os.path.join(vcpkg_root, "vcpkg.bat"),
    ]
    return any(os.path.isfile(path) for path in candidates)


def ensure_local_vcpkg(repo_root: str) -> tuple[str, str]:
    local_vcpkg_root = os.path.join(repo_root, "vcpkg", "src")
    local_toolchain = os.path.join(local_vcpkg_root, "scripts", "buildsystems", "vcpkg.cmake")

    if not os.path.isdir(local_vcpkg_root):
        print("Error: local vcpkg is required, but ./vcpkg/src is missing.", file=sys.stderr)
        raise SystemExit(2)
    if not os.path.isfile(local_toolchain):
        print("Error: local vcpkg toolchain file is missing.", file=sys.stderr)
        raise SystemExit(2)
    if not is_local_vcpkg_bootstrapped(local_vcpkg_root):
        print("Error: local ./vcpkg/src is present but not bootstrapped.", file=sys.stderr)
        raise SystemExit(2)

    return os.path.abspath(local_vcpkg_root), os.path.abspath(local_toolchain)


def main() -> int:
    args = sys.argv[1:]
    agent_name = ""
    build_dir = ""
    ktrace_sdk_arg = ""
    install_sdk_arg = ""
    no_configure = False

    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ("-h", "--help"):
            usage(0)
        elif arg in ("-a", "--agent"):
            i += 1
            if i >= len(args):
                fail("missing value for --agent")
            agent_name = args[i].strip()
        elif arg in ("-d", "--directory"):
            i += 1
            if i >= len(args):
                fail("missing value for --directory")
            build_dir = args[i]
        elif arg == "--ktrace-sdk":
            i += 1
            if i >= len(args):
                fail("missing value for --ktrace-sdk")
            ktrace_sdk_arg = args[i]
        elif arg == "--install-sdk":
            i += 1
            if i >= len(args):
                fail("missing value for --install-sdk")
            install_sdk_arg = args[i]
        elif arg == "--no-configure":
            no_configure = True
        elif arg.startswith("-"):
            fail(f"unknown option '{arg}'")
        else:
            if build_dir:
                fail("only one build directory may be provided")
            build_dir = arg
        i += 1

    if not agent_name:
        fail("--agent <name> is required")
    if not build_dir:
        fail("--directory <build-dir> is required")
    if not ktrace_sdk_arg:
        fail("--ktrace-sdk <path> is required")

    validate_build_dir_layout(build_dir)

    script_dir = os.path.abspath(os.path.dirname(__file__))
    repo_root = os.path.abspath(os.path.join(script_dir, "..", "..", ".."))

    ktrace_sdk_prefix = resolve_prefix(ktrace_sdk_arg, repo_root)
    validate_ktrace_sdk(ktrace_sdk_prefix)
    vcpkg_installed_dir, vcpkg_triplet = resolve_vcpkg_context(ktrace_sdk_prefix, repo_root)
    vcpkg_prefix = os.path.join(vcpkg_installed_dir, vcpkg_triplet)

    source_dir = script_dir

    local_vcpkg_root, local_toolchain = ensure_local_vcpkg(repo_root)
    local_vcpkg_build_root = os.path.join(repo_root, "vcpkg", "build")
    local_vcpkg_downloads = os.path.join(local_vcpkg_build_root, "downloads")
    local_vcpkg_binary_cache = os.path.join(local_vcpkg_build_root, "binary-cache")
    os.makedirs(local_vcpkg_downloads, exist_ok=True)
    os.makedirs(local_vcpkg_binary_cache, exist_ok=True)

    env = os.environ.copy()
    env["VCPKG_ROOT"] = local_vcpkg_root
    if not env.get("VCPKG_DOWNLOADS", "").strip():
        env["VCPKG_DOWNLOADS"] = local_vcpkg_downloads
    if not env.get("VCPKG_DEFAULT_BINARY_CACHE", "").strip():
        env["VCPKG_DEFAULT_BINARY_CACHE"] = local_vcpkg_binary_cache

    cmake_args = [
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DVCPKG_TARGET_TRIPLET={vcpkg_triplet}",
        f"-DVCPKG_INSTALLED_DIR={vcpkg_installed_dir}",
        f"-DCMAKE_PREFIX_PATH={ktrace_sdk_prefix};{vcpkg_prefix}",
        "-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON",
        f"-DKTraceSDK_DIR={os.path.join(ktrace_sdk_prefix, 'lib', 'cmake', 'KTraceSDK')}",
        f"-DCMAKE_TOOLCHAIN_FILE={local_toolchain}",
    ]

    if no_configure:
        cache_path = os.path.join(build_dir, "CMakeCache.txt")
        if not os.path.isfile(cache_path):
            fail("--no-configure requires an existing CMakeCache.txt in the build directory")
    else:
        os.makedirs(build_dir, exist_ok=True)
        run(["cmake", "-S", source_dir, "-B", build_dir, *cmake_args], env=env)

    run(["cmake", "--build", build_dir, "-j4"], env=env)

    if install_sdk_arg:
        install_prefix = resolve_prefix(install_sdk_arg, repo_root)
    else:
        install_prefix = os.path.abspath(os.path.join(build_dir, "sdk"))

    clean_install_prefix(install_prefix)
    run(
        [
            "cmake",
            "--install",
            build_dir,
            "--prefix",
            install_prefix,
            "--component",
            SDK_COMPONENT,
        ],
        env=env,
    )

    print(f"Build complete -> dir={build_dir} | sdk={install_prefix}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
