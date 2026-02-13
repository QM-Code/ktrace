#!/usr/bin/env python3

import os
import subprocess
import sys


PLATFORM_OPTIONS = ["sdl3", "sdl2", "glfw"]
RENDERER_OPTIONS = ["bgfx", "diligent"]
PHYSICS_OPTIONS = ["jolt", "physx"]
UI_OPTIONS = ["rmlui", "imgui"]
AUDIO_OPTIONS = ["sdl3audio", "miniaudio"]

DEFAULT_PLATFORM = "sdl3"
DEFAULT_RENDERER = "bgfx"
DEFAULT_PHYSICS = "jolt"
DEFAULT_UI = "rmlui"
DEFAULT_AUDIO = "sdl3audio"


def usage(exit_code: int = 1) -> None:
    prog = os.path.basename(sys.argv[0])
    print(
        f"Usage: {prog} [-c|--configure] [-a|--all] "
        "[--test-physics] [--test-audio] [--test-engine] "
        "[build-<platform>-<renderer>-<physics>-<ui>-<audio>]",
        file=sys.stderr,
    )
    print("  -c, --configure   run cmake -S configure step before building", file=sys.stderr)
    print("  -a, --all         build consolidated dev profile into build-dev/", file=sys.stderr)
    print("  --test-physics    run physics backend parity tests via ctest after build", file=sys.stderr)
    print("  --test-audio      run audio backend smoke tests via ctest after build", file=sys.stderr)
    print("  --test-engine     run both physics and audio backend tests via ctest after build", file=sys.stderr)
    raise SystemExit(exit_code)


def fail(message: str) -> None:
    print(f"Error: {message}", file=sys.stderr)
    usage(1)


def prompt_choice(prompt: str, default: str, options: list[str]) -> str:
    if not sys.stdin.isatty():
        print("Error: interactive prompts require a tty. Provide a build directory.", file=sys.stderr)
        raise SystemExit(1)

    while True:
        try:
            choice = input(f"{prompt} [{default}]: ").strip().lower()
        except EOFError:
            print("Error: unable to read interactive input.", file=sys.stderr)
            raise SystemExit(1)
        except KeyboardInterrupt:
            print("\nCanceled")
            raise SystemExit(130)

        if not choice:
            choice = default
        if choice in options:
            return choice
        print(f"Invalid choice: {choice}")


def run(cmd: list[str], *, env: dict[str, str] | None = None) -> None:
    subprocess.run(cmd, check=True, env=env)


def read_cached_toolchain(build_dir: str) -> str:
    cache_path = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cache_path):
        return ""

    try:
        with open(cache_path, "r", encoding="utf-8") as cache:
            for line in cache:
                if line.startswith("CMAKE_TOOLCHAIN_FILE:"):
                    _, value = line.split("=", 1)
                    return value.strip()
    except OSError:
        return ""

    return ""


def is_local_vcpkg_bootstrapped(vcpkg_root: str) -> bool:
    candidates = [
        os.path.join(vcpkg_root, "vcpkg"),
        os.path.join(vcpkg_root, "vcpkg.exe"),
        os.path.join(vcpkg_root, "vcpkg.bat"),
    ]
    return any(os.path.isfile(path) for path in candidates)


def ensure_local_vcpkg(repo_root: str) -> tuple[str, str]:
    local_vcpkg_root = os.path.join(repo_root, "vcpkg")
    local_toolchain = os.path.join(local_vcpkg_root, "scripts", "buildsystems", "vcpkg.cmake")

    if not os.path.isdir(local_vcpkg_root):
        print(
            "Error: local vcpkg is mandatory for builds, but ./vcpkg is missing.\n"
            "Bootstrap once from repo root:\n"
            "  git clone https://github.com/microsoft/vcpkg.git vcpkg\n"
            "  ./vcpkg/bootstrap-vcpkg.sh -disableMetrics",
            file=sys.stderr,
        )
        raise SystemExit(2)

    if not os.path.isfile(local_toolchain):
        print(
            "Error: local vcpkg exists but toolchain file is missing.\n"
            "Expected:\n"
            "  ./vcpkg/scripts/buildsystems/vcpkg.cmake\n"
            "Re-bootstrap local vcpkg and retry.",
            file=sys.stderr,
        )
        raise SystemExit(2)

    if not is_local_vcpkg_bootstrapped(local_vcpkg_root):
        print(
            "Error: local ./vcpkg is present but not bootstrapped.\n"
            "Run:\n"
            "  ./vcpkg/bootstrap-vcpkg.sh -disableMetrics",
            file=sys.stderr,
        )
        raise SystemExit(2)

    return os.path.abspath(local_vcpkg_root), os.path.abspath(local_toolchain)


