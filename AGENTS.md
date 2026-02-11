# AGENTS.md (bz3-rewrite)

## Rewrite Context
This workspace hosts the production rewrite of BZ3.

Primary target:
- Rebuild `m-dev` user-facing behavior inside a clean, engine-owned architecture in `m-rewrite`.
- Keep architecture ownership clear: engine drives lifecycle/subsystems; game submits game-specific logic/content.

## Engine Product Direction (Must Preserve)
- Implement an engine-owned lifecycle model similar to modern engines (Unity/Unreal/Godot style ownership boundaries).
- Optimize for rapid game creation by moving game-agnostic behavior into engine defaults whenever contract-safe.
- Follow a default-first policy: engine covers the common path (~95% of scenarios) with explicit escape hatches for uncommon cases.
- Keep game-facing surface area small: game should primarily define content/rules and consume engine contracts.
- Use `m-dev` for behavior parity, and `KARMA-REPO` for capability ideas, without inheriting their structure.

Repository roles:
- `m-rewrite/`: production rewrite codebase (active implementation target).
- `m-dev/`: behavior/reference baseline (source-of-truth for parity, not architecture to copy).
- `KARMA-REPO/`: exploratory reference (capability ideas, not structure template).
- `m-rewrite/docs/`: canonical rewrite guidance and project specs.

## KARMA Capability Intake Policy (Must Preserve)
- Treat `KARMA-REPO` as a feature-intent upstream, not an architecture upstream.
- Continue two tracks in parallel:
  - port and stabilize `m-dev` behavior in rewrite-owned engine/game boundaries,
  - adopt significant new engine capabilities discovered in `KARMA-REPO` under rewrite-owned contracts.
- Port capability outcomes, not file layout, backend sprawl, or local implementation shape.
- Do not block rewrite progress waiting on `KARMA-REPO`; intake is continuous but bounded by rewrite priorities.
- End state target: rewrite docs/contracts carry full direction so `KARMA-REPO` can be removed without roadmap loss.

## Workspace Scope (Must Follow)
- Treat `m-rewrite/` as the only active codebase for edits, builds, and git operations.
- Run git commands from `m-rewrite/` (or with `git -C m-rewrite ...`).
- Do not stage/commit from workspace root.
- Workspace root (`bz3-rewrite/`) is bootstrap-only and should contain only `README.md` plus sibling repos (`m-rewrite/`, `m-dev/`, `KARMA-REPO/`).
- All active control docs are tracked inside `m-rewrite/`.

## Execution Root (Required)
- Delegated sessions may start at workspace root (`bz3-rewrite/`), not repo root.
- Before following any read-order checklist that uses `AGENTS.md` / `docs/...` shorthand, anchor to repo root:

```bash
cd m-rewrite
```

- After that, treat all unprefixed doc paths as repo-relative (for example `AGENTS.md`, `docs/AGENTS.md`).
- If you cannot change directory, use explicit prefixed paths (for example `m-rewrite/AGENTS.md`).

## Core Architecture Contract
- `src/engine/` is game-agnostic and owns loop/lifecycle/timing.
- `src/game/` contains BZ3-specific game logic and host usage.
- Input, world ownership, render loop, and subsystem lifetime are engine-owned.
- Config is authoritative; required values must be read via strict helpers.
- Render layering is stable (`kLayerWorld = 0`, `kLayerUI = 1000`).
- UI policy: engine supports ImGui + RmlUi; game intentionally chooses one primary UI backend.

## Backend Exposure Contract
- Platform/render/physics/audio backends are engine-internal.
- Game code must consume engine contracts, not backend APIs.
- Backend-specific complexity stays in backend layers.
- Exception: game UI may directly target chosen UI backend (`imgui` or `rmlui`).

## Rewrite-Level Non-Goals
- Do not mirror `m-dev`/`KARMA-REPO` file layouts.
- Do not prioritize full gameplay feature parity before core infra contracts are stable.
- Do not move gameplay semantics into engine-core subsystems.

## Documentation System (Normalized)
Use docs by scope:

1. High-level rewrite contract:
- `AGENTS.md`

2. General execution model for delegated project work:
- `docs/AGENTS.md`

3. Project selection + canonical project specs:
- `docs/projects/README.md`
- `docs/projects/*.md` (one file per project)

Guidance precedence:
1. `AGENTS.md` (root stance/boundaries)
2. `docs/AGENTS.md` (shared execution rules)
3. Relevant `docs/projects/<project>.md` (project-specific scope/spec/validation)

## Agent Startup Order
1. Read this file.
2. Read `docs/AGENTS.md`.
3. Pick one project in `docs/projects/README.md`.
4. Read exactly one matching `docs/projects/<project>.md`.

## Global Validation Gates
From `m-rewrite/`:

```bash
./scripts/test-engine-backends.sh
./scripts/test-server-net.sh
```

Project-specific required tests are defined in each `docs/projects/<project>.md`.

## Practical Notes
- Runtime data root is `m-rewrite/data/`.
- If a local user config still points to old top-level `data/`, startup fails until overridden/updated.
- CI workflow `.github/workflows/core-test-suite.yml` enforces engine-backend and server-net wrappers on PR/push (`main`/`master`).
