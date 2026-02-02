# Architecture

This repo builds a networked 3D client/server game (a BZFlag-inspired prototype).

At runtime there are two programs:

- **Client**: creates the window (SDL3), renders via bgfx/Diligent, runs local input/audio/UI, and syncs gameplay state to/from a server.
- **Server**: authoritative simulation of players/shots/world + chat + plugin callbacks, with LAN discovery support.

The code is organized around a small **engine layer** (game-agnostic systems) and a **gameplay layer** (World/Player/Shot/Game/Chat/etc.) that uses the engine subsystems (renderer/physics/network/input/audio/ui).

## Important directories (highlighted tree)

```
src/
    game/
        client/
            main.cpp                   # Client entrypoint (EngineApp wiring)
            game.{hpp,cpp}             # Orchestrates gameplay objects on the client
            world_session.*            # Receives world from server, merges config/assets, builds render+physics
            player.*                   # Local player input -> physics + sends network updates
            shot.*                     # Local + replicated shots (visual + raycast ricochet)
            console.*                  # Chat/console glue to UiSystem
            server/                    # Client-side discovery + connect flow
        server/
            main.cpp                   # Server entrypoint (EngineApp wiring)
            game.{hpp,cpp}             # Authoritative orchestration (clients, shots, chat, world)
            world_session.*            # Loads world config/assets + bundles world for clients
            client.*                   # Per-client authoritative state + replication
            shot.*                     # Authoritative shot creation/expiry + hit checks
            chat.*                     # Chat routing + plugin hook
            plugin.*                   # Embedded Python (pybind11) plugin API and callback registration
        engine/                        # BZ3-specific engine/game wiring
        net/                           # Game protocol and message networking
        renderer/                      # Game render orchestration (radar, ECS sync)
        ui/                            # HUD/console + UI frontends
        protos/                        # Protobuf wire schema

    engine/
        app/                           # EngineApp + lifecycle
        audio/                         # Audio system + backends
        common/                        # Config, data paths, i18n
        ecs/                           # ECS primitives + systems
        geometry/                      # Mesh loading utilities
        graphics/                      # Graphics device + backends (bgfx, Diligent)
        input/                         # Action-agnostic input mapping
        network/                       # Transport layer (ENet)
        physics/                       # Physics system + backends
        platform/                      # Window/events abstraction
        renderer/                      # Renderer core and scene orchestration
    karma-extras/                      # Optional UI frontends + helpers
        ui/                            # ImGui/RmlUi frontends + render bridges

data/
    common/config.json                 # Shared config layer (assets, network defaults, fonts)
    client/config.json                 # Client config layer (graphics/audio/gui/server lists)
    server/config.json                 # Server config layer (plugins, server defaults)
    common/{models,shaders,fonts}/     # Shared assets
    server/worlds/                     # Bundled worlds
    plugins/                           # Python plugins and bzapi helpers
```

## Core concepts

### Data root and configuration layering

The runtime **data root** is discovered via a game-provided spec (`src/game/common/data_path_spec.*`). For BZ3 this uses the `KARMA_DATA_DIR` environment variable. All config and assets are loaded relative to this directory.
See `CONFIG-SCHEMA.md` for a concise reference on layer order, asset lookup, and user config paths.

Configuration is **layered JSON** merged into a single “config cache”:

- Client initializes layers: `data/common/config.json` → `data/client/config.json` → user config (created if missing).
- Server initializes layers: `data/common/config.json` → `data/server/config.json` → selected world `config.json`.

This is implemented in `src/engine/common/data_path_resolver.*` (parameterized by the game spec). It also builds an **asset key lookup** by flattening `assets.*` and `fonts.*` entries across layers.

Practical implication:

- Prefer using asset keys (`ResolveConfiguredAsset(...)` or the World’s asset map) rather than hardcoding file paths.

#### User config + per-server downloads

The code keeps a per-user config directory (platform-specific) and uses it for:

- `config.json` (client-side user overrides)
- downloaded worlds received from servers

On Linux this typically ends up under `$XDG_CONFIG_HOME/bz3` or `~/.config/bz3` (based on the game spec’s app name).

Relevant helpers live in `src/engine/common/data_path_resolver.*`:

- `UserConfigDirectory()`
- `EnsureUserConfigFile("config.json")`
- `EnsureUserWorldsDirectory()`
- `EnsureUserWorldDirectoryForServer(host, port)`

### Engine vs gameplay

- **Engine** classes (`src/engine/...`) provide subsystems and own lifecycle/timing via `EngineApp`.
- **Gameplay** classes (`src/game/client/...`, `src/game/server/...`) implement game rules/state and plug into the engine via `GameInterface`.

The game mutates ECS data and uses public engine APIs; `EngineApp` owns the loop and system updates.

## Runtime flow

### Engine-owned loop (client)

