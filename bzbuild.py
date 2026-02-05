#!/usr/bin/env python3

import os
import sys
import subprocess


def usage() -> None:
    prog = os.path.basename(sys.argv[0])
    print(f"Usage: {prog} [-c] [build-<window>-<ui>-<physics>-<audio>-<renderer>-<network>]", file=sys.stderr)
    print("  -c   run cmake -S configure step before building", file=sys.stderr)
    raise SystemExit(1)


def prompt_choice(prompt: str, default: str, options: list[str]) -> str:
    if not sys.stdin.isatty():
        print("Error: interactive prompts require a tty. Provide a build directory.", file=sys.stderr)
        raise SystemExit(1)

    while True:
        try:
            choice = input(f"{prompt} [{default}]: ").strip()
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


args = sys.argv[1:]
run_configure = False

# Minimal option parsing for -c
if args and args[0] == "-c":
    run_configure = True
    args = args[1:]

if len(args) > 1:
    usage()

window = ""
ui = ""
physics = ""
audio = ""
render = ""
network = ""
build_dir = ""

if not args:
    window = prompt_choice("Platform (sdl3)", "sdl3", ["sdl3"])
    # ui = prompt_choice("UI (rmlui/imgui)", "rmlui", ["rmlui", "imgui"])
    # physics = prompt_choice("Physics (jolt/physx)", "jolt", ["jolt", "physx"])
    # audio = prompt_choice("Audio (miniaudio/sdlaudio)", "sdlaudio", ["miniaudio", "sdlaudio"])
    render = prompt_choice("Renderer (diligent/bgfx)", "bgfx", ["diligent", "bgfx"])
    # network = prompt_choice("Network (enet)", "enet", ["enet"])
    build_dir = f"build-{window}-{render}"
else:
    build_dir = args[0]
    if not build_dir.startswith("build-"):
        print("Error: build directory must start with 'build-'", file=sys.stderr)
        usage()

    name = build_dir[len("build-"):]
    parts = name.split("-") if name else []

    for part in parts:
        if part in ("sdl3",):
            if not window:
                window = part
        # elif part in ("imgui", "rmlui"):
        #     ui = part
        # elif part in ("jolt", "physx"):
        #     physics = part
        # elif part in ("miniaudio", "sdlaudio"):
        #     audio = part
        elif part in ("diligent", "bgfx"):
            render = part
        # elif part == "enet":
        #     network = part
        elif part == "fs":
            print("Error: build dir must not include the deprecated world backend token 'fs'.", file=sys.stderr)
            usage()

    if not (window and render):
        print("Error: build dir must include window and renderer tokens.", file=sys.stderr)
        usage()

cmake_args = [
    f"-DKARMA_WINDOW_BACKEND={window}",
    f"-DKARMA_RENDER_BACKEND={render}",
    # f"-DKARMA_UI_BACKEND={ui}",
    # f"-DKARMA_PHYSICS_BACKEND={physics}",
    # f"-DKARMA_AUDIO_BACKEND={audio}",
    # f"-DKARMA_NETWORK_BACKEND={network}",
]

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

# If the build directory exists but hasn't been configured, force a configure.
cache_path = os.path.join(build_dir, "CMakeCache.txt")
if not os.path.isfile(cache_path):
    run_configure = True
else:
    # If the cache exists but no build files are present, reconfigure.
    has_makefile = os.path.isfile(os.path.join(build_dir, "Makefile"))
    has_ninja = os.path.isfile(os.path.join(build_dir, "build.ninja"))
    if not (has_makefile or has_ninja):
        run_configure = True

if run_configure:
    run(["cmake", "-S", ".", "-B", build_dir, *cmake_args], env=env)

# if render in ("bgfx",):
#     scripts_dir = os.path.join(os.path.dirname(__file__), "scripts")
#     if render == "bgfx":
#         script_path = os.path.join(scripts_dir, "build_bgfx_shaders.sh")
#         if os.path.isfile(script_path):
#             env = os.environ.copy()
#             env.setdefault("KARMA_BUILD_DIR", os.path.abspath(build_dir))
#             run([script_path], env=env)
#         else:
#             print(f"Warning: bgfx shader build script not found at {script_path}", file=sys.stderr)

run(["cmake", "--build", build_dir, "-j4"], env=env)
