# Rewrite Invariants

## Purpose
This file defines rewrite-level invariants and ownership boundaries.

Execution mechanics (build commands, wrapper gates, handoff mechanics) are defined in `docs/foundation/policy/execution-policy.md`.
Overseer workflow is defined in `docs/foundation/governance/overseer-playbook.md`.
Quick startup orientation is in `docs/BOOTSTRAP.md`.

## Rewrite Context
This workspace hosts the production rewrite of BZ3.

Primary target:
- Rebuild `m-dev` user-facing behavior inside a clean, engine-owned architecture in `m-rewrite`.
- Keep architecture ownership clear: engine drives lifecycle/subsystems; game submits game-specific logic/content.

## Product Direction (Non-Negotiable)
- Implement an engine-owned lifecycle model similar to modern engines (Unity/Unreal/Godot style ownership boundaries).
- Optimize for rapid game creation by moving game-agnostic behavior into engine defaults whenever contract-safe.
- Follow a default-first policy: engine covers the common path (~95% of scenarios) with explicit escape hatches for uncommon cases.
- Keep game-facing surface area small: game should primarily define content/rules and consume engine contracts.

## Dual-Track Strategy (Non-Negotiable)
- Track A: deliver modern BZ3 behavior parity against `m-dev`.
- Track B: continuously ingest significant `q-karma` capabilities under rewrite-owned contracts.
- Port capability outcomes, never upstream file layout/structure.
- Rewrite priorities remain authoritative; q-karma intake is continuous but bounded.

Repository roles:
- `m-rewrite/`: production rewrite codebase (active implementation target).
- `m-dev/`: behavior/reference baseline (source-of-truth for parity, not architecture to copy).
- `q-karma/`: capability-intent reference (not structure template).
- `m-rewrite/docs/`: canonical rewrite guidance, decisions, and project specs.

## Workspace Scope (Required)
- Standalone mode (this repo only): treat repo root as the active codebase for edits, builds, and git operations.
- Integration mode (`bz3-rewrite/` with sibling repos): treat `m-rewrite/` as the only active codebase for edits, builds, and git operations.
- In integration mode, run git commands from `m-rewrite/` (or with `git -C m-rewrite ...`) and do not stage/commit from workspace root.
- In integration mode, workspace root (`bz3-rewrite/`) is bootstrap-only and should contain only `README.md` plus sibling repos (`m-rewrite/`, `m-dev/`, `q-karma/`).

## Core Architecture Contract
- `src/engine/` is game-agnostic and owns loop/lifecycle/timing.
- `src/game/` contains BZ3-specific game logic and host usage.
- `src/webserver/` is a game-agnostic community-management sidecar service.
  - It is not `src/game/` ownership and should remain generic across games.
  - It is not engine runtime SDK surface (`src/engine/`), but it is part of the engine ecosystem toolchain.
- Input, world ownership, render loop, and subsystem lifetime are engine-owned.
- Config is authoritative; required values must be read via strict helpers.
- Render layering is stable (`kLayerWorld = 0`, `kLayerUI = 1000`).
- UI policy: engine supports ImGui + RmlUi; game intentionally chooses one primary UI backend.

## Backend Exposure Contract
- Window/render/physics/audio backends are engine-internal.
- Game code must consume engine contracts, not backend APIs.
- Backend-specific complexity stays in backend layers.
- Exception: game UI may directly target chosen UI backend (`imgui` or `rmlui`).

## Rewrite-Level Non-Goals
- Do not mirror `m-dev`/`q-karma` file layouts.
- Do not prioritize full gameplay feature parity before core infra contracts are stable.
- Do not move gameplay semantics into engine-core subsystems.

## Documentation Precedence
1. `docs/AGENTS.md` (rewrite invariants and ownership boundaries)
2. `docs/foundation/policy/execution-policy.md` (execution mechanics and validation policy)
3. `docs/foundation/governance/overseer-playbook.md` (overseer-only coordination protocol)
4. `docs/projects/<project>.md` (project-specific scope/spec/validation)
5. `docs/foundation/policy/decisions-log.md` (durable strategy/policy decisions and rationale)

## Startup Read Order
1. `docs/BOOTSTRAP.md` (startup entrypoint)
2. `docs/AGENTS.md`
3. `docs/foundation/policy/execution-policy.md`
4. `docs/projects/AGENTS.md`
5. `docs/projects/ASSIGNMENTS.md`
6. Relevant `docs/projects/<project>.md`
7. `docs/foundation/policy/decisions-log.md`

## Practical Notes
- Runtime data root is `m-rewrite/data/`.
- If a local user config still points to old top-level `data/`, startup fails until overridden/updated.
