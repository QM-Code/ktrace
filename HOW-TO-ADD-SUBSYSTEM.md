# How to Add a New Subsystem

This guide describes the standard pattern for adding a new engine subsystem
that can be selected via backends (audio, physics, graphics, etc.).
Use it as a template to keep engine/game separation consistent.

## 1) Decide the split: engine vs game

- Engine responsibilities: low-level interfaces, backend selection, and
  platform-agnostic integration.
- Game responsibilities: BZ3-specific orchestration, gameplay usage, and UI.

If the subsystem is reusable across games, put the core API under `src/engine/`.
If it is gameplay-specific, keep it under `src/game/`.

## 2) Create the public interface

- Add a new folder under `src/engine/<subsystem>/`.
- Define the public interface in a header (e.g., `device.hpp`,
  `physics_world.hpp`).
- Keep the API small and backend-agnostic; avoid game types.

## 3) Add backend implementations

- Create `src/engine/<subsystem>/backends/<name>/`.
- Implement the interface for each backend.
- Keep backend-specific includes and types inside the backend folder.

## 4) Add a backend factory

- Add `src/engine/<subsystem>/backend_factory.cpp`.
- Read the compile-time selection macro (e.g., `KARMA_<SUBSYSTEM>_BACKEND`).
- Return the correct backend implementation or fail with a clear error.

## 5) Wire build configuration

- Add the backend selection option to CMake (e.g.,
  `KARMA_<SUBSYSTEM>_BACKEND=<name>`).
- Ensure backend-specific dependencies are guarded by the option.
- Update `CMakeLists.txt` and any build scripts/presets.

## 6) Expose the subsystem to the engine orchestrator

- Add a pointer/member in `src/game/engine/client_engine.*` and/or
  `src/game/engine/server_engine.*`.
- Initialize it in the engine constructor, destroy it in the destructor.
- Add update hooks if needed (early/step/late).

## 7) Add config keys and defaults

- Add default keys to `src/engine/data/config.json` when the subsystem
  needs engine-level defaults.
- Add BZ3-specific overrides to `data/common` or `data/client`/`data/server`.
- Update `CONFIG-SCHEMA.md` if any keys are required.

## 8) Use asset keys, not paths

- Declare asset keys in `assets.*` or `fonts.*` in config.
- Resolve via `ResolveConfiguredAsset(...)` in code.

## 9) Add minimal validation and logging

- Fail fast if required backends or config keys are missing.
- Log backend selection, resource init, and shutdown events.

## 10) Update docs

- Add a short note to `architecture.md` if the subsystem changes runtime flow.
- Add usage notes to `README.md` if there are new build options.
- Keep `AGENTS.md` updated if the subsystem adds common workflows.