def parse_named_build_dir(build_dir: str) -> dict[str, object]:
    if build_dir == "build-dev":
        return {
            "build_dir": "build-dev",
            "platform": DEFAULT_PLATFORM,
            "renderer": DEFAULT_RENDERER,
            "renderers": ["bgfx", "diligent"],
            "physics": DEFAULT_PHYSICS,
            "ui": DEFAULT_UI,
            "audio": DEFAULT_AUDIO,
            "all": True,
        }

    if not build_dir.startswith("build-"):
        fail("build directory must start with 'build-'")

    tokens = build_dir[len("build-") :].split("-")
    if len(tokens) != 5:
        fail(
            "build directory must follow 'build-<platform>-<renderer>-<physics>-<ui>-<audio>'"
        )

    platform, renderer, physics, ui, audio = tokens
    if platform not in PLATFORM_OPTIONS:
        fail(f"invalid platform token '{platform}'")
    if renderer not in RENDERER_OPTIONS:
        fail(f"invalid renderer token '{renderer}'")
    if physics not in PHYSICS_OPTIONS:
        fail(f"invalid physics token '{physics}'")
    if ui not in UI_OPTIONS:
        fail(f"invalid ui token '{ui}'")
    if audio not in AUDIO_OPTIONS:
        fail(f"invalid audio token '{audio}'")

    return {
        "build_dir": build_dir,
        "platform": platform,
        "renderer": renderer,
        "renderers": [renderer],
        "physics": physics,
        "ui": ui,
        "audio": audio,
        "all": False,
    }


def interactive_profile() -> dict[str, object]:
    platform = prompt_choice("Platform (sdl3/sdl2/glfw)", DEFAULT_PLATFORM, PLATFORM_OPTIONS)
    renderer = prompt_choice("Renderer (bgfx/diligent)", DEFAULT_RENDERER, RENDERER_OPTIONS)
    physics = prompt_choice("Physics (jolt/physx)", DEFAULT_PHYSICS, PHYSICS_OPTIONS)
    ui = prompt_choice("UI (rmlui/imgui)", DEFAULT_UI, UI_OPTIONS)
    audio = prompt_choice("Audio (sdl3audio/miniaudio)", DEFAULT_AUDIO, AUDIO_OPTIONS)

    build_dir = f"build-{platform}-{renderer}-{physics}-{ui}-{audio}"
    return {
        "build_dir": build_dir,
        "platform": platform,
        "renderer": renderer,
        "renderers": [renderer],
        "physics": physics,
        "ui": ui,
        "audio": audio,
        "all": False,
    }


def all_profile() -> dict[str, object]:
    return {
        "build_dir": "build-dev",
        "platform": DEFAULT_PLATFORM,
        "renderer": DEFAULT_RENDERER,
        "renderers": ["bgfx", "diligent"],
        "physics": DEFAULT_PHYSICS,
        "ui": DEFAULT_UI,
        "audio": DEFAULT_AUDIO,
        "all": True,
    }


