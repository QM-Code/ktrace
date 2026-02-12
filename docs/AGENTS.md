# AGENTS.md (docs)

## Purpose
This file is the canonical execution policy for delegated work:
- build/run/test command policy,
- validation gates,
- conflict coordination,
- handoff quality minimum.

Rewrite-level strategy and boundaries live in `AGENTS.md`.
Overseer-only coordination workflow lives in `docs/OVERSEER.md`.

## Canonical Document Ownership
- `docs/STARTUP_BRIEF.md`: quick startup checklist.
- `AGENTS.md`: rewrite invariants and architecture ownership.
- `docs/AGENTS.md` (this file): execution mechanics and validation policy.
- `docs/OVERSEER.md`: overseer startup/rotation/checkpoint protocol.
- `docs/projects/README.md` + `docs/projects/*.md`: project scopes and project-specific validation.
- `docs/DECISIONS.md`: durable decisions and rationale.

## Execution Root (Required)
- Delegated sessions may begin at workspace root (`bz3-rewrite/`) where shorthand paths are ambiguous.
- Before reading docs or running project commands, anchor to repo root:

```bash
cd m-rewrite
```

- After anchoring, unprefixed paths are repo-relative (`AGENTS.md`, `docs/...`).
- If staying at workspace root, prefix all paths with `m-rewrite/`.

## Conflict Hotspots (Coordinate Ownership)
- `m-rewrite/src/engine/CMakeLists.txt`
- `m-rewrite/src/game/CMakeLists.txt`
- `m-rewrite/src/game/protos/messages.proto`
- `m-rewrite/src/game/net/protocol.hpp`
- `m-rewrite/src/game/net/protocol_codec.cpp`
- `docs/projects/core-engine-infrastructure.md`

If multiple agents need a hotspot, assign one owner and queue merge order.

## Build and Isolation Policy (Required)
From `m-rewrite/`:
- Use `./bzbuild.py <build-dir>` for configure/build/test flows.
- Do not run raw `cmake -S/-B` directly for delegated project work.
- Build dir names must follow `build-<platform>-<renderer>-<physics>-<ui>-<audio>`.
- Valid platform token is `sdl3` (not `sdl`).
- Each active specialist stays inside assigned isolated build dirs.

Standard isolated pairs:
- Physics:
  - `build-sdl3-bgfx-jolt-rmlui-sdl3audio`
  - `build-sdl3-bgfx-physx-rmlui-sdl3audio`
- Audio:
  - `build-sdl3-bgfx-jolt-imgui-sdl3audio`
  - `build-sdl3-bgfx-jolt-imgui-miniaudio`
- Renderer:
  - `build-sdl3-bgfx-physx-imgui-sdl3audio`
  - `build-sdl3-diligent-physx-imgui-sdl3audio`
- Engine-network foundation:
  - `build-sdl3-bgfx-jolt-rmlui-miniaudio`
  - `build-sdl3-bgfx-physx-rmlui-miniaudio`

Wrapper gate policy:
- `./scripts/test-engine-backends.sh` and `./scripts/test-server-net.sh` accept optional build-dir args and default to `build-dev`.
- In parallel delegated work, always pass explicit build dirs.
- `build-dev` default runs are serialized shared resources.
- Wrapper internals may still use direct `cmake`/`ctest`; this does not change operator policy above.

## Validation Gates
Global baseline (from `m-rewrite/`):

```bash
./scripts/test-engine-backends.sh
./scripts/test-server-net.sh
```

Mandatory by touch scope:
- Touching `src/game/server/*`, `src/game/server/net/*`, or `src/game/net/*`:

```bash
./scripts/test-server-net.sh <build-dir>
```

- Touching `src/engine/physics/*`, `src/engine/audio/*`, `include/karma/physics/*`, `include/karma/audio/*`, or backend-test wiring in `src/engine/CMakeLists.txt`:

```bash
./scripts/test-engine-backends.sh <build-dir>
```

- Cross-scope changes: run both wrappers with explicit build dirs.
- Project docs can add stricter gates; project-specific requirements are authoritative for that project.

## Debug and Trace Rules
Debugging tenets:
- Prefer trace-first divergence isolation over intuition-only edits.
- If 1-2 quick fixes fail, switch to sequence comparison against known-good traces.

Logging rules:
- Use `-v` for debug-level logs.
- Use `-t <comma-separated-channels>` for trace channel selection.
- Use `KARMA_TRACE` / `KARMA_TRACE_CHANGED` (not raw `spdlog::trace`).
- Keep base channels high-signal and move per-frame noise into leaf channels.
- Gate expensive trace computations with `ShouldTraceChannel()` or `KARMA_TRACE_CHANGED`.

Trace quick reference:
- server/network: `engine.server,net.server,net.client`
- simulation: `engine.sim,engine.sim.frames`
- audio: `audio.system,audio.sdl3audio,audio.miniaudio`
- ui: `ui.system,ui.system.overlay,ui.system.imgui,ui.system.rmlui`
- input/events: `input.events`

## Delegation Workflow (All Specialists)
1. Pick one project doc from `docs/projects/README.md`.
2. Confirm owned paths and non-goals from that project doc.
3. Implement within project boundaries.
4. Run required validation for touched scope and project gates.
5. Update project `Project Snapshot` (`Current owner`, `Status`, `Immediate next task`).
6. Update `docs/projects/ASSIGNMENTS.md` for owner/status/next-task changes.
7. Update status and handoff notes with exact commands/results.
8. If contracts changed, update linked docs in the same change.

## Handoff Minimum
- Scope: what changed and what was intentionally not touched.
- Validation: exact commands run and outcomes.
- Risk: open issues, assumptions, blockers.
- Docs: project doc and assignment row updated when required.

## Docs Lint
After structural docs changes:

```bash
./docs/scripts/lint-project-docs.sh
```
