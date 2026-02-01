# BZ3

BZ3 is a C++20 client/server 3D game inspired by BZFlag.

## Quick Start

**Linux/macOS**
- `./setup.sh`
- `cmake --build build`

**Windows**
- Run `setup.bat`
- `cmake --build build --config Release`

This project uses vcpkg to provide most native dependencies and the setup scripts automatically configure CMake with the correct toolchain.

## Runtime Data

The programs load assets/config from a data root resolved via the `KARMA_DATA_DIR` environment variable (configured by `src/game/common/data_path_spec.*`).
See `CONFIG-SCHEMA.md` for config layering and asset lookup details.

- Linux/macOS:

  - `export KARMA_DATA_DIR="$PWD/data"`
- Windows (PowerShell):

  - `$env:KARMA_DATA_DIR = "$pwd\data"`

## Install (Prerequisites)

Dependencies are built via vcpkg, but you still need a working C/C++ toolchain and OS graphics headers.

### Linux (Ubuntu/Debian)

```bash
sudo apt update
sudo apt install -y \
  git cmake build-essential ninja-build pkg-config \
  python3 python3-dev \
  xorg-dev libgl1-mesa-dev
```

Then run `./setup.sh`.

### macOS

- Install Xcode Command Line Tools:

  - `xcode-select --install`
- Install build tools:

  - `brew install git cmake ninja pkg-config python`

Then run `./setup.sh`.

### Windows

- Install **Visual Studio 2022** (or Build Tools) with **Desktop development with C++**.
- Install **Git**.
- Ensure **CMake** is available (VS includes it, or install separately).

Then run `setup.bat`.

## Run

After building:

- Linux/macOS: `./build/bz3` and `./build/bz3-server`
- Windows: `build\Release\bz3.exe` and `build\Release\bz3-server.exe`

### Config validation

Startup validates required config keys. To allow startup with warnings only, pass:

- Client: `./build/bz3 --strict-config=false`
- Server: `./build/bz3-server --strict-config=false`

You can verify the required-key list stays in sync with code using:

- `scripts/check_required_config.py`

### UI smoke test

The client has a lightweight HUD smoke test that toggles visibility flags on a timer:

- `./build/bz3 --ui-smoke`

## Overview (Libraries in Use)

Rendering and windowing
- **bgfx** (renderer backend, default)
- **Diligent** (renderer backend, optional)
- **Filament** (renderer/scene graph, optional)
- **SDL3** (windowing, default)
- **SDL2** (windowing stub)
- **Assimp** (model loading)

UI

Networking
- **ENet** (reliable UDP transport)
- **Protobuf** (message schema/serialization)
- Custom LAN discovery protocol (see `src/game/net/discovery_protocol.hpp`)

Simulation
- **Jolt** or **PhysX** (physics, selectable)
- **glm** (math)

Other
- **miniaudio** or **SDL3 audio** (selectable)
- **spdlog** (logging)
- **cxxopts** (CLI parsing)
- **nlohmann-json** (config)
- **miniz** (world zip/unzip)
- **libcurl** (HTTP fetches, e.g. remote server list)
- **pybind11** + **Python** (server-side plugins)

## Notes

- Most dependencies are provided by vcpkg (see `vcpkg.json`).
- Some libraries are fetched via CMake FetchContent (notably `enet`, `pybind11`).
- Python plugin bytecode is redirected to a writable cache: set `KARMA_PY_CACHE_DIR` to choose the location (defaults to `/tmp/bz3-pycache`). If the directory cannot be created, bytecode writing is disabled. Current behavior is acceptable for now; we may revisit a dedicated cache path later.

## Engine vs game

- **Engine** (game-agnostic systems) lives under `src/engine/`.
- **Game** (BZ3-specific rules, UI, and gameplay) lives under `src/game/`.
- The game configures engine data/asset discovery via `src/game/common/data_path_spec.*`.

## Engine library (Karma)

The engine is exported as a standalone library with public headers under
`include/karma/`. After install, consumers can use `find_package(karma)` and
link against `karma::karma`.

Example:

```cmake
find_package(karma REQUIRED)
add_executable(my_game main.cpp)
target_link_libraries(my_game PRIVATE karma::karma)
```

See `examples/karma_find_package/` for a minimal CMake + main.cpp.

## Backends and entry points

BZ3 uses a consistent interface → backend pattern for several subsystems. Engine-side public interfaces live in `src/engine/<subsystem>/` and delegate to backend implementations under `src/engine/<subsystem>/backends/<name>/`.

Entry points (public interfaces)
- Audio: `Audio` in `src/engine/audio/audio.hpp`
- Windowing: `Window` in `src/engine/platform/window.hpp`
- Graphics: `GraphicsDevice` in `src/engine/graphics/device.hpp`
- Renderer orchestration: `Renderer` in `src/game/renderer/renderer.hpp`
- UI: `UiSystem` in `src/game/ui/core/system.hpp`
- UI render bridge: `ui::RendererBridge` in `src/game/ui/bridges/renderer_bridge.hpp`
- Physics: `PhysicsWorld` in `src/engine/physics/physics_world.hpp`
- Networking: `ClientNetwork` and `ServerNetwork` in `src/game/net/` (message-level); transports live in `src/engine/network/`
- World runtime: `ClientWorldSession` and `ServerWorldSession` in `src/game/client/` and `src/game/server/`
- Input: `Input` in `src/engine/input/input.hpp` (mapping only) + game input state in `src/game/input/state.*`
- Data paths: `DataPathSpec` in `src/engine/common/data_path_resolver.hpp` (configured in `src/game/common/data_path_spec.*`)

