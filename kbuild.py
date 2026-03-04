#!/usr/bin/env python3

import json
import os
import re
import shutil
import subprocess
import sys
import tempfile


VCPKG_REPO_URL = "https://github.com/microsoft/vcpkg.git"


def usage(exit_code: int = 1) -> None:
    prog = os.path.basename(sys.argv[0])
    print(f"Usage: {prog} <options>", file=sys.stderr)
    print("  --list-builds       list existing build version directories", file=sys.stderr)
    print("  --remove-latest     remove every build/latest/ directory", file=sys.stderr)
    print("  --version <name>    build version slot under build/ (default: latest)", file=sys.stderr)
    print(
        "  --build-demos [demo ...]  build demos in order; with no args uses kbuild.json 'build-demos'",
        file=sys.stderr,
    )
    print("  --no-configure      skip cmake configure step", file=sys.stderr)
    print("  --create-config     create a starter kbuild.json template", file=sys.stderr)
    print("  --initialize-repo   scaffold this repo from kbuild.json metadata", file=sys.stderr)
    print(
        "  --initialize-git    verify remote, initialize local git repo, commit, and push main",
        file=sys.stderr,
    )
    print("  --git-sync <msg>    git add . && git commit -m <msg> && git push", file=sys.stderr)
    print("  --sync-vcpkg-baseline  set baseline fields from ./vcpkg/src HEAD", file=sys.stderr)
    print(
        "  --install-vcpkg     clone/bootstrap local vcpkg under ./vcpkg, sync baseline, then build",
        file=sys.stderr,
    )
    raise SystemExit(exit_code)


def fail(message: str) -> None:
    print(f"Error: {message}", file=sys.stderr)
    usage(1)


def run(cmd: list[str], *, env: dict[str, str] | None = None) -> None:
    subprocess.run(cmd, check=True, env=env)


def enforce_script_directory() -> str:
    repo_root = os.path.abspath(os.path.dirname(__file__))
    cwd = os.path.abspath(os.getcwd())
    repo_root_cmp = os.path.normcase(os.path.realpath(repo_root))
    cwd_cmp = os.path.normcase(os.path.realpath(cwd))
    if cwd_cmp != repo_root_cmp:
        print(
            "Error: kbuild.py must be run from the directory it is in.\n"
            "Run `./kbuild.py` from that directory.",
            file=sys.stderr,
        )
        raise SystemExit(2)
    return repo_root


def format_dir_for_output(path: str, repo_root: str) -> str:
    rel = os.path.relpath(path, repo_root).replace("\\", "/").strip("/")
    return f"./{rel}/"


def collect_build_version_dirs(repo_root: str) -> list[str]:
    output: list[str] = []
    core_build_root = os.path.join(repo_root, "build")
    if os.path.isdir(core_build_root):
        for entry in sorted(os.listdir(core_build_root)):
            path = os.path.join(core_build_root, entry)
            if os.path.isdir(path):
                output.append(path)

    demo_root = os.path.join(repo_root, "demo")
    if os.path.isdir(demo_root):
        for current_root, dirnames, _ in os.walk(demo_root):
            dirnames.sort()
            if "build" not in dirnames:
                continue

            demo_build_root = os.path.join(current_root, "build")
            for entry in sorted(os.listdir(demo_build_root)):
                path = os.path.join(demo_build_root, entry)
                if os.path.isdir(path):
                    output.append(path)

            # Build trees can be large; they are not candidates for nested demo discovery.
            dirnames.remove("build")
    return output


def list_build_dirs(repo_root: str) -> int:
    output = collect_build_version_dirs(repo_root)

    for line in output:
        print(format_dir_for_output(line, repo_root))
    return 0


def is_safe_latest_build_dir(path: str, repo_root: str) -> bool:
    path_abs = os.path.abspath(path)
    repo_root_abs = os.path.abspath(repo_root)
    rel = os.path.relpath(path_abs, repo_root_abs).replace("\\", "/")
    if rel == ".." or rel.startswith("../"):
        return False

    parts = [part for part in rel.split("/") if part not in ("", ".")]
    if parts == ["build", "latest"]:
        return True
    if len(parts) >= 4 and parts[0] == "demo" and parts[-2:] == ["build", "latest"]:
        return True
    return False


def remove_latest_build_dirs(repo_root: str) -> int:
    removed = 0
    for path in collect_build_version_dirs(repo_root):
        if os.path.basename(path.rstrip("/\\")) != "latest":
            continue
        if not is_safe_latest_build_dir(path, repo_root):
            fail(f"refusing to remove unexpected latest directory: {path}")
        if os.path.islink(path):
            fail(f"refusing to remove symlinked latest directory: {path}")
        shutil.rmtree(path)
        removed += 1
        print(f"removed {format_dir_for_output(path, repo_root)}")

    if removed == 0:
        print("no build/latest/ directories found")
    return 0


def resolve_prefix(path_arg: str, repo_root: str) -> str:
    if os.path.isabs(path_arg):
        return os.path.abspath(path_arg)
    return os.path.abspath(os.path.join(repo_root, path_arg))


def package_config_path(prefix: str, cmake_package_name: str) -> str:
    return os.path.join(
        prefix,
        "lib",
        "cmake",
        cmake_package_name,
        f"{cmake_package_name}Config.cmake",
    )


def package_dir(prefix: str, cmake_package_name: str) -> str:
    return os.path.join(prefix, "lib", "cmake", cmake_package_name)


