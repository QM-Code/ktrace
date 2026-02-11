# AGENTS.md (docs)

## Purpose
This file defines the shared execution model for delegated rewrite work.

Use this file for:
- debugging and trace workflow
- logging standards
- validation/test gates
- handoff quality bar

Project-specific scope lives in `docs/projects/<project>.md`.

## Documentation Hierarchy
1. `AGENTS.md` (root): rewrite context and non-negotiable boundaries.
2. `docs/AGENTS.md` (this file): common execution rules for all delegated work.
3. `docs/OVERSEER.md`: rotation protocol and overseer coordination model.
4. `docs/projects/<project>.md`: project mission, owned paths, interfaces, validation, and handoff notes.

## Execution Root (Required)
- Delegated sessions may begin at workspace root (`bz3-rewrite/`) where `AGENTS.md` shorthand is ambiguous.
- Before reading docs or running project commands, anchor to repo root:

```bash
cd m-rewrite
```

- After anchoring, unprefixed paths in packets/checklists are repo-relative (`AGENTS.md`, `docs/...`).
- If staying at workspace root, all packet/checklist paths must be prefixed with `m-rewrite/`.

## Big Picture
- `m-rewrite/src/engine/*`: engine-owned lifecycle/subsystems/contracts.
- `m-rewrite/src/game/*`: BZ3-specific game/client/server behavior.
- `m-rewrite/data/*`: runtime baseline data/config/assets.
- `docs/projects/*`: per-project execution specs and status.

## Shared North Star (All Delegated Work)
- Recreate `m-dev` user-facing behavior inside the `m-rewrite` engine-owned architecture.
- Prefer engine-level defaults over repeated game-side setup.
- Design for a `95% default path / 5% explicit override path`.
- Promote reusable game-agnostic behavior from `src/game/*` into `src/engine/*` when contract-safe.
- Keep backend details engine-internal; game code consumes stable contracts.

## Strategic Dual-Track Directive (All Delegated Work)
- Track A: keep rewrite behavior parity with `m-dev` outcomes needed for the modern BZ3 game path.
- Track B: continuously ingest significant `KARMA-REPO` engine capability deltas under rewrite-owned contracts.
- Never trade architecture ownership for parity speed:
  - do not mirror `KARMA-REPO` layout/structure,
  - do not move gameplay/protocol semantics out of game-owned boundaries.
- Every active project/slice should be explainable as:
  - parity delivery for `m-dev`, or
  - capability uplift from `KARMA-REPO`, or
  - required infra that unblocks both.

## KARMA Capability Intake (All Delegated Work)
- `KARMA-REPO` is a capability-intent reference only; it is never a structural template.
- Intake rule:
  - identify significant engine capability deltas from `KARMA-REPO`,
  - restate them as rewrite-owned contracts/tasks in `docs/projects/*`,
  - implement in `m-rewrite` with rewrite architecture ownership.
- Selection rule:
  - prioritize intake slices that improve engine defaults or unblock rewrite parity work,
  - defer low-impact experiments and backend-breadth churn.
- Boundary rule:
  - do not mirror `KARMA-REPO` layouts or backend decisions directly,
  - preserve engine-owned lifecycle/contracts and game-owned gameplay/protocol semantics.
- Durability rule:
  - after each adopted capability, update rewrite docs so direction remains local if `KARMA-REPO` is removed.

## Conflict Hotspots (Coordinate)
- `m-rewrite/src/engine/CMakeLists.txt`
- `m-rewrite/src/game/CMakeLists.txt`
- `m-rewrite/src/game/protos/messages.proto`
- `m-rewrite/src/game/net/protocol.hpp`
- `m-rewrite/src/game/net/protocol_codec.cpp`
- `docs/projects/core-engine-infrastructure.md`

If two agents need the same hotspot, assign a single owner and queue merge order.

## Debugging Tenets
- Do not rely on code-reading intuition alone; use traces to isolate first divergence.
- Compare failing path against known-good path with the same trace channels.
- If 1-2 quick fixes fail, switch to trace-first sequence comparison.

## Logging Rules
- Use `-v` for debug-level logs.
- Use `-t <comma-separated-channels>` for trace channels.
- Use `KARMA_TRACE` / `KARMA_TRACE_CHANGED` (not raw `spdlog::trace`).
- Keep base channels high-signal (lifecycle/state transitions).
- Put per-frame/high-frequency logs in leaf channels.
- Gate expensive trace computations with `ShouldTraceChannel()` or `KARMA_TRACE_CHANGED`.