Backend factories (compile-time selection)
- Audio: `src/engine/audio/backend_factory.cpp`
- Windowing: `src/engine/platform/window_factory.cpp`
- UI: `src/game/ui/core/backend_factory.cpp`
- Physics: `src/engine/physics/backend_factory.cpp`
- Networking: `src/game/net/backend_factory.cpp`
- World: `src/engine/world/backend_factory.cpp`

Backend layouts (examples)
- `src/engine/audio/backends/miniaudio/` and `src/engine/audio/backends/sdl/`
- `src/engine/platform/backends/` (currently `sdl3`, with an `sdl2` stub)
- `src/game/ui/frontends/imgui/` and `src/game/ui/frontends/rmlui/`
- `src/engine/physics/backends/jolt/` and `src/engine/physics/backends/physx/`
- `src/engine/graphics/backends/bgfx/` and `src/engine/graphics/backends/diligent/`
- `src/game/net/backends/enet/` (future: steam, webrtc, etc.)
- `src/engine/world/backends/fs/` (future: zip, remote, etc.)
- `src/engine/input/mapping/` (action-agnostic mapping: bindings, maps, mapper)
- `src/game/input/` (BZ3 action IDs + default bindings)

## Build options (backend selection)

These CMake cache variables select backends at build time:

- `KARMA_UI_BACKEND=imgui|rmlui`
- `KARMA_WINDOW_BACKEND=sdl3|sdl2`
- `KARMA_PHYSICS_BACKEND=jolt|physx`
- `KARMA_AUDIO_BACKEND=miniaudio|sdlaudio`
- `KARMA_RENDER_BACKEND=bgfx|diligent`
- `KARMA_NETWORK_BACKEND=enet`

Engine/Game build modes:

- `KARMA_ENGINE_ONLY=ON` to build and install only the engine libraries.
- `BZ3_GAME_ONLY=ON` to build only the game (requires an installed Karma package; set `CMAKE_PREFIX_PATH`).

Example (engine-only build + install):

```bash
cmake -S . -B build-engine-only -DKARMA_ENGINE_ONLY=ON
cmake --build build-engine-only
cmake --install build-engine-only --prefix /opt/karma
```

Example (game-only build using installed engine):

```bash
cmake -S . -B build-game-only -DBZ3_GAME_ONLY=ON -DCMAKE_PREFIX_PATH=/opt/karma
cmake --build build-game-only
```

Example:

```bash
cmake -S . -B build-sdl3-rmlui-sdlaudio-bgfx-enet \
  -DKARMA_WINDOW_BACKEND=sdl3 \
  -DKARMA_UI_BACKEND=rmlui \
  -DKARMA_PHYSICS_BACKEND=jolt \
  -DKARMA_AUDIO_BACKEND=sdlaudio \
  -DKARMA_RENDER_BACKEND=bgfx \
  -DKARMA_NETWORK_BACKEND=enet
cmake --build build-sdl3-rmlui-sdlaudio-bgfx-enet
```

## Input bindings

Input actions are mapped via the `keybindings` config object (merged from the usual config layers). Keys are specified as strings like `"W"`, `"SPACE"`, `"LEFT_MOUSE"`, `"F1"`, or `"MOUSE_BUTTON_4"`. If a binding is missing or invalid, defaults are used.

Actions:
- `fire` (default: `["F", "E", "LEFT_MOUSE"]`)
- `spawn` (default: `["U"]`)
- `jump` (default: `["SPACE"]`)
- `quickQuit` (default: `["F12"]`)
- `chat` (default: `["T"]`)
- `escape` (default: `["ESCAPE"]`)
- `toggleFullscreen` (default: `["RIGHT_BRACKET"]`)
- `moveLeft` (default: `["LEFT", "J"]`)
- `moveRight` (default: `["RIGHT", "L"]`)
- `moveForward` (default: `["UP", "I"]`)
- `moveBackward` (default: `["DOWN", "K"]`)

## Agent prompts

Repo-level guidance lives in `AGENTS.md`. Task-specific prompts live under `docs/agent-prompts/` (e.g., Webserver, UiSystem/HUD). In a new session, ask to use a prompt by its title.

## Guides

- `HOW-TO-ADD-SUBSYSTEM.md`

## TODO
* Teams
* Shots are shooting inside buildings (this is because of muzzle shot position)
* Muzzle flash
* Shot fizzle out effect
* Make current player and FOV appear on radar
* Tank treads when moving
* Improve radar shader
* API Event handler
* Better sounds
* Switch to Ogre rendering engine
* RmlUi: add select dropdown open/close events (e.g., selectboxshown/selectboxhidden) so UI can react to select popup visibility; maintain a small patch/fork and upstream a PR once validated.

## Events
* Tick
* Chat
* PlayerJoin
* PlayerLeave
* PlayerSpawn
* PlayerDie
* PlayerCreateShot
* RemoveShot
* FlagGrab
* FlagDrop

## License

MIT