def main() -> int:
    args = sys.argv[1:]
    run_configure = False
    all_build = False
    run_physics_tests = False
    run_audio_tests = False
    build_dir_arg = ""

    i = 0
    while i < len(args):
        arg = args[i]
        if arg in ("-h", "--help"):
            usage(0)
        elif arg in ("-c", "--configure"):
            run_configure = True
        elif arg in ("-a", "--all"):
            all_build = True
        elif arg == "--test-physics":
            run_physics_tests = True
        elif arg == "--test-audio":
            run_audio_tests = True
        elif arg == "--test-engine":
            run_physics_tests = True
            run_audio_tests = True
        elif arg.startswith("-"):
            fail(f"unknown option '{arg}'")
        else:
            if build_dir_arg:
                fail("only one build directory argument may be provided")
            build_dir_arg = arg
        i += 1

    if all_build and build_dir_arg:
        fail("cannot combine --all with an explicit build directory")

    if all_build:
        profile = all_profile()
    elif build_dir_arg:
        profile = parse_named_build_dir(build_dir_arg)
    else:
        profile = interactive_profile()

    build_dir = str(profile["build_dir"])
    platform = str(profile["platform"])
    renderer_values = [str(value) for value in profile["renderers"]]
    physics = str(profile["physics"])
    ui = str(profile["ui"])
    audio = str(profile["audio"])
    all_mode = bool(profile["all"])

    cmake_args = [
        f"-DKARMA_WINDOW_BACKEND={platform}",
        f"-DKARMA_RENDER_BACKENDS={';'.join(renderer_values)}",
        f"-DKARMA_PHYSICS_BACKEND={physics}",
        f"-DKARMA_UI_BACKEND={ui}",
        f"-DKARMA_AUDIO_BACKEND={audio}",
    ]

    if all_mode or ui == "rmlui":
        cmake_args.append("-DKARMA_ENABLE_RMLUI_BACKEND=ON")
    else:
        cmake_args.append("-DKARMA_ENABLE_RMLUI_BACKEND=OFF")

    if all_mode:
        cmake_args.append("-DKARMA_PHYSICS_BACKENDS=jolt;physx")
        cmake_args.append("-DKARMA_UI_BACKENDS=imgui;rmlui")
        cmake_args.append("-DKARMA_AUDIO_BACKENDS=sdl3audio;miniaudio")
    if run_physics_tests or run_audio_tests:
        cmake_args.append("-DBUILD_TESTING=ON")

    # Default to offline-safe FetchContent behavior to avoid unexpected
    # dependency git updates during normal local builds.
    fetch_updates_disconnected = os.environ.get("BZBUILD_FETCH_UPDATES_DISCONNECTED", "1").strip().lower()
    if fetch_updates_disconnected in ("0", "false", "off", "no"):
        cmake_args.append("-DFETCHCONTENT_UPDATES_DISCONNECTED=OFF")
    else:
        cmake_args.append("-DFETCHCONTENT_UPDATES_DISCONNECTED=ON")

    env = os.environ.copy()
    repo_root = os.path.abspath(os.path.dirname(__file__))
    local_vcpkg_root, local_toolchain = ensure_local_vcpkg(repo_root)

    cached_toolchain = read_cached_toolchain(build_dir)
    expected_toolchain = os.path.abspath(local_toolchain)
    cached_toolchain_abs = os.path.abspath(cached_toolchain) if cached_toolchain else ""
    if cached_toolchain and cached_toolchain_abs != expected_toolchain:
        print(
            "Error: build directory is pinned to a different vcpkg toolchain than required local ./vcpkg.\n"
            f"Current cached toolchain:\n  {cached_toolchain}\n"
            f"Required toolchain:\n  {expected_toolchain}\n"
            "Fix:\n"
            f"  rm -f {build_dir}/CMakeCache.txt\n"
            f"  rm -rf {build_dir}/CMakeFiles\n"
            f"  ./bzbuild.py -c {build_dir}",
            file=sys.stderr,
        )
        return 2

    env_vcpkg_root = env.get("VCPKG_ROOT")
    if env_vcpkg_root and os.path.abspath(env_vcpkg_root) != local_vcpkg_root:
        print(
            "Error: VCPKG_ROOT points outside mandatory local ./vcpkg.\n"
            f"Current VCPKG_ROOT:\n  {env_vcpkg_root}\n"
            f"Required VCPKG_ROOT:\n  {local_vcpkg_root}\n"
            "Unset VCPKG_ROOT or set it to local ./vcpkg and retry.",
            file=sys.stderr,
        )
        return 2

    env["VCPKG_ROOT"] = local_vcpkg_root
    cmake_args.append(f"-DCMAKE_TOOLCHAIN_FILE={local_toolchain}")

    if not os.path.isdir(build_dir):
        os.makedirs(build_dir, exist_ok=True)
        run_configure = True

    cache_path = os.path.join(build_dir, "CMakeCache.txt")
    if not os.path.isfile(cache_path):
        run_configure = True
    else:
        has_makefile = os.path.isfile(os.path.join(build_dir, "Makefile"))
        has_ninja = os.path.isfile(os.path.join(build_dir, "build.ninja"))
        if not (has_makefile or has_ninja):
            run_configure = True

    if run_configure:
        run(["cmake", "-S", ".", "-B", build_dir, *cmake_args], env=env)

    run(["cmake", "--build", build_dir, "-j4"], env=env)
    if run_physics_tests:
        run(["ctest", "--test-dir", build_dir, "-R", "physics_backend_parity", "--output-on-failure"], env=env)
    if run_audio_tests:
        run(["ctest", "--test-dir", build_dir, "-R", "audio_backend_smoke", "--output-on-failure"], env=env)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
