# Make Karma Standalone (Repo Extraction Checklist)

This is a concrete checklist to make `src/engine/` fully standalone in a future `karma` repo.

## 1) Top‑level build system for the engine
- Add a root `CMakeLists.txt` that:
  - defines `KARMA_*` options (render/ui/audio/physics/window backends).
  - pulls in third‑party deps (bgfx/diligent/rmlui/imgui/jolt/physx/etc.).
  - includes `src/engine/CMakeLists.txt`.
  - exposes `karma` / `karma_server` targets (object libs or static libs).

## 2) Runtime data layout
- Move or mirror required `data/common/...` assets into the engine repo, or:
  - make the engine data root configurable via env var + docs.
- Ensure `data/common/config.json` exists in the engine repo (or replace with engine defaults).

## 3) Engine config layer
- Keep `src/engine/data/config.json` in the engine repo.
- Update the config layer path to be repo‑relative (or data‑root‑relative) instead of hardcoded `src/engine/...`.

## 4) Dependency management
- Bring `vcpkg.json`, `vcpkg-configuration.json`, and overlays into the engine repo.
- Or replace with submodules + `find_package` logic.

## 5) Install/Export
- Install/export headers under `include/karma/`.
- Install engine data (config + assets).
- Provide a `karmaConfig.cmake` for downstream game builds.

## 6) Build script / presets
- Add `karma-build.py` or `CMakePresets.json` to select backends and build `karma`/`karma_server`.

## 7) Smoke tests for the standalone repo
- Build targets: `karma`, `karma_server`.
- If sample apps exist, run a minimal render path (skybox + one mesh).
- Validate config loading order:
  - `src/engine/data/config.json` is loaded before app/game overrides.
  - Missing required keys produce a hard failure (as intended).

## 8) Minimal sample app (optional but helpful)
- Add a tiny engine‑only sample (`karma-demo`) that:
  - opens a window
  - initializes graphics + render core
  - draws a simple mesh or skybox
- This validates the engine repo without requiring the game.

## 9) Documentation & versioning
- Document required config keys for engine runtime.
- Document supported backend combinations.
- Decide semantic versioning scheme for engine releases.
