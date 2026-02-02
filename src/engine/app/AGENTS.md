# src/engine/app/AGENTS.md

Read `src/AGENTS.md` and `src/engine/AGENTS.md` first.
This directory contains the **engine runtime shell**: the minimal glue that
creates and orchestrates engine subsystems during startup and shutdown.

## Purpose
`app/` is where Karma’s runtime lifecycle is defined. It’s intentionally small
and generic so any game can plug in. The key idea is:

- **Engine owns systems** (window, render device, audio, physics, input, network)
- **Game supplies hooks** (init/update/shutdown, and any per-frame logic)

In BZ3, the game layer adapts this through `ClientEngine` and server main loops.

## Key files
- `engine_app.hpp` / `engine_app.cpp`
  - Defines `EngineApp` and its core loop.
  - Owns the `EngineContext` object (the struct that holds subsystem pointers).
  - Intended to be the public entry point once Karma is standalone.

## How it connects to game code
- `EngineApp` is called by game entrypoints (client/server main).
- The game provides a `GameInterface` or adapter layer that responds to
  lifecycle calls.
- Engine code does not import game logic directly; the dependency direction is
  always **game → engine**.

## Common tasks
- **Add a subsystem**: extend `EngineContext` and initialize it here.
- **Change lifecycle**: modify ordering in `EngineApp` init/update/shutdown.
- **Expose engine capabilities to game**: add accessors in `EngineContext`.

## Gotchas
- Keep this code **game-agnostic**.
- Input/event pumping and overlay updates live here, but game-specific input
  gating and action handling still belong in `src/game/`.
