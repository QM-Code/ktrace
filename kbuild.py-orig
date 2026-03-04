#!/usr/bin/env python3

import json
import os
import shutil
import subprocess
import sys


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
    print(
        "  --install-vcpkg     clone/bootstrap local vcpkg under ./vcpkg, then build (default: build/latest)",
        file=sys.stderr,
    )
    raise SystemExit(exit_code)


def fail(message: str) -> None:
    print(f"Error: {message}", file=sys.stderr)
    usage(1)


def run(cmd: list[str], *, env: dict[str, str] | None = None) -> None:
    subprocess.run(cmd, check=True, env=env)


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
            "  schema_version (int)\n"
            "  cmake_package_name (string)\n"
            "  requires_vcpkg (bool)\n"
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

    schema_version = raw.get("schema_version")
    cmake_package_name = raw.get("cmake_package_name")
    requires_vcpkg = raw.get("requires_vcpkg")
    build_demos_raw = raw.get("build-demos", [])
    sdk_dependencies_raw = raw.get("sdk-dependencies", [])

    if not isinstance(schema_version, int):
        print("Error: kbuild.json key 'schema_version' must be an integer", file=sys.stderr)
        raise SystemExit(2)
    if schema_version != 1:
        print(
            f"Error: unsupported kbuild.json schema_version '{schema_version}' (expected 1)",
            file=sys.stderr,
        )
        raise SystemExit(2)
    if not isinstance(cmake_package_name, str) or not cmake_package_name.strip():
        print(
            "Error: kbuild.json key 'cmake_package_name' must be a non-empty string",
            file=sys.stderr,
        )
        raise SystemExit(2)
    if not isinstance(requires_vcpkg, bool):
        print("Error: kbuild.json key 'requires_vcpkg' must be true or false", file=sys.stderr)
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

        if dep_package == cmake_package_name.strip():
            print(
                f"Error: kbuild.json sdk dependency '{dep_package}' cannot match root cmake_package_name",
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

    return cmake_package_name.strip(), requires_vcpkg, build_demos, sdk_dependencies


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
            "Error: build requires vcpkg to be set up under:\n"
            "  ./vcpkg/src\n"
            "  ./vcpkg/build\n"
            "Run:\n"
            "  ./kbuild.py --install-vcpkg",
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
    args = sys.argv[1:]
    version = "latest"
    version_explicit = False
    no_configure = False
    install_vcpkg = False
    build_demos = False
    list_builds = False
    remove_latest_builds = False
    requested_demos: list[str] = []

    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ("-h", "--help"):
            usage(0)
        elif arg == "--list-builds":
            list_builds = True
        elif arg == "--remove-latest":
            remove_latest_builds = True
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

    if list_builds and remove_latest_builds:
        fail("--list-builds and --remove-latest cannot be combined")
    if list_builds and build_mode_flags:
        fail("--list-builds cannot be combined with build options")
    if remove_latest_builds and build_mode_flags:
        fail("--remove-latest cannot be combined with build options")

    repo_root = os.path.abspath(os.path.dirname(__file__))
    if remove_latest_builds:
        return remove_latest_build_dirs(repo_root)
    if list_builds:
        return list_build_dirs(repo_root)

    cmake_package_name, requires_vcpkg, config_build_demos, config_sdk_dependencies = load_kbuild_config(repo_root)
    sdk_dependencies = resolve_sdk_dependencies(repo_root, version, config_sdk_dependencies)

    demo_order: list[str] = []
    if build_demos:
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


if __name__ == "__main__":
    raise SystemExit(main())
