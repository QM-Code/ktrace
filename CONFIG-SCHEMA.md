# Configuration & Data Layout

This document describes how configuration and assets are layered and resolved at runtime.
It is descriptive only; see the referenced code for authoritative behavior.

## Data root (KARMA_DATA_DIR)

- The game requires `KARMA_DATA_DIR` to point at the runtime data root.
- The data root is validated by checking for a required marker file:
  `common/config.json`.
- The spec is configured in `src/game/common/data_path_spec.cpp` and consumed
  by `src/engine/common/data_path_resolver.*`.

If the data root is missing or invalid, startup fails with an explicit error.

## Configuration layers

Configuration is layered JSON objects merged into a single config cache.
Later layers override earlier values (object keys merge recursively; non-objects
overwrite).

The core config store is in `src/engine/common/config_store.*`.

### Shared engine defaults (always first)

`src/engine/data/config.json` is always loaded first as the engine default layer.

### Client layering

Configured in `src/game/client/main.cpp`:

1) `src/engine/data/config.json` (engine defaults)
2) `data/common/config.json`
3) `data/client/config.json`
4) user config file (see below)

### Server layering

Configured in `src/game/server/main.cpp`:

1) `src/engine/data/config.json` (engine defaults)
2) `data/common/config.json`
3) `data/server/config.json`
4) user config file (see below)
5) world config (runtime layer; see next section)

### Runtime layers (world config)

When a server loads a world, it reads `<world>/config.json` and injects it as a
runtime layer labeled `"world config"` (see `src/game/server/main.cpp`).
This lets a world override or extend server settings without replacing base
configuration files.

On the client, world data received from a server is unpacked and the world
config is merged as a runtime layer as part of world session initialization
(`src/game/client/world_session.*`).

## User config location

The user config lives under a per-user directory based on the configured app
name (currently `bz3`). The location is resolved in
`src/engine/common/data_path_resolver.*`:

- Windows: `%APPDATA%/<appName>` (fallback: `%USERPROFILE%/AppData/Roaming/<appName>`)
- macOS: `~/Library/Application Support/<appName>`
- Linux: `$XDG_CONFIG_HOME/<appName>` (fallback: `~/.config/<appName>`)

The config file is `config.json` under this directory. It is created if missing
and initialized to an empty object (`{}`).

## User world downloads

World data received from a server is stored under:

`<userConfigDir>/worlds/<host>.<port>/`

This location is created on demand using
`EnsureUserWorldDirectoryForServer(...)` in
`src/engine/common/data_path_resolver.*`.

## Asset lookup

Assets are resolved by key rather than by hardcoded file paths:

- Config layers can define `assets.*` and `fonts.*`.
- These entries are flattened into a key → path map.
- Both full keys (`models.tank`) and leaf keys (`tank`) are accepted.

Resolution flow:

1) If `ConfigStore` is initialized, asset keys are resolved from the merged
   config layers (`ResolveConfiguredAsset` in
   `src/engine/common/data_path_resolver.cpp`).
2) If not, a fallback lookup is built from the `fallbackAssetLayers` defined in
   `src/game/common/data_path_spec.cpp` (common → client → user).
3) If still unresolved, the optional default relative path is used.

## Required files and keys

### Required config files

These files are loaded with `required=true` and are expected to exist:

- Client: `data/common/config.json`, `data/client/config.json`
- Server: `data/common/config.json`, `data/server/config.json`

The engine default layer (`src/engine/data/config.json`) is always loaded first
and supplies core defaults used by required-key reads.

### Required keys (read without fallbacks)

These keys are read via `ReadRequired*Config(...)` and must resolve to a valid
value across the merged layers:

- `language`
- `platform.WindowWidth`
- `platform.WindowHeight`
- `platform.WindowTitle`
- `graphics.theme`
- `graphics.skybox.Mode`
- `graphics.skybox.Cubemap.Name`
- `graphics.Camera.FovDegrees`
- `graphics.Camera.NearPlane`
- `graphics.Camera.FarPlane`
- `game.roamingCamera.MoveSpeed`
- `game.roamingCamera.FastMultiplier`
- `game.roamingCamera.LookSensitivity`
- `game.roamingCamera.InvertY`
- `game.roamingCamera.StartYawOffsetDeg`

These are currently provided by `src/engine/data/config.json` and
`data/client/config.json` in this repo. If you relocate or remove those layers,
ensure the required keys still exist in the merged config.

### Commonly used keys (with fallbacks/defaults)

These keys are read with defaults or optional lookups; they are not strictly
required but are commonly referenced across the codebase:

- `graphics.resolution.Width`
- `graphics.resolution.Height`
- `graphics.Fullscreen`
- `graphics.VSync`
- `platform.SdlVideoDriver`
- `network.ServerPort`
- `network.ConnectTimeoutMs`
- `network.ServerAdvertiseHost`
- `network.ServerHost`
- `serverName`
- `worldName`
- `ui.RenderScale`
- `ui.Validate`
- `gui.communityCredentials`
- `assets.hud.fonts.console.Extras`
- `assets.hud.fonts.console.Regular.Size`
- `assets.hud.fonts.console.Title.Size`
- `assets.hud.fonts.console.Heading.Size`
- `assets.hud.fonts.console.Button.Size`

## Practical notes

- Prefer asset keys via `ResolveConfiguredAsset(...)` over hardcoded paths.
- Config layers are additive and allow world-specific overrides without
  duplicating the full base config.
- If you change layer ordering or required keys, update this document and
  consider adding startup validation in the config store.