The client entrypoint (`src/game/client/main.cpp`) wires `EngineApp` with a game-specific
`GameInterface`. The engine owns timing and system ordering. High-level flow per frame:

1. Poll window events and pump input (engine).
2. `GameInterface::onUpdate(dt)` (client gameplay orchestration).
3. UI layer `onFrame()` builds draw data (game UI layer).
4. ECS sync systems + physics/audio updates (engine).
5. Render main scene + UI draw data (engine).

Network message handling follows the **peek + flush** pattern: gameplay peeks messages,
and the network system flushes them during the engine tick.

### Engine-owned loop (server)

The server entrypoint (`src/game/server/main.cpp`) wires `EngineApp` with a server-side
`GameInterface`. Per frame:

1. Pump network transport (engine).
2. `GameInterface::onUpdate(dt)` (server gameplay orchestration).
3. ECS/physics updates (engine).
4. Flush peeked messages (engine).

## Networking

### Transport + protocol

- Transport: **ENet** (reliable UDP).
- Schema: **protobuf** in `src/game/protos/messages.proto`.

The network components decode protobuf messages and translate them into simple C++ structs (`ClientMsg_*`, `ServerMsg_*`) stored in an internal queue.

Key files:

- Client network: `src/game/net/client_network.*`
- Server network: `src/game/net/server_network.*`

### Message lifecycle (peek/flush)

Both client and server store received messages as heap-allocated structs and/or ENet packets. The pattern is:

- `peekMessage<T>(optionalPredicate)` returns a pointer to a queued message and marks it “peeked”.
- `flushPeekedMessages()` deletes the heap message or destroys the ENet packet backing it.

If you add a new message type, you must:

1. Update `src/game/protos/messages.proto`.
2. Update the encode/decode glue in the network components.
3. Add handling in the appropriate gameplay layer.

## World distribution and loading

### Server side

`src/game/server/world_session.*` loads config and assets for the selected world.

If the server is launched with a “custom world directory” (CLI), it can zip that directory on startup and later send it to connecting clients via `ServerMsg_Init.worldData`.

On a new connection, the server sends:

- Client id
- Server name
- Default player parameters
- Optional zipped world bytes

### Client side

`src/game/client/world_session.*` waits for `ServerMsg_Init`.

If `worldData` is present:

1. Create a per-server world directory under the user config directory (see `EnsureUserWorldDirectoryForServer(...)`).
2. Write `world.zip` and unzip all files.
3. If a `config.json` exists in the extracted world, merge it into the config cache as a new layer labelled “world config”.
4. Optionally read `manifest.json` for additional defaults and assets.

Then world initialization builds:

- Render model: ECS `MeshComponent` for `asset("world")`
- Static collision: `PhysicsWorld::createStaticMesh(asset("world"), 0.0f)`

### Key end-to-end flows

#### Connect → init → world

1. Community browser selects a server and calls `ServerConnector::connect(...)`.
2. `ClientNetwork::connect(...)` establishes ENet connection.
3. Client constructs `Game`, which constructs `World`.
4. Server sends `ServerMsg_Init` (client id + defaults + optional world zip).
5. Client `World::update()` consumes `ServerMsg_Init`, optionally unpacks world zip, merges world config/manifest, then creates render + physics world.
6. Client constructs the local `Player` and sends `ClientMsg_Init` with chosen player name.

If you see “connected but nothing happens”, follow this exact chain and verify where it breaks.

#### Shots

- Client firing creates a local shot immediately and also sends `ClientMsg_CreateShot` with a *local shot id*.
- Server receives it, allocates a *global shot id*, and broadcasts `ServerMsg_CreateShot` to everyone except the owner.
- When a server-side shot expires (or hits a player), the shot destructor broadcasts `ServerMsg_RemoveShot`:
    - to the owner: remove-by-local-id
    - to others: remove-by-global-id

#### Chat

- Client submits chat via the UiSystem console panel.
- Client sends `ClientMsg_Chat`.
- Server `Chat::update()` receives it, offers plugins a chance to handle it, then forwards `ServerMsg_Chat` to the target (or broadcasts).

## Engine systems

### Render (client)

`src/game/renderer/renderer.*` owns BZ3 render orchestration on top of the engine graphics device:

- `GraphicsDevice` (engine) exposes an engine-agnostic render API.
- The bgfx/Diligent backends own the renderer and model loading.
- `Renderer` manages radar targets, layers, and gameplay-facing `render_id`s.

Gameplay stores a `render_id` per object and updates transforms each frame.

### UiSystem (client)

`src/game/ui.*` owns Dear ImGui setup and two major views:

- **Community browser** view (when not connected)
- **HUD/console** view (when in game)

Fonts are loaded via `ResolveConfiguredAsset(...)` using configured font keys.

### Input (client)