## Global Validation Baseline
From `m-rewrite/`:

```bash
./scripts/test-engine-backends.sh
./scripts/test-server-net.sh
```

## Mandatory Test Gates By Touch Scope
From `m-rewrite/`:

- If touching `src/game/server/*`, `src/game/server/net/*`, or `src/game/net/*`:

```bash
./scripts/test-server-net.sh
```

- If touching `src/engine/physics/*`, `src/engine/audio/*`, `include/karma/physics/*`, `include/karma/audio/*`, or backend test wiring in `src/engine/CMakeLists.txt`:

```bash
./scripts/test-engine-backends.sh
```

- For cross-project changes: run both wrappers.

## Build Isolation Policy (Required)
From `m-rewrite/`:

- Use `./bzbuild.py <build-dir>` for configure/build/test flows.
- Do not run raw `cmake -S/-B` directly for delegated project work.
- Build dir names must match:
  - `build-<platform>-<renderer>-<physics>-<ui>-<audio>`
  - valid platform token here is `sdl3` (not `sdl`).
- Each active agent must stay inside their assigned build directory pair to avoid build interference.

Current standard pairs:
- Physics:
  - `build-sdl3-bgfx-jolt-rmlui-sdl3audio`
  - `build-sdl3-bgfx-physx-rmlui-sdl3audio`
- Audio:
  - `build-sdl3-bgfx-jolt-imgui-sdl3audio`
  - `build-sdl3-bgfx-jolt-imgui-miniaudio`

Wrapper gate policy:
- `./scripts/test-engine-backends.sh` and `./scripts/test-server-net.sh` accept an optional build directory argument and default to `build-dev`.
- Use `./scripts/test-engine-backends.sh <build-dir>` and `./scripts/test-server-net.sh <build-dir>` to avoid shared-directory collisions during parallel delegated work.
- If using the default `build-dev` path, treat wrapper runs as a serialized shared resource.
- Wrapper internals still use direct `cmake`/`ctest` because wrapper-specific target selection and server/net test filtering are not fully exposed through `bzbuild.py` yet.

## Build/Run Quick Reference
From `m-rewrite/`:

```bash
./bzbuild.py -a
./build-dev/bz3 --backend-render bgfx --backend-ui imgui
./build-dev/bz3-server -d /home/karmak/dev/bz3-rewrite/m-rewrite/data -p 11911 -w common
```

## Trace Channel Quick Reference
- server/network: `engine.server,net.server,net.client`
- simulation: `engine.sim,engine.sim.frames`
- audio: `audio.system,audio.sdl3audio,audio.miniaudio`
- ui: `ui.system,ui.system.overlay,ui.system.imgui,ui.system.rmlui`
- input/events: `input.events`

## Delegation Workflow
1. Pick one project file from `docs/projects/README.md`.
2. Confirm owned paths and non-goals in that project file.
3. Implement changes within that boundary.
4. Run project-required validation.
5. Update `Project Snapshot` fields (`Current owner`, `Status`, `Immediate next task`).
6. Update `docs/projects/ASSIGNMENTS.md` row for that project.
7. Update project status/handoff notes.
8. If contracts changed, update linked docs in the same change.

## Accepted Slice Persistence Gate (Required)
- Overseer owns git checkpointing for accepted slices in `m-rewrite/`.
- Minimum cadence:
  - run `./scripts/overseer-checkpoint.sh -m "<slice batch summary>" --all-accepted`,
  - require successful exit before any new specialist assignment is sent.
- Do not leave accepted multi-slice batches uncommitted for long; frequent push-backed checkpoints are required for outage recovery.

## Docs Lint
After structural documentation changes, run:

```bash
./docs/scripts/lint-project-docs.sh
```

## Session Memory
For durable project memory across agent/session rotation:
- `docs/DECISIONS.md` for project-level decisions and rationale.
- `docs/OVERSEER.md` for role bootstrap + delegation protocol.

## Handoff Minimum
- Scope: what changed and what was intentionally not touched.
- Validation: exact commands run + result.
- Risk: known open issues, assumptions, blockers.
- Docs: project file updated when semantics changed.
