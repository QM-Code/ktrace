# src/engine/app/architecture.md

## Role
`EngineApp` is the top-level runtime orchestrator. It is intentionally small and
should remain free of game-specific logic.

## Core responsibilities
- Create and wire the engine subsystems.
- Own the main loop (update/tick).
- Provide an `EngineContext` to game code.
- Pump input + overlay events and advance overlay updates each tick.

## Integration
The game implements a minimal interface or adapter, and `EngineApp` drives it.
All game code sits *outside* the engine, calling into the engine through the
context.