`src/engine/input.*` is action-agnostic mapping (bindings + mapper). BZ3 builds a game-specific `InputState` in `src/game/input/state.*`, and `ClientEngine` exposes it via `engine.getInputState()`.

### Audio (client)

`src/engine/audio.*` wraps miniaudio or SDL audio:

- `Audio::loadClip(path, maxInstances)` pools multiple instances to avoid “dropouts” when many events occur (e.g. rapid firing).
- Listener position and direction are updated from the local player each frame.

### Physics (client + server)

`src/engine/physics/physics_world.*` wraps Jolt or PhysX.

- The world steps at fixed timestep substeps.
- `createStaticMesh()` loads a GLB and builds convex hull shapes per mesh.
- `raycast()` is used for shot ricochet.

## Gameplay modules

### Client gameplay

- `Game` (`src/game/client/game.*`): owns `World`, `Player`, remote `Client` objects, and `Shot` list; handles focus switching between HUD and chat.
- `World` (`src/game/client/world_session.*`): merges assets + defaults, receives server init, unzips/merges world layer, creates render+physics world.
- `Player` (`src/game/client/player.*`):
    - Sends `ClientMsg_Init` once on construction.
    - Handles spawn request (`ClientMsg_RequestPlayerSpawn`).
    - Applies movement/jump/fire based on `InputState`.
    - Sends location updates when position/rotation exceed thresholds.
- `Shot` (`src/game/client/shot.*`):
    - Local shots send `ClientMsg_CreateShot` with a local shot id.
    - Replicated shots use a global id from the server.
    - Client simulates shot motion and does a physics raycast each tick for ricochet.

### Server gameplay

- `Game` (`src/game/server/game.*`): authoritative hub; creates `Client` objects on join and updates shots/clients/chat/world.
- `Client` (`src/game/server/client.*`): authoritative per-player state; handles initialization, spawn, location forwarding, parameter updates, and death.
- `Shot` (`src/game/server/shot.*`): authoritative creation/expiry + hit checks; sends create/remove messages.
- `Chat` (`src/game/server/chat.*`): routes chat; offers plugin interception.
- `World` (`src/game/server/world_session.*`): world config/assets + optional world zip distribution.

## Community browser and discovery

There are two server discovery sources:

1. **LAN discovery**
     - Protocol: `src/game/net/discovery_protocol.hpp`
     - Server responder: `src/game/server/server_discovery.*` (UDP port 47800)
     - Client scanner: `src/game/client/server/server_discovery.*`

2. **Remote server lists (HTTP JSON)**
     - Fetch: `src/game/client/server/server_list_fetcher.*` (libcurl)
     - Orchestration: `src/game/client/server/community_browser_controller.*`
     - UI: `src/game/ui/frontends/*/console/console.*`

The server browser controller merges results from LAN + remote lists into a single list of UI entries and delegates connection to `ServerConnector`.

## Plugins (server)

The server embeds Python using pybind11 (`src/game/server/plugin.*`).

At startup the server:

1. Creates a Python interpreter (`py::scoped_interpreter`).
2. Loads configured plugins from `data/plugins/<pluginName>/plugin.py`.
3. Exposes an embedded module named `bzapi` containing:
     - callback registration by event type
     - chat send
     - set player parameter
     - kill/kick/disconnect
     - basic player lookup helpers

Plugin callbacks are keyed by `ClientMsg_Type` and currently invoked from gameplay (notably chat).

## “Where do I implement X?” cheat sheet

- **Main loop timing / ordering**: `src/game/client/main.cpp`, `src/game/server/main.cpp`, `src/game/engine/*_engine.*`
- **New networked feature**: `src/game/protos/messages.proto` + `src/game/net/*` + gameplay handler in `src/game/client/*` and/or `src/game/server/*`
- **World loading / packaging**: `src/game/server/world_session.*` and `src/game/client/world_session.*`
- **Physics issues**: `src/engine/physics/physics_world.*` (and GLB meshes used by world assets)
- **UI/HUD**: `src/game/ui/` (backend entry + frontends/*/hud)
- **Community browser**: `src/game/client/server/*` (control) + `src/game/ui/frontends/*/console/console.*` (view)
- **Plugins / moderation / commands**: `src/game/server/plugin.*` and `data/plugins/*`
- **Adding new subsystems**: `HOW-TO-ADD-SUBSYSTEM.md`

## Common gotchas

- `KARMA_DATA_DIR` must point at the repo’s `data/` directory (or an installed equivalent).
- Config and assets are *layered*; if something “mysteriously” changes between worlds, check the world’s `config.json` and `manifest.json` merging.
- Network messages are freed on `flushPeekedMessages()`; don’t hold pointers returned by `peekMessage<T>()` beyond the current frame.

## Agent prompts

Repo-level guidance lives in `AGENTS.md`. Task-specific prompts live under `docs/agent-prompts/`.
