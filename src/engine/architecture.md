# src/engine/architecture.md

This document describes how Karma is structured and how its subsystems connect.
For subsystem-level architecture, see each subdirectory’s `architecture.md`.

## Engine runtime layers

1) **Platform / Windowing** (`platform/`)
   - SDL-backed window and event capture.

2) **Core / Common** (`core/`, `common/`)
   - Shared types, config store, data paths, i18n.

3) **Systems**
   - **Graphics** (`graphics/` + `renderer/`)
   - **Physics** (`physics/`)
   - **Audio** (`audio/`)
   - **Input mapping** (`input/`)
   - **Networking transport** (`network/`)

4) **App shell** (`app/`)
   - Orchestrates initialization and ties subsystems together.

Optional UI frontends and helpers live in `src/karma-extras/` and are consumed
by game code, not the engine core.

## Key integration points
- **Renderer ↔ Graphics backends**: `graphics` owns backend selection and GPU
  resources; `renderer` orchestrates scene and render passes.
- **UI ↔ Graphics**: Game-provided UI layers fill `UIContext` draw data; the
  renderer consumes that draw data via backend-agnostic paths.
- **Input ↔ Platform**: platform events feed the input mapper; input provides
  action-based queries for game code.
- **Physics ↔ Game**: physics backends provide a consistent API; game code uses
  physics objects without knowing which backend is selected.

## Public headers
Public engine headers live under `include/karma/` and point directly at
`src/engine/...`. When Karma is split out, these headers become the installed
API surface.
