# Execution Policy

## Purpose
This file is the canonical execution policy for delegated work:
- build/run/test command policy,
- validation gates,
- conflict coordination,
- handoff quality minimum.

Rewrite-level strategy and boundaries live in `AGENTS.md`.
Overseer-only coordination workflow lives in `docs/foundation/governance/overseer-playbook.md`.

## Canonical Document Ownership
- `docs/BOOTSTRAP.md`: canonical startup entrypoint.
- `AGENTS.md`: rewrite invariants and architecture ownership.
- `docs/foundation/policy/execution-policy.md` (this file): execution mechanics and validation policy.
- `docs/foundation/governance/overseer-playbook.md`: overseer startup/rotation/checkpoint protocol.
- `docs/projects/AGENTS.md` + `docs/projects/*.md`: project scopes and project-specific validation.
- `docs/foundation/policy/decisions-log.md`: durable decisions and rationale.

## Execution Root (Required)
- Standalone mode: if the current directory is the `m-rewrite` repository root (contains `abuild.py` and `docs/`), use unprefixed repo-relative paths (`AGENTS.md`, `docs/...`).
- Integration mode: if the current directory is workspace root (`bz3-rewrite/`), anchor first:

```bash
cd m-rewrite
```

- If staying at integration workspace root, prefix every project path with `m-rewrite/`.

## Demo Test Data Root (Required)
- Use `demo/` as the canonical tracked root for local test/demo fixtures and reusable state.
- Path conventions:
  - communities/webserver state: `demo/communities/*`
  - user-home/config state: `demo/users/*`
  - world fixtures and world archives: `demo/worlds/*`
- Prefer running local client/server/webserver verification against these paths.
- Do not rely on personal `~/.config/bz3` or ad-hoc `/tmp` locations for durable/reusable test fixtures.

## Conflict Hotspots (Coordinate Ownership)
- `src/engine/CMakeLists.txt`
- `src/game/CMakeLists.txt`
- `src/game/protos/messages.proto`
- `src/game/net/protocol.hpp`
- `src/game/net/protocol_codec/*`
- `docs/foundation/architecture/core-engine-contracts.md`

If multiple agents need a hotspot, assign one owner and queue merge order.

## Build and Isolation Policy (Required)
From repo root:
- Use `./abuild.py -c -d <build-dir>` for configure/build/test flows (`-d` required; omit `-c` only when intentionally reusing an already configured build dir).
- Do not run raw `cmake -S/-B` directly for delegated project work.
- Specialists must work only in overseer-assigned pre-provisioned build slots (`build-a*`); creating new build dirs is an overseer task.
- Local repo `./vcpkg` is mandatory for all delegated builds.
- External `VCPKG_ROOT` paths are not allowed for delegated builds.
- If `./vcpkg` is missing or not bootstrapped, treat as immediate blocker and notify the human/overseer before continuing any build/test execution.
- Overseer/workspace setup helpers (not specialist slice work):
  - Linux/macOS: `./scripts/setup.sh`
  - Windows: `scripts\\setup.bat`
- Overseer/workspace one-time bootstrap commands (run once from repo root, when setup is required):
  - `git clone https://github.com/microsoft/vcpkg.git vcpkg`
  - `./vcpkg/bootstrap-vcpkg.sh -disableMetrics`
- One-time migration note: older build dirs may still have `CMAKE_TOOLCHAIN_FILE` cached to a non-local vcpkg path; clear that build dir cache (`CMakeCache.txt`, `CMakeFiles/`) before reconfigure.
- Build dir names must start with `build-`; managed specialist slot pool is `build-a1` through `build-a8`.
- `build-dev` remains shared fallback and should be avoided for parallel specialist work.
- Build dirs can target defaults or explicit backend sets via `./abuild.py -b ...`.
- Default-first operator rule: use `./abuild.py -c -d <build-dir>` for most slices and omit `-b` unless backend variability is directly required by the scoped work.
- Canonical explicit runtime-select command form:
  - `./abuild.py -c -d <build-dir>` (default backend set; preferred for non-backend slices).
  - `./abuild.py -c -d <build-dir> -b bgfx,diligent` (single binary with runtime-selectable renderer backend).
  - `./abuild.py -c -d <build-dir> -b imgui,rmlui` (single binary with runtime-selectable UI backend).
  - `./abuild.py -c -d <build-dir> -b jolt,physx` (single binary with runtime-selectable physics backend).
- Backend-category selection behavior:
  - if a category is omitted, `abuild.py` uses that category default,
  - if one token in a category is provided, that backend becomes the active/default for that category,
  - if multiple tokens in a category are provided, `abuild.py` builds a runtime-selectable set for that category.
  - avoid multi-category backend lists unless each included category is required for the active validation scope.
- Each active specialist stays inside assigned slot(s) from `docs/projects/ASSIGNMENTS.md`.

### Agent Identity and Slot Ownership (Required)
- Every specialist packet must include:
  - explicit agent identity (for example: `specialist-renderer-parity`),
  - assigned build slot(s) (for example: `build-a5`, `build-a8`).
- Specialists must set identity before build/test execution:
  - `export ABUILD_AGENT_NAME=<agent-name>` (or pass `--agent <agent-name>` on each `abuild.py` call).
- Specialists must claim assigned slot(s) before first build in a session:
  - `./abuild.py --claim-lock -d <build-dir>`
- Specialists must release assigned slot(s) when retiring or transferring ownership:
  - `./abuild.py --release-lock -d <build-dir>`
- Default lock behavior is strict owner enforcement. `--ignore-lock` is emergency-only and requires overseer approval.

Wrapper gate policy:
- `./scripts/test-engine-backends.sh` and `./scripts/test-server-net.sh` accept optional build-dir args and default to `build-dev`.
- In parallel delegated work, always pass explicit build dirs.
- `build-dev` default runs are serialized shared resources.
- Wrapper internals may still use direct `cmake`/`ctest`; this does not change operator policy above.

## Platform Backend Expansion Admission (Required for non-SDL3 work)
- SDL3 remains the default active platform backend unless a separate explicit default-switch acceptance is approved.
- No second-backend implementation work starts until all entry criteria are met:
  - concrete blocker documented (cannot be resolved inside SDL3-only policy),
  - named owner assigned with isolated build dirs,
  - proposal includes seam changes, validation commands, and rollback plan,
  - overseer approval recorded.
- Required seam invariants:
  - engine-facing platform contract stays centered on `karma::platform::Window`,
  - `./scripts/check-platform-seam.sh` stays passing,
  - backend headers/types stay out of `src/game/*` and engine/game-facing public contracts,
  - no dormant/stub-only backend trees.
- Required conformance evidence (in one handoff):
  - baseline: `./abuild.py -c -d build-a1`
  - baseline wrapper: `./scripts/test-engine-backends.sh build-a1`
  - candidate build: `./abuild.py -c -d build-a2 -b <candidate>,bgfx,jolt,rmlui,sdl3audio` (or explicitly approved equivalent)
  - candidate wrapper: `./scripts/test-engine-backends.sh build-a2` (or explicitly approved equivalent)
- Reject speculative backend work when:
  - blocker/proposal/approval is missing,
  - backend leakage breaks seam policy,
  - renderer/network/gameplay scope is pulled in,
  - any conformance gate fails.

## Validation Gates
Global baseline:

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
1. Pick one active project doc listed in `docs/projects/ASSIGNMENTS.md`.
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
