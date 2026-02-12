# Projects Index (docs/projects)

This is the delegation entrypoint for rewrite work.

## Strategic Directive (Required)
- Run two tracks in parallel:
  - `m-dev` behavior parity needed for the modern BZ3 game path in `m-rewrite`,
  - significant engine capability intake from `KARMA-REPO` under rewrite-owned contracts.
- Keep rewrite architecture ownership intact:
  - capability parity by behavior/outcome, never by file/layout mirroring,
  - game rules/protocol semantics remain game-owned unless explicitly re-scoped.
- If prioritization conflicts occur, choose slices that best unblock both tracks (or clearly document why one track is being deferred).

## How To Use This Folder
1. Read `AGENTS.md` (repo root).
2. Read `docs/AGENTS.md`.
3. Select one project file below.
4. Use that single file as the project-level source of truth.
5. Start from its `Project Snapshot` section.

## Project Files
- `audio-backend.md`
- `content-mount.md`
- `core-engine-infrastructure.md`
- `engine-defaults-architecture.md`
- `engine-backend-testing.md`
- `engine-game-boundary-hygiene.md`
- `engine-network-foundation.md`
- `gameplay-netcode.md`
- `physics-backend.md`
- `platform-backend-policy.md`
- `renderer-parity.md`
- `server-network.md`
- `testing-ci-docs.md`
- `ui-integration.md`

## Current Focus Board
- `engine-network-foundation.md`: priority/ready, move generic transport/session boilerplate into engine-owned contracts while keeping game protocol semantics game-owned.
- `core-engine-infrastructure.md`: in progress, backbone planning and dependency sequencing.
- `renderer-parity.md`: priority/in progress, complete queued P0 integrity/signature continuity then execute merged P1 visual-quality slices (scene shadows + stable distance texture quality) under backend-parity guardrails.
- `engine-defaults-architecture.md`: in progress, architecture baseline is now codified in `docs/architecture/ENGINE_DEFAULTS_MODEL.md`; next convert scheduling and component-catalog guidance into implementation slices.
- `engine-game-boundary-hygiene.md`: queued (P2 low-medium; ready to assign), capture and execute bounded extraction of game-side boilerplate into engine-owned scaffolding contracts without moving gameplay/protocol semantics.
- `server-network.md`: queued, continue runtime/protocol hardening after the engine-network-foundation first slice lands.
- `physics-backend.md`: queued, continue backend parity while aligning to architecture/defaults decisions.
- `platform-backend-policy.md`: queued (P2 medium; ready to assign), execute Slice 1 SDL3-only policy inventory/removal plan while preserving thin engine-owned platform seam and future adapter contract boundary.
- `audio-backend.md`: queued, continue backend parity while aligning to architecture/defaults decisions.
- `content-mount.md`: queued, continue after top-priority renderer/network milestones unless it blocks runtime stability.
- `ui-integration.md`: queued, continue after top-priority renderer/network milestones unless it blocks validation.
- `gameplay-netcode.md`: queued, continue prediction/reconciliation after engine-network-foundation boundary is established.
- `engine-backend-testing.md`: in progress, keep backend tests authoritative as implementation deepens.
- `testing-ci-docs.md`: in progress, keep wrappers/CI/docs synchronized as tracks evolve.

## Shared Files
- `PROJECT_TEMPLATE.md` (use to create new project docs)
- `ASSIGNMENTS.md` (active owner/status board for project delegation)

## Assignment Pattern
- Prefer one agent per project.
- Avoid concurrent edits to conflict hotspots listed in `docs/AGENTS.md`.
- If cross-project edits are required, name a single integration owner.
- Update `ASSIGNMENTS.md` in the same handoff when owner/status/next-task changes.

## Assigned Build Profiles
- Always use `./bzbuild.py <build-dir>` from `m-rewrite/`.
- Do not use raw `cmake -S/-B` for delegated work.
- Standard isolated pairs:
  - Physics: `build-sdl3-bgfx-jolt-rmlui-sdl3audio`, `build-sdl3-bgfx-physx-rmlui-sdl3audio`
  - Audio: `build-sdl3-bgfx-jolt-imgui-sdl3audio`, `build-sdl3-bgfx-jolt-imgui-miniaudio`
  - Renderer: `build-sdl3-bgfx-physx-imgui-sdl3audio`, `build-sdl3-diligent-physx-imgui-sdl3audio`
  - Engine-network foundation: `build-sdl3-bgfx-jolt-rmlui-miniaudio`, `build-sdl3-bgfx-physx-rmlui-miniaudio`

## Hygiene
- Run docs lint after structural doc changes:
  - `./docs/scripts/lint-project-docs.sh`
- Legacy snapshots are preserved under `docs/archive/` and are reference-only.