def load_kbuild_config(repo_root: str) -> tuple[str, bool, list[str], list[dict[str, object]]]:
    config_path = os.path.join(repo_root, "kbuild.json")
    if not os.path.isfile(config_path):
        print(
            "Error: missing required config file './kbuild.json'.\n"
            "Expected keys:\n"
            "  cmake (object, optional)\n"
            "  cmake.sdk (object, optional)\n"
            "  vcpkg (object, optional)\n"
            "Optional keys:\n"
            "  build-demos (array of demo names)\n"
            "  sdk-dependencies (array of dependency objects)",
            file=sys.stderr,
        )
        raise SystemExit(2)

    try:
        with open(config_path, "r", encoding="utf-8") as handle:
            raw = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        print(f"Error: could not parse {config_path}: {exc}", file=sys.stderr)
        raise SystemExit(2)

    if not isinstance(raw, dict):
        print("Error: kbuild.json must be a JSON object", file=sys.stderr)
        raise SystemExit(2)

    cmake_block = raw.get("cmake")
    if cmake_block is not None and not isinstance(cmake_block, dict):
        print("Error: kbuild.json key 'cmake' must be an object", file=sys.stderr)
        raise SystemExit(2)

    sdk_mode = False
    cmake_package_name = ""
    if isinstance(cmake_block, dict) and "sdk" in cmake_block:
        sdk_mode = True
        sdk_block = cmake_block.get("sdk")
        if not isinstance(sdk_block, dict):
            print("Error: kbuild.json key 'cmake.sdk' must be an object when defined", file=sys.stderr)
            raise SystemExit(2)
        package_name = sdk_block.get("package_name")
        if not isinstance(package_name, str) or not package_name.strip():
            print("Error: kbuild.json key 'cmake.sdk.package_name' must be a non-empty string", file=sys.stderr)
            raise SystemExit(2)
        cmake_package_name = package_name.strip()

    vcpkg_entry = raw.get("vcpkg")
    if "vcpkg" in raw and not isinstance(vcpkg_entry, dict):
        print("Error: kbuild.json key 'vcpkg' must be an object when defined", file=sys.stderr)
        raise SystemExit(2)
    requires_vcpkg = "vcpkg" in raw
    build_demos_raw = raw.get("build-demos", [])
    sdk_dependencies_raw = raw.get("sdk-dependencies", [])

    if sdk_mode and not cmake_package_name:
        print("Error: kbuild.json key 'cmake.sdk.package_name' must be a non-empty string", file=sys.stderr)
        raise SystemExit(2)
    if not isinstance(build_demos_raw, list):
        print("Error: kbuild.json key 'build-demos' must be an array if provided", file=sys.stderr)
        raise SystemExit(2)
    if not isinstance(sdk_dependencies_raw, list):
        print("Error: kbuild.json key 'sdk-dependencies' must be an array if provided", file=sys.stderr)
        raise SystemExit(2)

    build_demos: list[str] = []
    for idx, item in enumerate(build_demos_raw):
        if not isinstance(item, str) or not item.strip():
            print(
                f"Error: kbuild.json key 'build-demos[{idx}]' must be a non-empty string",
                file=sys.stderr,
            )
            raise SystemExit(2)
        build_demos.append(item.strip())

    sdk_dependencies: list[dict[str, object]] = []
    seen_dependency_packages: set[str] = set()
    for idx, item in enumerate(sdk_dependencies_raw):
        if not isinstance(item, dict):
            print(
                f"Error: kbuild.json key 'sdk-dependencies[{idx}]' must be an object",
                file=sys.stderr,
            )
            raise SystemExit(2)

        dep_package = item.get("cmake_package_name")
        dep_prefix_template = item.get("sdk_prefix")
        dep_fallbacks_raw = item.get("version_fallbacks", [])

        if not isinstance(dep_package, str) or not dep_package.strip():
            print(
                f"Error: kbuild.json key 'sdk-dependencies[{idx}].cmake_package_name' must be a non-empty string",
                file=sys.stderr,
            )
            raise SystemExit(2)
        dep_package = dep_package.strip()

        if cmake_package_name and dep_package == cmake_package_name:
            print(
                f"Error: kbuild.json sdk dependency '{dep_package}' cannot match root cmake.sdk.package_name",
                file=sys.stderr,
            )
            raise SystemExit(2)
        if dep_package in seen_dependency_packages:
            print(
                f"Error: duplicate sdk dependency package '{dep_package}' in kbuild.json",
                file=sys.stderr,
            )
            raise SystemExit(2)
        seen_dependency_packages.add(dep_package)

        if not isinstance(dep_prefix_template, str) or not dep_prefix_template.strip():
            print(
                f"Error: kbuild.json key 'sdk-dependencies[{idx}].sdk_prefix' must be a non-empty string",
                file=sys.stderr,
            )
            raise SystemExit(2)
        dep_prefix_template = dep_prefix_template.strip()

        if not isinstance(dep_fallbacks_raw, list):
            print(
                f"Error: kbuild.json key 'sdk-dependencies[{idx}].version_fallbacks' must be an array if provided",
                file=sys.stderr,
            )
            raise SystemExit(2)

        dep_fallbacks: list[str] = []
        for fallback_idx, fallback in enumerate(dep_fallbacks_raw):
            if not isinstance(fallback, str) or not fallback.strip():
                print(
                    f"Error: kbuild.json key 'sdk-dependencies[{idx}].version_fallbacks[{fallback_idx}]' must be a non-empty string",
                    file=sys.stderr,
                )
                raise SystemExit(2)
            dep_fallbacks.append(validate_version_slot(fallback))

        sdk_dependencies.append(
            {
                "cmake_package_name": dep_package,
                "sdk_prefix": dep_prefix_template,
                "version_fallbacks": dep_fallbacks,
            }
        )

    return cmake_package_name, requires_vcpkg, build_demos, sdk_dependencies


def clean_sdk_install_prefix(prefix: str) -> None:
    if os.path.isfile(prefix):
        print(f"Error: SDK install prefix is a file, expected directory: {prefix}", file=sys.stderr)
        raise SystemExit(2)

    os.makedirs(prefix, exist_ok=True)
    for entry in ("include", "lib", "bin", "share"):
        path = os.path.join(prefix, entry)
        if os.path.isdir(path):
            shutil.rmtree(path)
        elif os.path.exists(path):
            os.remove(path)


def validate_core_build_dir_layout(build_dir: str) -> None:
    normalized = build_dir.replace("\\", "/")
    while normalized.startswith("./"):
        normalized = normalized[2:]
    parts = [part for part in normalized.split("/") if part not in ("", ".")]
    if len(parts) < 2 or parts[0] != "build" or any(part == ".." for part in parts):
        fail("build directory must be under 'build/' (example: build/test/)")


def validate_version_slot(version: str) -> str:
    token = version.strip()
    if not token:
        fail("--version requires a non-empty value")
    if "/" in token or "\\" in token or token in (".", "..") or ".." in token:
        fail("--version must be a simple slot name (for example: latest or 0.1)")
    return token


def validate_sdk_prefix(prefix: str, cmake_package_name: str) -> None:
    if not os.path.isdir(prefix):
        print(
            "Error: SDK prefix must point to an existing directory.\n"
            f"Provided:\n  {prefix}",
            file=sys.stderr,
        )
        raise SystemExit(2)

    include_dir = os.path.join(prefix, "include")
    lib_dir = os.path.join(prefix, "lib")
    missing_dirs: list[str] = []
    if not os.path.isdir(include_dir):
        missing_dirs.append("include/")
    if not os.path.isdir(lib_dir):
        missing_dirs.append("lib/")
    if missing_dirs:
        print(
            "Error: SDK prefix is invalid; required SDK directories are missing.\n"
            f"Provided:\n  {prefix}\n"
            f"Missing:\n  {', '.join(missing_dirs)}",
            file=sys.stderr,
        )
        raise SystemExit(2)

    config_path = os.path.join(
        prefix,
        "lib",
        "cmake",
        cmake_package_name,
        f"{cmake_package_name}Config.cmake",
    )
    if os.path.isfile(config_path):
        return
    print(
        "Error: SDK prefix is missing package config.\n"
        f"Expected:\n  {config_path}\n"
        "Build/install SDK from a core build first (for example: ./kbuild.py --version test).",
        file=sys.stderr,
    )
    raise SystemExit(2)


def normalize_demo_name(demo_token: str) -> str:
    value = demo_token.strip().replace("\\", "/")
    while value.startswith("./"):
        value = value[2:]
    if value.startswith("demo/"):
        value = value[5:]
    if not value:
        fail(f"invalid demo '{demo_token}'")
    if value.startswith("/") or ".." in value.split("/"):
        fail(f"invalid demo '{demo_token}'")
    return value


def resolve_demo_source_dir(repo_root: str, demo_name: str) -> str:
    source_dir = os.path.join(repo_root, "demo", demo_name)
    cmake_lists = os.path.join(source_dir, "CMakeLists.txt")
    if not os.path.isfile(cmake_lists):
        fail(f"demo source directory is missing CMakeLists.txt: {source_dir}")
    return source_dir


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


