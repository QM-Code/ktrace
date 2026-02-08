# src/engine/common/AGENTS.md

Read `src/AGENTS.md` and `src/engine/AGENTS.md` first.
This directory is the **engine’s shared utility layer**: config, data paths,
i18n, file helpers, and global setup.

## Key responsibilities
- Config store and helpers (required-config reads, layered configs)
- Data root resolution (host-defined env var via `DataPathSpec`) and asset lookup
- i18n string loading and language selection
- File utilities and basic helpers
- Global setup (curl init, stb image impl)

## Key files
- `config_store.*`
  - Central config system: layered JSONs, change tracking, persistence.
  - Game and engine both read config through this API.

- `config_helpers.*`
  - `ReadRequired*` helpers that throw if config keys are missing.

- `data_path_resolver.*` / `data_dir_override.*`
  - Resolve data root and layered asset paths.

- `i18n.*`
  - Loads language JSON files and provides string lookup.

- `file_utils.*`
  - Small filesystem helper routines used across engine.

## How it connects to game code
- Game config lives in `m-rewrite/data/` and is **merged by ConfigStore**.
- Game UI reads and writes config through the engine store.
- Game uses i18n for UI strings.

## Gotchas
- Config is layered; always use `ConfigStore` instead of raw file I/O.
- `ReadRequired*` throws on missing config; ensure engine defaults exist.
