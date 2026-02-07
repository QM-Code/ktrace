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
        "[build-<platform>-<renderer>-<physics>-<ui>-<audio>]",
        file=sys.stderr,
    )
    print("  -c, --configure   run cmake -S configure step before building", file=sys.stderr)
    print("  -a, --all         build consolidated dev profile into build-dev/", file=sys.stderr)
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

    # Default to offline-safe FetchContent behavior to avoid unexpected
    # dependency git updates during normal local builds.
    fetch_updates_disconnected = os.environ.get("BZBUILD_FETCH_UPDATES_DISCONNECTED", "1").strip().lower()
    if fetch_updates_disconnected in ("0", "false", "off", "no"):
        cmake_args.append("-DFETCHCONTENT_UPDATES_DISCONNECTED=OFF")
    else:
        cmake_args.append("-DFETCHCONTENT_UPDATES_DISCONNECTED=ON")

    env = os.environ.copy()
    vcpkg_root = env.get("VCPKG_ROOT")
    if not vcpkg_root:
        candidate = os.path.join(os.path.dirname(__file__), "..", "m-dev", "vcpkg")
        if os.path.isdir(candidate):
            vcpkg_root = os.path.abspath(candidate)
            env["VCPKG_ROOT"] = vcpkg_root

    if vcpkg_root:
        toolchain = os.path.join(vcpkg_root, "scripts", "buildsystems", "vcpkg.cmake")
        if os.path.isfile(toolchain):
            cmake_args.append(f"-DCMAKE_TOOLCHAIN_FILE={toolchain}")

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
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