def resolve_demo_vcpkg_context(sdk_prefix: str, repo_root: str) -> tuple[str, str]:
    candidate_build_dirs: list[str] = []
    if os.path.basename(sdk_prefix.rstrip("/\\")) == "sdk":
        candidate_build_dirs.append(os.path.abspath(os.path.dirname(sdk_prefix)))
    candidate_build_dirs.append(os.path.abspath(os.path.join(sdk_prefix, os.pardir)))

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
        "Error: could not infer vcpkg installed tree/triplet for demo build from SDK prefix.\n"
        f"SDK prefix:\n  {sdk_prefix}\n"
        "Expected a core build layout like:\n"
        "  build/<slot>/sdk\n"
        "  build/<slot>/installed/<triplet>\n"
        "You can also set VCPKG_INSTALLED_DIR and VCPKG_TARGET_TRIPLET explicitly.",
        file=sys.stderr,
    )
    raise SystemExit(2)


def resolve_sdk_dependencies(
    repo_root: str,
    version: str,
    dependency_specs: list[dict[str, object]],
) -> list[tuple[str, str]]:
    resolved: list[tuple[str, str]] = []

    for dependency in dependency_specs:
        package_name = dependency["cmake_package_name"]
        prefix_template = dependency["sdk_prefix"]
        fallback_versions = dependency["version_fallbacks"]

        if not isinstance(package_name, str) or not isinstance(prefix_template, str):
            print("Error: internal sdk dependency validation failure", file=sys.stderr)
            raise SystemExit(2)
        if not isinstance(fallback_versions, list):
            print("Error: internal sdk dependency validation failure", file=sys.stderr)
            raise SystemExit(2)

        candidate_slots: list[str] = [version]
        for fallback in fallback_versions:
            if isinstance(fallback, str) and fallback not in candidate_slots:
                candidate_slots.append(fallback)

        candidate_paths: list[tuple[str, str]] = []
        seen_paths: set[str] = set()
        for slot in candidate_slots:
            raw_path = prefix_template.replace("{version}", slot)
            candidate_prefix = resolve_prefix(raw_path, repo_root)
            if candidate_prefix in seen_paths:
                continue
            seen_paths.add(candidate_prefix)
            candidate_paths.append((slot, candidate_prefix))

        selected_prefix = ""
        selected_slot = ""
        for slot, candidate_prefix in candidate_paths:
            config_path = package_config_path(candidate_prefix, package_name)
            if os.path.isfile(config_path):
                selected_prefix = candidate_prefix
                selected_slot = slot
                break

        if not selected_prefix:
            checked = "\n".join(path for _, path in candidate_paths)
            print(
                "Error: sdk dependency package config not found.\n"
                f"Package:\n  {package_name}\n"
                "Checked SDK prefixes:\n"
                f"{checked}",
                file=sys.stderr,
            )
            raise SystemExit(2)

        if selected_slot != version and "{version}" in prefix_template:
            print(
                f"{package_name}: using fallback slot '{selected_slot}' (requested '{version}') -> {selected_prefix}",
                flush=True,
            )

        validate_sdk_prefix(selected_prefix, package_name)
        resolved.append((package_name, selected_prefix))

    return resolved


def local_vcpkg_paths(repo_root: str) -> tuple[str, str, str, str, str]:
    local_vcpkg_root = os.path.join(repo_root, "vcpkg", "src")
    local_toolchain = os.path.join(local_vcpkg_root, "scripts", "buildsystems", "vcpkg.cmake")
    local_vcpkg_build_root = os.path.join(repo_root, "vcpkg", "build")
    local_vcpkg_downloads = os.path.join(local_vcpkg_build_root, "downloads")
    local_vcpkg_binary_cache = os.path.join(local_vcpkg_build_root, "binary-cache")
    return (
        os.path.abspath(local_vcpkg_root),
        os.path.abspath(local_toolchain),
        os.path.abspath(local_vcpkg_build_root),
        os.path.abspath(local_vcpkg_downloads),
        os.path.abspath(local_vcpkg_binary_cache),
    )


def is_local_vcpkg_bootstrapped(vcpkg_root: str) -> bool:
    candidates = [
        os.path.join(vcpkg_root, "vcpkg"),
        os.path.join(vcpkg_root, "vcpkg.exe"),
        os.path.join(vcpkg_root, "vcpkg.bat"),
    ]
    return any(os.path.isfile(path) for path in candidates)


def run_vcpkg_bootstrap(vcpkg_root: str) -> None:
    if os.name == "nt":
        bootstrap = os.path.join(vcpkg_root, "bootstrap-vcpkg.bat")
        if not os.path.isfile(bootstrap):
            print(f"Error: missing bootstrap script: {bootstrap}", file=sys.stderr)
            raise SystemExit(2)
        run(["cmd", "/c", bootstrap, "-disableMetrics"])
        return

    bootstrap = os.path.join(vcpkg_root, "bootstrap-vcpkg.sh")
    if not os.path.isfile(bootstrap):
        print(f"Error: missing bootstrap script: {bootstrap}", file=sys.stderr)
        raise SystemExit(2)
    run([bootstrap, "-disableMetrics"])


def install_local_vcpkg(repo_root: str) -> tuple[str, str, str, str]:
    (
        local_vcpkg_root,
        local_toolchain,
        local_vcpkg_build_root,
        local_vcpkg_downloads,
        local_vcpkg_binary_cache,
    ) = local_vcpkg_paths(repo_root)

    vcpkg_parent = os.path.dirname(local_vcpkg_root)
    if os.path.isfile(vcpkg_parent):
        print(f"Error: expected directory at {vcpkg_parent}, found file", file=sys.stderr)
        raise SystemExit(2)
    os.makedirs(vcpkg_parent, exist_ok=True)

    if not os.path.isdir(local_vcpkg_root):
        print(f"Installing vcpkg checkout -> {local_vcpkg_root}", flush=True)
        run(["git", "clone", VCPKG_REPO_URL, local_vcpkg_root])

    if not os.path.isfile(local_toolchain):
        print(
            "Error: vcpkg checkout is invalid; missing toolchain file.\n"
            f"Expected:\n  {local_toolchain}",
            file=sys.stderr,
        )
        raise SystemExit(2)

    if not is_local_vcpkg_bootstrapped(local_vcpkg_root):
        print("Bootstrapping vcpkg...", flush=True)
        run_vcpkg_bootstrap(local_vcpkg_root)

    if os.path.isfile(local_vcpkg_build_root):
        print(f"Error: expected directory at {local_vcpkg_build_root}, found file", file=sys.stderr)
        raise SystemExit(2)
    os.makedirs(local_vcpkg_downloads, exist_ok=True)
    os.makedirs(local_vcpkg_binary_cache, exist_ok=True)

    print(
        f"vcpkg ready -> src={local_vcpkg_root} | build={local_vcpkg_build_root}",
        flush=True,
    )
    return local_vcpkg_root, local_toolchain, local_vcpkg_downloads, local_vcpkg_binary_cache


def read_git_head_commit(repo_path: str) -> str:
    result = subprocess.run(
        ["git", "-C", repo_path, "rev-parse", "HEAD"],
        check=False,
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        detail = result.stderr.strip() or result.stdout.strip() or "git rev-parse failed"
        print(
            "Error: could not read vcpkg baseline commit from ./vcpkg/src.\n"
            f"Details:\n  {detail}",
            file=sys.stderr,
        )
        raise SystemExit(2)

    commit = result.stdout.strip()
    if not re.fullmatch(r"[0-9a-fA-F]{40}", commit):
        print(
            "Error: unexpected git commit format from ./vcpkg/src HEAD.\n"
            f"Value:\n  {commit}",
            file=sys.stderr,
        )
        raise SystemExit(2)
    return commit.lower()


def load_json_object(path: str) -> dict[str, object]:
    if not os.path.isfile(path):
        print(f"Error: missing required JSON file: {path}", file=sys.stderr)
        raise SystemExit(2)
    try:
        with open(path, "r", encoding="utf-8") as handle:
            payload = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        print(f"Error: could not parse {path}: {exc}", file=sys.stderr)
        raise SystemExit(2)
    if not isinstance(payload, dict):
        print(f"Error: expected JSON object in {path}", file=sys.stderr)
        raise SystemExit(2)
    return payload


def write_json_object(path: str, payload: dict[str, object]) -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        json.dump(payload, handle, indent=2)
        handle.write("\n")


def create_kbuild_config_template(repo_root: str) -> int:
    config_path = os.path.join(repo_root, "kbuild.json")
    if os.path.exists(config_path):
        print("Error: './kbuild.json' already exists.", file=sys.stderr)
        return 2

    payload = {
        "project_title": "My Project Title",
        "project_id": "myproject",
        "git": {
            "url": "https://github.com/your-org/your-repo",
            "auth": "git@github.com:your-org/your-repo.git",
        },
        "cmake": {
            "minimum_version": "3.20",
            "sdk": {
                "package_name": "MyPackageNameSDK",
            },
        },
        "vcpkg": {
            "dependencies": [],
        },
    }
    write_json_object(config_path, payload)
    print("Created ./kbuild.json template.", flush=True)
    return 0


def load_git_urls(repo_root: str) -> tuple[str, str]:
    config_path = os.path.join(repo_root, "kbuild.json")
    raw = load_json_object(config_path)

    git_raw = raw.get("git")
    if not isinstance(git_raw, dict):
        print("Error: kbuild.json key 'git' must be an object", file=sys.stderr)
        raise SystemExit(2)

    url_raw = git_raw.get("url")
    if not isinstance(url_raw, str) or not url_raw.strip():
        print("Error: kbuild.json key 'git.url' must be a non-empty string", file=sys.stderr)
        raise SystemExit(2)
    auth_raw = git_raw.get("auth")
    if not isinstance(auth_raw, str) or not auth_raw.strip():
        print("Error: kbuild.json key 'git.auth' must be a non-empty string", file=sys.stderr)
        raise SystemExit(2)
    return url_raw.strip(), auth_raw.strip()


def verify_remote_repo_access(repo_url: str, auth_url: str) -> None:
    env = os.environ.copy()
    env["GIT_TERMINAL_PROMPT"] = "0"
    result = subprocess.run(
        ["git", "ls-remote", repo_url],
        check=False,
        capture_output=True,
        text=True,
        env=env,
    )
    if result.returncode == 0:
        pass
    else:
        print(
            f"\nError: Could not reach\n  {repo_url}\n\n"
            "This is most likely due to one of the following reasons:\n"
            "  (1) There is a typo in the git repo specified in kbuild.json (git.url).\n"
            "  (2) You have not created the remote repo.\n"
            "  (3) You do not have network access.\n",
            file=sys.stderr,
        )
        raise SystemExit(2)

    with tempfile.TemporaryDirectory(prefix="kbuild-auth-probe-") as probe_root:
        init_result = subprocess.run(
            ["git", "init", probe_root],
            check=False,
            capture_output=True,
            text=True,
        )
        if init_result.returncode != 0:
            detail = init_result.stderr.strip() or init_result.stdout.strip() or "git init failed"
            print(
                "Error: failed to run git authentication preflight.\n"
                f"Detail:\n  {detail}",
                file=sys.stderr,
            )
            raise SystemExit(2)

        config_name_result = subprocess.run(
            ["git", "-C", probe_root, "config", "user.name", "kbuild-auth-probe"],
            check=False,
            capture_output=True,
            text=True,
        )
        if config_name_result.returncode != 0:
            detail = (
                config_name_result.stderr.strip()
                or config_name_result.stdout.strip()
                or "git config user.name failed"
            )
            print(
                "Error: failed to run git authentication preflight.\n"
                f"Detail:\n  {detail}",
                file=sys.stderr,
            )
            raise SystemExit(2)

        config_email_result = subprocess.run(
            ["git", "-C", probe_root, "config", "user.email", "kbuild-auth-probe@example.invalid"],
            check=False,
            capture_output=True,
            text=True,
        )
        if config_email_result.returncode != 0:
            detail = (
                config_email_result.stderr.strip()
                or config_email_result.stdout.strip()
                or "git config user.email failed"
            )
            print(
                "Error: failed to run git authentication preflight.\n"
                f"Detail:\n  {detail}",
                file=sys.stderr,
            )
            raise SystemExit(2)

        probe_file = os.path.join(probe_root, ".kbuild-auth-probe")
        try:
            with open(probe_file, "w", encoding="utf-8", newline="\n") as handle:
                handle.write("probe\n")
        except OSError as exc:
            print(
                "Error: failed to run git authentication preflight.\n"
                f"Detail:\n  {exc}",
                file=sys.stderr,
            )
            raise SystemExit(2)

        add_result = subprocess.run(
            ["git", "-C", probe_root, "add", ".kbuild-auth-probe"],
            check=False,
            capture_output=True,
            text=True,
        )
        if add_result.returncode != 0:
            detail = add_result.stderr.strip() or add_result.stdout.strip() or "git add failed"
            print(
                "Error: failed to run git authentication preflight.\n"
                f"Detail:\n  {detail}",
                file=sys.stderr,
            )
            raise SystemExit(2)

        commit_result = subprocess.run(
            ["git", "-C", probe_root, "commit", "-m", "kbuild auth probe"],
            check=False,
            capture_output=True,
            text=True,
        )
        if commit_result.returncode != 0:
            detail = commit_result.stderr.strip() or commit_result.stdout.strip() or "git commit failed"
            print(
                "Error: failed to run git authentication preflight.\n"
                f"Detail:\n  {detail}",
                file=sys.stderr,
            )
            raise SystemExit(2)

        push_result = subprocess.run(
            [
                "git",
                "-C",
                probe_root,
                "push",
                "--dry-run",
                auth_url,
                "HEAD:refs/heads/kbuild-auth-probe",
            ],
            check=False,
            capture_output=True,
            text=True,
            env=env,
        )
        if push_result.returncode != 0:
            print(
                f"\nError: Authentication failed for\n  {auth_url}\n\n"
                "This is most likely due to one of the following reasons:\n"
                "  (1) Your git credentials for this host are missing, expired, or invalid.\n"
                "  (2) You do not have push permission for this repository.\n"
                "  (3) Your credential helper is not configured for non-interactive use.\n",
                file=sys.stderr,
            )
            raise SystemExit(2)


def initialize_git_repo(repo_root: str, repo_url: str, auth_url: str) -> int:
    verify_remote_repo_access(repo_url, auth_url)

    inside_worktree = subprocess.run(
        ["git", "-C", repo_root, "rev-parse", "--is-inside-work-tree"],
        check=False,
        capture_output=True,
        text=True,
    )
    if inside_worktree.returncode == 0 and inside_worktree.stdout.strip().lower() == "true":
        print("Error: current directory is already inside a git worktree.", file=sys.stderr)
        raise SystemExit(2)

    git_dir = os.path.join(repo_root, ".git")
    if os.path.exists(git_dir):
        print("Error: './.git' already exists.", file=sys.stderr)
        raise SystemExit(2)

    run(["git", "init", repo_root])
    run(["git", "-C", repo_root, "branch", "-M", "main"])

    remote_check = subprocess.run(
        ["git", "-C", repo_root, "remote", "get-url", "origin"],
        check=False,
        capture_output=True,
        text=True,
    )
    if remote_check.returncode == 0:
        run(["git", "-C", repo_root, "remote", "set-url", "origin", auth_url])
        remote_action = "updated"
    else:
        run(["git", "-C", repo_root, "remote", "add", "origin", auth_url])
        remote_action = "added"

    run(["git", "-C", repo_root, "add", "-A"])

    commit_result = subprocess.run(
        ["git", "-C", repo_root, "commit", "-m", "Initial scaffold"],
        check=False,
        capture_output=True,
        text=True,
    )
    if commit_result.returncode != 0:
        detail = commit_result.stderr.strip() or commit_result.stdout.strip() or "git commit failed"
        print(
            "Error: failed to create initial commit.\n"
            "Configure git identity (user.name/user.email) and retry.\n"
            f"Detail:\n  {detail}",
            file=sys.stderr,
        )
        raise SystemExit(2)

    push_env = os.environ.copy()
    push_env["GIT_TERMINAL_PROMPT"] = "0"
    push_result = subprocess.run(
        ["git", "-C", repo_root, "push", "-u", "origin", "main"],
        check=False,
        capture_output=True,
        text=True,
        env=push_env,
    )
    if push_result.returncode != 0:
        detail = push_result.stderr.strip() or push_result.stdout.strip() or "git push failed"
        print(
            "Error: failed to push initial commit to remote.\n"
            "Ensure the remote exists and git authentication is configured.\n"
            f"Detail:\n  {detail}",
            file=sys.stderr,
        )
        raise SystemExit(2)

    print("Initialized git repository:", flush=True)
    print("  branch: main", flush=True)
    print(f"  remote origin ({remote_action}): {auth_url}", flush=True)
    print("  initial commit: created", flush=True)
    print("  push: origin/main", flush=True)
    return 0


def git_sync(repo_root: str, commit_message: str) -> int:
    worktree_check = subprocess.run(
        ["git", "-C", repo_root, "rev-parse", "--is-inside-work-tree"],
        check=False,
        capture_output=True,
        text=True,
    )
    if worktree_check.returncode != 0 or worktree_check.stdout.strip().lower() != "true":
        print(
            "Error: git repository is not initialized. Run `./kbuild.py --initialize-git`.",
            file=sys.stderr,
        )
        raise SystemExit(2)

    add_result = subprocess.run(["git", "-C", repo_root, "add", "."], check=False)
    if add_result.returncode != 0:
        print("Error: git add failed.", file=sys.stderr)
        raise SystemExit(2)

    commit_result = subprocess.run(
        ["git", "-C", repo_root, "commit", "-m", commit_message],
        check=False,
    )
    if commit_result.returncode != 0:
        print("Error: git commit failed.", file=sys.stderr)
        raise SystemExit(2)

    push_result = subprocess.run(["git", "-C", repo_root, "push"], check=False)
    if push_result.returncode != 0:
        print("Error: git push failed.", file=sys.stderr)
        raise SystemExit(2)

    print("Git sync complete.", flush=True)
    return 0


def sync_vcpkg_baseline(repo_root: str) -> str:
    local_vcpkg_root, _, _, _, _ = local_vcpkg_paths(repo_root)
    if not os.path.isdir(local_vcpkg_root):
        print(
            "Error: missing vcpkg checkout under ./vcpkg/src.\n"
            "Run:\n"
            "  ./kbuild.py --install-vcpkg",
            file=sys.stderr,
        )
        raise SystemExit(2)

    baseline = read_git_head_commit(local_vcpkg_root)

    manifest_path = os.path.join(repo_root, "vcpkg", "vcpkg.json")
    configuration_path = os.path.join(repo_root, "vcpkg", "vcpkg-configuration.json")

    manifest = load_json_object(manifest_path)
    configuration = load_json_object(configuration_path)

    old_manifest_baseline = manifest.get("builtin-baseline")
    manifest["builtin-baseline"] = baseline

    registry = configuration.get("default-registry")
    if registry is None:
        registry_obj: dict[str, object] = {}
        configuration["default-registry"] = registry_obj
    elif isinstance(registry, dict):
        registry_obj = registry
    else:
        print(
            "Error: vcpkg-configuration.json key 'default-registry' must be an object",
            file=sys.stderr,
        )
        raise SystemExit(2)
    if "kind" not in registry_obj:
        registry_obj["kind"] = "builtin"
    old_config_baseline = registry_obj.get("baseline")
    registry_obj["baseline"] = baseline

    write_json_object(manifest_path, manifest)
    write_json_object(configuration_path, configuration)

    old_manifest_text = (
        old_manifest_baseline.strip()
        if isinstance(old_manifest_baseline, str) and old_manifest_baseline.strip()
        else "<unset>"
    )
    old_config_text = (
        old_config_baseline.strip()
        if isinstance(old_config_baseline, str) and old_config_baseline.strip()
        else "<unset>"
    )
    print(f"vcpkg baseline synced -> {baseline}", flush=True)
    print(
        f"  ./vcpkg/vcpkg.json: builtin-baseline {old_manifest_text} -> {baseline}",
        flush=True,
    )
    print(
        "  ./vcpkg/vcpkg-configuration.json: "
        f"default-registry.baseline {old_config_text} -> {baseline}",
        flush=True,
    )
    return baseline


def ensure_local_vcpkg(repo_root: str) -> tuple[str, str, str, str]:
    (
        local_vcpkg_root,
        local_toolchain,
        local_vcpkg_build_root,
        local_vcpkg_downloads,
        local_vcpkg_binary_cache,
    ) = local_vcpkg_paths(repo_root)

    ready = (
        os.path.isdir(local_vcpkg_root)
        and os.path.isdir(local_vcpkg_build_root)
        and os.path.isfile(local_toolchain)
        and is_local_vcpkg_bootstrapped(local_vcpkg_root)
    )
    if not ready:
        print(
            "Error: vcpkg has not been set up. Run `./kbuild.py --install-vcpkg`",
            file=sys.stderr,
        )
        raise SystemExit(2)

    os.makedirs(local_vcpkg_downloads, exist_ok=True)
    os.makedirs(local_vcpkg_binary_cache, exist_ok=True)
    return local_vcpkg_root, local_toolchain, local_vcpkg_downloads, local_vcpkg_binary_cache


def build_demo(
    repo_root: str,
    demo_name: str,
    version: str,
    no_configure: bool,
    cmake_package_name: str,
    sdk_dependencies: list[tuple[str, str]],
    env: dict[str, str],
    demo_order: list[str],
) -> None:
    core_build_dir = os.path.join(repo_root, "build", version)
    core_sdk_prefix = os.path.join(core_build_dir, "sdk")
    validate_sdk_prefix(core_sdk_prefix, cmake_package_name)

    demo_vcpkg_installed_dir, demo_vcpkg_triplet = resolve_demo_vcpkg_context(
        core_sdk_prefix, repo_root
    )
    demo_vcpkg_prefix = os.path.join(demo_vcpkg_installed_dir, demo_vcpkg_triplet)
    if not os.path.isdir(demo_vcpkg_prefix):
        fail(f"missing vcpkg triplet prefix: {demo_vcpkg_prefix}")

    source_dir = resolve_demo_source_dir(repo_root, demo_name)
    build_dir = os.path.join(repo_root, "demo", demo_name, "build", version)
    install_prefix = os.path.join(build_dir, "sdk")

    prefix_entries: list[str] = [core_sdk_prefix, demo_vcpkg_prefix]
    for _, dependency_prefix in sdk_dependencies:
        if dependency_prefix not in prefix_entries:
            prefix_entries.append(dependency_prefix)
    for dependency_demo in demo_order:
        dependency_sdk = os.path.join(repo_root, "demo", dependency_demo, "build", version, "sdk")
        if os.path.isdir(dependency_sdk) and dependency_sdk not in prefix_entries:
            prefix_entries.append(dependency_sdk)

    cmake_args = [
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_PREFIX_PATH={';'.join(prefix_entries)}",
        "-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON",
        f"-D{cmake_package_name}_DIR={package_dir(core_sdk_prefix, cmake_package_name)}",
    ]
    for package_name, dependency_prefix in sdk_dependencies:
        cmake_args.append(f"-D{package_name}_DIR={package_dir(dependency_prefix, package_name)}")
    print(
        f"Demo build -> dir={build_dir} | demo={demo_name} | sdk={core_sdk_prefix} | triplet={demo_vcpkg_triplet}",
        flush=True,
    )

    if no_configure:
        cache_path = os.path.join(build_dir, "CMakeCache.txt")
        if not os.path.isfile(cache_path):
            fail(f"--no-configure requires an existing CMakeCache.txt in the build directory ({build_dir})")
    else:
        os.makedirs(build_dir, exist_ok=True)
        run(["cmake", "-S", source_dir, "-B", build_dir, *cmake_args], env=env)

    run(["cmake", "--build", build_dir, "-j4"], env=env)
    clean_sdk_install_prefix(install_prefix)
    run(
        [
            "cmake",
            "--install",
            build_dir,
            "--prefix",
            install_prefix,
        ],
        env=env,
    )
    print(f"Build complete -> dir={build_dir} | sdk={install_prefix}")


def main() -> int:
    repo_root = enforce_script_directory()
    args = sys.argv[1:]
    version = "latest"
    version_explicit = False
    no_configure = False
    create_config = False
    install_vcpkg = False
    sync_vcpkg_baseline_only = False
    build_demos = False
    list_builds = False
    remove_latest_builds = False
    initialize_repo = False
    initialize_git = False
    git_sync_requested = False
    git_sync_message = ""
    requested_demos: list[str] = []

    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ("-h", "--help"):
            usage(0)
        elif arg == "--create-config":
            create_config = True
        elif arg == "--list-builds":
            list_builds = True
        elif arg == "--remove-latest":
            remove_latest_builds = True
        elif arg == "--initialize-repo":
            initialize_repo = True
        elif arg == "--initialize-git":
            initialize_git = True
        elif arg == "--git-sync":
            git_sync_requested = True
            i += 1
            if i >= len(args):
                fail("missing value for '--git-sync'")
            git_sync_message = args[i].strip()
            if not git_sync_message:
                fail("--git-sync requires a non-empty commit message")
        elif arg == "--sync-vcpkg-baseline":
            sync_vcpkg_baseline_only = True
        elif arg == "--version":
            i += 1
            if i >= len(args):
                fail("missing value for '--version'")
            version = validate_version_slot(args[i])
            version_explicit = True
        elif arg == "--build-demos":
            build_demos = True
            i += 1
            while i < len(args) and not args[i].startswith("-"):
                requested_demos.append(args[i])
                i += 1
            continue
        elif arg == "--no-configure":
            no_configure = True
        elif arg == "--install-vcpkg":
            install_vcpkg = True
        elif arg.startswith("-"):
            fail(f"unknown option '{arg}'")
        else:
            fail(f"unexpected positional argument '{arg}'; use --version <name>")
        i += 1

    build_mode_flags: list[str] = []
    if version_explicit:
        build_mode_flags.append("--version")
    if build_demos:
        build_mode_flags.append("--build-demos")
    if no_configure:
        build_mode_flags.append("--no-configure")
    if install_vcpkg:
        build_mode_flags.append("--install-vcpkg")

    if create_config and (
        list_builds
        or remove_latest_builds
        or initialize_repo
        or initialize_git
        or git_sync_requested
        or sync_vcpkg_baseline_only
        or bool(build_mode_flags)
    ):
        fail("--create-config cannot be combined with other options")

    if list_builds and remove_latest_builds:
        fail("--list-builds and --remove-latest cannot be combined")
    if list_builds and build_mode_flags:
        fail("--list-builds cannot be combined with build options")
    if remove_latest_builds and build_mode_flags:
        fail("--remove-latest cannot be combined with build options")
    if initialize_repo and (list_builds or remove_latest_builds or build_mode_flags):
        fail("--initialize-repo cannot be combined with other options")
    if initialize_git and (
        list_builds
        or remove_latest_builds
        or initialize_repo
        or git_sync_requested
        or sync_vcpkg_baseline_only
        or bool(build_mode_flags)
    ):
        fail("--initialize-git cannot be combined with other options")
    if git_sync_requested and (
        list_builds
        or remove_latest_builds
        or initialize_repo
        or initialize_git
        or sync_vcpkg_baseline_only
        or bool(build_mode_flags)
    ):
        fail("--git-sync cannot be combined with other options")
    if sync_vcpkg_baseline_only and (
        list_builds
        or remove_latest_builds
        or initialize_repo
        or initialize_git
        or git_sync_requested
        or build_mode_flags
    ):
        fail("--sync-vcpkg-baseline cannot be combined with other options")

    config_path = os.path.join(repo_root, "kbuild.json")
    if not os.path.isfile(config_path):
        if create_config:
            return create_kbuild_config_template(repo_root)
        print(
            "Error: 'kbuild.json' does not exist.\n"
            "Run `./kbuild.py --create-config` to create a template.",
            file=sys.stderr,
        )
        return 2
    if create_config:
        print("Error: './kbuild.json' already exists.", file=sys.stderr)
        return 2

    if initialize_repo:
        return initialize_repo_layout(repo_root)
    if initialize_git:
        git_url, git_auth = load_git_urls(repo_root)
        return initialize_git_repo(repo_root, git_url, git_auth)
    if git_sync_requested:
        return git_sync(repo_root, git_sync_message)
    allowed_entries = {"kbuild.py", "kbuild.json"}
    if all(entry in allowed_entries for entry in os.listdir(repo_root)):
        print("Empty directory. Run `./kbuild.py --initialize-repo` to initialize.", file=sys.stderr)
        return 2
    if sync_vcpkg_baseline_only:
        sync_vcpkg_baseline(repo_root)
        return 0
    if remove_latest_builds:
        return remove_latest_build_dirs(repo_root)
    if list_builds:
        return list_build_dirs(repo_root)

    cmake_package_name, requires_vcpkg, config_build_demos, config_sdk_dependencies = load_kbuild_config(repo_root)
    sdk_dependencies = resolve_sdk_dependencies(repo_root, version, config_sdk_dependencies)

    demo_order: list[str] = []
    if build_demos:
        if not cmake_package_name:
            fail(
                "--build-demos requires SDK metadata; define cmake.sdk.package_name in kbuild.json"
            )
        if requested_demos:
            demo_order = [normalize_demo_name(token) for token in requested_demos]
        else:
            if not config_build_demos:
                fail("kbuild.json must define 'build-demos' for --build-demos with no demo arguments")
            demo_order = [normalize_demo_name(token) for token in config_build_demos]

        # Validate all requested demo source directories before core build work.
        for demo_name in demo_order:
            resolve_demo_source_dir(repo_root, demo_name)

    if install_vcpkg:
        install_local_vcpkg(repo_root)
        sync_vcpkg_baseline(repo_root)

    build_dir = os.path.join("build", version)

    source_dir = repo_root
    cmake_args = ["-DCMAKE_BUILD_TYPE=Release"]
    if sdk_dependencies:
        prefix_entries = [dependency_prefix for _, dependency_prefix in sdk_dependencies]
        cmake_args.extend(
            [
                f"-DCMAKE_PREFIX_PATH={';'.join(prefix_entries)}",
                "-DCMAKE_FIND_PACKAGE_PREFER_CONFIG=ON",
            ]
        )
        for package_name, dependency_prefix in sdk_dependencies:
            cmake_args.append(f"-D{package_name}_DIR={package_dir(dependency_prefix, package_name)}")

    validate_core_build_dir_layout(build_dir)

    env = os.environ.copy()
    if requires_vcpkg:
        local_vcpkg_root, local_toolchain, local_vcpkg_downloads, local_vcpkg_binary_cache = (
            ensure_local_vcpkg(repo_root)
        )
        env["VCPKG_ROOT"] = local_vcpkg_root
        if not env.get("VCPKG_DOWNLOADS", "").strip():
            env["VCPKG_DOWNLOADS"] = local_vcpkg_downloads
        if not env.get("VCPKG_DEFAULT_BINARY_CACHE", "").strip():
            env["VCPKG_DEFAULT_BINARY_CACHE"] = local_vcpkg_binary_cache
        cmake_args.append(f"-DCMAKE_TOOLCHAIN_FILE={local_toolchain}")

    if no_configure:
        cache_path = os.path.join(build_dir, "CMakeCache.txt")
        if not os.path.isfile(cache_path):
            fail("--no-configure requires an existing CMakeCache.txt in the build directory")
    else:
        os.makedirs(build_dir, exist_ok=True)
        run(["cmake", "-S", source_dir, "-B", build_dir, *cmake_args], env=env)

    run(["cmake", "--build", build_dir, "-j4"], env=env)

    install_prefix = os.path.abspath(os.path.join(build_dir, "sdk"))
    clean_sdk_install_prefix(install_prefix)
    run(
        [
            "cmake",
            "--install",
            build_dir,
            "--prefix",
            install_prefix,
        ],
        env=env,
    )

    print(f"Build complete -> dir={build_dir} | sdk={install_prefix}")

    if build_demos:
        for demo_name in demo_order:
            build_demo(
                repo_root=repo_root,
                demo_name=demo_name,
                version=version,
                no_configure=no_configure,
                cmake_package_name=cmake_package_name,
                sdk_dependencies=sdk_dependencies,
                env=env,
                demo_order=demo_order,
            )

    return 0


def load_initialize_repo_config(repo_root: str) -> dict[str, object]:
    config_path = os.path.join(repo_root, "kbuild.json")
    if not os.path.isfile(config_path):
        print("Error: missing required config file './kbuild.json'", file=sys.stderr)
        raise SystemExit(2)

    try:
        with open(config_path, "r", encoding="utf-8") as handle:
            raw = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        print(f"Error: could not parse {config_path}: {exc}", file=sys.stderr)
        raise SystemExit(2)

    if not isinstance(raw, dict):
        print("Error: kbuild.json must be a JSON object", file=sys.stderr)
        raise SystemExit(2)

    project_title_raw = raw.get("project_title")
    if not isinstance(project_title_raw, str) or not project_title_raw.strip():
        print("Error: kbuild.json key 'project_title' must be a non-empty string", file=sys.stderr)
        raise SystemExit(2)
    project_title = project_title_raw.strip()

    project_id_raw = raw.get("project_id")
    if not isinstance(project_id_raw, str) or not project_id_raw.strip():
        print("Error: kbuild.json key 'project_id' must be a non-empty string", file=sys.stderr)
        raise SystemExit(2)
    project_id = project_id_raw.strip()
    if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]*", project_id):
        print(
            "Error: kbuild.json key 'project_id' must be a valid C/C++ identifier",
            file=sys.stderr,
        )
        raise SystemExit(2)

    git_raw = raw.get("git")
    if not isinstance(git_raw, dict):
        print("Error: kbuild.json key 'git' must be an object", file=sys.stderr)
        raise SystemExit(2)
    git_url_raw = git_raw.get("url")
    if not isinstance(git_url_raw, str) or not git_url_raw.strip():
        print("Error: kbuild.json key 'git.url' must be a non-empty string", file=sys.stderr)
        raise SystemExit(2)
    git_auth_raw = git_raw.get("auth")
    if not isinstance(git_auth_raw, str) or not git_auth_raw.strip():
        print("Error: kbuild.json key 'git.auth' must be a non-empty string", file=sys.stderr)
        raise SystemExit(2)
    git_url = git_url_raw.strip()
    git_auth = git_auth_raw.strip()

    cmake_raw = raw.get("cmake")
    if not isinstance(cmake_raw, dict):
        print("Error: kbuild.json key 'cmake' must be an object", file=sys.stderr)
        raise SystemExit(2)

    cmake_minimum_version_raw = cmake_raw.get("minimum_version", "3.20")
    if not isinstance(cmake_minimum_version_raw, str) or not cmake_minimum_version_raw.strip():
        print("Error: kbuild.json key 'cmake.minimum_version' must be a non-empty string", file=sys.stderr)
        raise SystemExit(2)
    cmake_minimum_version = cmake_minimum_version_raw.strip()

    sdk_enabled = False
    sdk_package_name = ""
    if "sdk" in cmake_raw:
        sdk_raw = cmake_raw.get("sdk")
        if not isinstance(sdk_raw, dict):
            print("Error: kbuild.json key 'cmake.sdk' must be an object when defined", file=sys.stderr)
            raise SystemExit(2)
        sdk_package_name_raw = sdk_raw.get("package_name")
        if not isinstance(sdk_package_name_raw, str) or not sdk_package_name_raw.strip():
            print(
                "Error: kbuild.json key 'cmake.sdk.package_name' must be a non-empty string",
                file=sys.stderr,
            )
            raise SystemExit(2)
        sdk_enabled = True
        sdk_package_name = sdk_package_name_raw.strip()

    vcpkg_raw = raw.get("vcpkg")
    if not isinstance(vcpkg_raw, dict):
        print("Error: kbuild.json key 'vcpkg' must be an object", file=sys.stderr)
        raise SystemExit(2)

    dependencies_raw = vcpkg_raw.get("dependencies", [])
    if not isinstance(dependencies_raw, list):
        print("Error: kbuild.json key 'vcpkg.dependencies' must be an array", file=sys.stderr)
        raise SystemExit(2)
    vcpkg_dependencies: list[str] = []
    for idx, dep in enumerate(dependencies_raw):
        if not isinstance(dep, str) or not dep.strip():
            print(
                f"Error: kbuild.json key 'vcpkg.dependencies[{idx}]' must be a non-empty string",
                file=sys.stderr,
            )
            raise SystemExit(2)
        vcpkg_dependencies.append(dep.strip())

    return {
        "project_title": project_title,
        "project_id": project_id,
        "git_url": git_url,
        "git_auth": git_auth,
        "cmake_minimum_version": cmake_minimum_version,
        "sdk_enabled": sdk_enabled,
        "sdk_package_name": sdk_package_name,
        "vcpkg_dependencies": vcpkg_dependencies,
    }


def format_path_for_output(path: str, repo_root: str) -> str:
    rel = os.path.relpath(path, repo_root).replace("\\", "/").strip("/")
    return f"./{rel}"


def ensure_directory_for_init(path: str) -> bool:
    if os.path.isdir(path):
        return False
    if os.path.exists(path):
        print(f"Error: expected directory path is occupied by a non-directory: {path}", file=sys.stderr)
        raise SystemExit(2)
    os.makedirs(path, exist_ok=True)
    return True


def ensure_initialize_repo_root_empty(repo_root: str) -> None:
    allowed_entries = {"kbuild.py", "kbuild.json"}
    unexpected_entries = sorted(entry for entry in os.listdir(repo_root) if entry not in allowed_entries)
    if not unexpected_entries:
        return

    print(
        "Error: --initialize-repo must be run from an empty directory "
        "(other than kbuild.json and kbuild.py).",
        file=sys.stderr,
    )
    print("Found:", file=sys.stderr)
    for entry in unexpected_entries:
        print(f"  {entry}", file=sys.stderr)
    raise SystemExit(2)


def write_file_for_init(path: str, content: str) -> None:
    if os.path.isdir(path):
        print(f"Error: expected file path is occupied by a directory: {path}", file=sys.stderr)
        raise SystemExit(2)
    if os.path.exists(path):
        print(f"Error: refusing to overwrite existing file: {path}", file=sys.stderr)
        raise SystemExit(2)
    parent = os.path.dirname(path)
    if parent:
        os.makedirs(parent, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as handle:
        handle.write(content)


def initialize_repo_layout(repo_root: str) -> int:
    config = load_initialize_repo_config(repo_root)
    ensure_initialize_repo_root_empty(repo_root)

    project_title = str(config["project_title"])
    project_id = str(config["project_id"])
    cmake_minimum_version = str(config["cmake_minimum_version"])
    sdk_enabled = bool(config["sdk_enabled"])
    sdk_package_name = str(config["sdk_package_name"])
    vcpkg_dependencies = list(config["vcpkg_dependencies"])

    created_dirs: list[str] = []
    created_files: list[str] = []

    directory_order = [
        os.path.join(repo_root, "agent"),
        os.path.join(repo_root, "agent", "projects"),
        os.path.join(repo_root, "cmake"),
        os.path.join(repo_root, "demo"),
        os.path.join(repo_root, "src"),
        os.path.join(repo_root, "tests"),
        os.path.join(repo_root, "vcpkg"),
    ]
    if sdk_enabled:
        directory_order.extend(
            [
                os.path.join(repo_root, "include"),
                os.path.join(repo_root, "include", project_id),
            ]
        )

    for path in directory_order:
        if ensure_directory_for_init(path):
            created_dirs.append(path)

    cmake_lists_content = (
        f"cmake_minimum_required(VERSION {cmake_minimum_version})\n\n"
        f"project({project_id} LANGUAGES CXX)\n\n"
        "set(CMAKE_CXX_STANDARD 20)\n"
        "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        "set(CMAKE_CXX_EXTENSIONS OFF)\n"
    )

    readme_content = f"# {project_title}\n\n##Overview\n"

    bootstrap_content = (
        "# Coding Agent Bootstrap Instructions\n\n"
        "## Preparation\n\n"
        "Read the following files:\n"
        "- README.md\n"
    )

    src_cpp_lines: list[str] = []
    if sdk_enabled:
        src_cpp_lines.extend(
            [
                f"#include <{project_id}.hpp>",
                "",
            ]
        )
    src_cpp_lines.extend(
        [
            f"namespace {project_id} {{",
            f"}}  // namespace {project_id}",
            "",
        ]
    )
    src_cpp_content = "\n".join(src_cpp_lines)

    if sdk_enabled:
        vcpkg_json_payload: dict[str, object] = {
            "name": project_id,
            "dependencies": vcpkg_dependencies,
        }
    else:
        vcpkg_json_payload = {
            "dependencies": vcpkg_dependencies,
        }
    vcpkg_json_content = f"{json.dumps(vcpkg_json_payload, indent=2)}\n"

    vcpkg_configuration_payload = {
        "default-registry": {
            "kind": "builtin",
        }
    }
    vcpkg_configuration_content = f"{json.dumps(vcpkg_configuration_payload, indent=2)}\n"
    gitignore_content = (
        "# Build directories\n"
        "/build/\n"
        "/demo/**/build/\n\n"
        "# vcpkg\n"
        "/vcpkg/src/\n"
        "/vcpkg/build/\n\n"
        "# Python caches\n"
        "__pycache__/\n"
        "*.pyc\n\n"
    )

    files_to_write: list[tuple[str, str]] = [
        (os.path.join(repo_root, "CMakeLists.txt"), cmake_lists_content),
        (os.path.join(repo_root, "README.md"), readme_content),
        (os.path.join(repo_root, ".gitignore"), gitignore_content),
        (os.path.join(repo_root, "agent", "BOOTSTRAP.md"), bootstrap_content),
        (os.path.join(repo_root, "src", f"{project_id}.cpp"), src_cpp_content),
        (os.path.join(repo_root, "vcpkg", "vcpkg.json"), vcpkg_json_content),
        (
            os.path.join(repo_root, "vcpkg", "vcpkg-configuration.json"),
            vcpkg_configuration_content,
        ),
    ]

    if sdk_enabled:
        include_header_content = (
            "#pragma once\n\n"
            f"namespace {project_id} {{\n"
            f"}}  // namespace {project_id}\n"
        )
        sdk_config_content = (
            "@PACKAGE_INIT@\n\n"
            f'include("${{CMAKE_CURRENT_LIST_DIR}}/{sdk_package_name}Targets.cmake")\n'
            f"check_required_components({sdk_package_name})\n"
        )
        files_to_write.extend(
            [
                (os.path.join(repo_root, "include", f"{project_id}.hpp"), include_header_content),
                (
                    os.path.join(repo_root, "cmake", f"{sdk_package_name}Config.cmake.in"),
                    sdk_config_content,
                ),
            ]
        )

    for path, content in files_to_write:
        write_file_for_init(path, content)
        created_files.append(path)

    print("Initialized repository scaffold:")
    if created_dirs:
        print("  Directories:")
        for path in created_dirs:
            print(f"    + {format_path_for_output(path, repo_root)}/")
    if created_files:
        print("  Files:")
        for path in created_files:
            print(f"    + {format_path_for_output(path, repo_root)}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
