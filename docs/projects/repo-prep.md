# Repo Prep (KGDK + BZ3 Virtual Repos)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (planning; no code movement started)`
- Immediate next task: resolve pre-code decision gates in `RP0`, then execute the migration plan through virtual-split and branch/worktree isolation.
- Validation gate:
  - planning/doc slices: `./docs/scripts/lint-project-docs.sh`
  - code slices: `./abuild.py -c -d <build-dir>` after each pass.

## Mission
Prepare `m-rewrite` for eventual split into:
- `KARMA-GAME-DEVELOPMENT-KIT` (KGDK), and
- `BZ3-THE-GAME` (BZ3),

while still working inside one monorepo.

This track explicitly simulates split-repo development by introducing two virtual repo roots under `src/` and making top-level build orchestration invoke each virtual repo's local build wrapper.

This project now also includes the pre-repo-split branch/worktree preparation step:
- run KGDK and BZ3 as separate long-lived branches in separate sibling directories,
- keep `m-rewrite` as rewrite/orchestration-only,
- preserve `m-dev` and `KARMA-REPO` as external intake/reference inputs.

## Requested Scope (Locked)
1. Rename:
   - `src/engine/` -> `src/kgdk/`
   - `src/game/` -> `src/bz3/`
2. Create full virtual repo structure for both sides and move current source trees into nested `src/` folders (`src/kgdk/src/...`, `src/bz3/src/...`).
3. Modify top-level `abuild.py` to orchestrate two-stage build:
   - first build KGDK in `src/kgdk/`,
   - then build BZ3 in `src/bz3/` against freshly built KGDK headers/libs.
4. Resolve top-level overlap (primarily `docs/` and `data/`) so ownership and migration posture are explicit.
5. After virtual-split layout is stable, run branch/worktree preparation with separate sibling directories:
   - `m-kgdk/` (KGDK branch worktree),
   - `m-bz3/` (BZ3 branch worktree),
   - `m-rewrite/` (rewrite/orchestrator branch worktree),
   - `m-dev/` (read-only reference branch worktree for rewrite flow),
   - `KARMA-REPO/` (separate repo for capability intake).

## Workspace Topology (Locked Intent)
`bz3-rewrite/` is a filesystem parent directory, not a git branch/repo name.

Target sibling directories:
- `bz3-rewrite/m-kgdk/`
- `bz3-rewrite/m-bz3/`
- `bz3-rewrite/m-rewrite/`
- `bz3-rewrite/m-dev/`
- `bz3-rewrite/KARMA-REPO/`

Interpretation:
- `m-kgdk`, `m-bz3`, and `m-rewrite` are separate branch worktrees (parallel directories, no branch switching workflow).
- `m-dev` remains available for rewrite parity/reference pull decisions (primarily read-only posture for rewrite agents).
- `KARMA-REPO` remains a separate repo used for KGDK capability intake tracking.

## Target Virtual Layout

### `m-rewrite/src/kgdk/` (virtual KGDK repo)
- `CMakeLists.txt`
- `README.md`
- `abuild.py`
- `build-aN/` (engine-only builds; gitignored artifact dirs)
- `cmake/`
- `data/` (engine/demo-test data)
- `demo/`
- `docs/`
- `include/` (public API headers)
- `lib/` (release artifacts; gitignored)
- `scripts/`
- `src/` (current `m-rewrite/src/engine/*` content)
- `vcpkg/`

### `m-rewrite/src/bz3/` (virtual BZ3 repo)
- `CMakeLists.txt`
- `README.md`
- `abuild.py`
- `build-aN/` (game builds linking KGDK; gitignored artifact dirs)
- `data/` (game data)
- `docs/`
- `scripts/`
- `src/` (current `m-rewrite/src/game/*` content)
- `vcpkg/`

## Findings (Current Blast Radius)
As of `2026-02-18`:
- `src/engine|src/game` path references: `~645` matches across `~76` files.
- Docs-heavy footprint: `~340` matches across `~17` docs files.
- Immediate hard-break points:
  - top-level CMake subdirectory wiring (`add_subdirectory(src/engine)`, `add_subdirectory(src/game)`),
  - large hardcoded source lists in current module CMake files,
  - scripts that hardcode output paths (for example shadow-sandbox binaries),
  - governance/project docs that encode path ownership rules.

## Caveats (Already Known)
1. This should be a path/layout migration first, not an API/namespace/product rename migration.
   - Keep target names (`karma_engine_core`, `karma_engine_client`) and package naming stable in this track.
2. Existing build caches will be stale after path moves.
   - plan for reconfigure (`-c`) and potential cache cleanup.
3. Script runtime paths will change (for example `build-*/src/engine/...` to `build-*/src/kgdk/...`).
4. Historical docs that intentionally reference external trees (for example `../m-dev/src/engine/*`) must not be blindly rewritten.

## Interface Boundaries
- Inputs consumed:
  - current monorepo source trees under `src/engine/*` and `src/game/*`,
  - top-level build wrapper behavior in `abuild.py`,
  - current docs ownership/policy references.
- Outputs exposed:
  - virtual-repo directory topology under `src/kgdk` and `src/bz3`,
  - two-stage top-level build orchestration (`kgdk` then `bz3`),
  - explicit overlap policy for shared top-level assets/docs.
- Coordinate before changing:
  - `CMakeLists.txt`
  - top-level `abuild.py`
  - `src/kgdk/CMakeLists.txt` and `src/bz3/CMakeLists.txt` (after move)
  - scripts under `scripts/`
  - policy/governance docs under `docs/foundation/`

## Execution Plan

This track now executes in **4 passes** (`RP0` through `RP3`) for lower risk and clearer rollback points.

### RP0: Decision Gates (Pass 1, Required Before Code Movement)
Goal: lock preconditions so path moves do not create avoidable rework.

Actions:
1. Resolve and lock decisions in **Blockers / Uncertainties** below.
2. Finalize vcpkg posture and KGDK -> BZ3 handoff contract.
3. Freeze move map + grep allowlist (historical external references that must remain unchanged).

Acceptance:
- required gates are explicitly decided and recorded.
- migration map is approved before touching filesystem paths.

### RP1: Filesystem + CMake Migration (Pass 2)
Goal: rename/move directories and restore configure/build with minimal semantic change.

Actions:
1. Create virtual roots and move content:
   - `src/engine/*` -> `src/kgdk/src/*`
   - `src/game/*` -> `src/bz3/src/*`
2. Move/adjust CMake entry points:
   - add `src/kgdk/CMakeLists.txt` (engine-specific),
   - add `src/bz3/CMakeLists.txt` (game-specific),
   - top-level CMake calls `add_subdirectory(src/kgdk)` and `add_subdirectory(src/bz3)`.
3. Rewrite CMake path literals from old roots to new virtual roots.
4. Do not change namespaces, public include paths, or exported target names in this pass.

Acceptance:
- configure/build succeeds in at least one assigned build dir via top-level wrapper.
- core CMake graph uses new roots (`src/kgdk`, `src/bz3`) without fallback shims.

### RP2: Build-Orchestration + Artifact Handoff (Pass 3)
Goal: simulate split-repo developer workflow from monorepo root.

Actions:
1. Add local wrappers:
   - `src/kgdk/abuild.py` for KGDK-only build,
   - `src/bz3/abuild.py` for BZ3-only build consuming KGDK output.
2. Refactor top-level `abuild.py` to orchestrate:
   - phase A: invoke `src/kgdk/abuild.py`,
   - phase B: invoke `src/bz3/abuild.py` with KGDK prefix/include/lib locations.
3. Define and document stable handoff artifact contract from KGDK -> BZ3:
   - include root,
   - library root,
   - package config root (if `find_package` path is used).

Acceptance:
- top-level wrapper performs two-stage build end-to-end.
- BZ3 build consumes KGDK outputs from the same run.
- artifact handoff from KGDK -> BZ3 is deterministic and documented.

### RP3: Scripts + Docs + Overlap Closeout (Pass 4)
Goal: remove stale path usage and lock ownership posture for shared top-level assets.

Actions:
1. Rewrite scripts using `src/engine`/`src/game` output paths.
2. Update docs and policy/governance references to new roots.
3. Classify top-level overlap (`docs/`, `data/`, `demo/`, `scripts/`) as:
   - stays top-level for now,
   - mirrored under a virtual repo,
   - migrated to one virtual repo as source-of-truth.
4. Run final grep gates and confirm only allowlisted legacy references remain.

Acceptance:
- no stale `src/engine/` or `src/game/` references remain outside allowlisted historical/external contexts.
- overlap policy is written and consistent with filesystem + wrapper behavior.
- docs and scripts are aligned with the new virtual layout.

## Blockers / Uncertainties (Decide Before Code Changes)
1. **vcpkg strategy (required gate)**
   - decide between:
     - temporarily shared top-level vcpkg for both virtual repos, or
     - fully independent `src/kgdk/vcpkg` and `src/bz3/vcpkg` now.
   - note: revisit/lock this before any code movement.
2. **KGDK consumption contract for BZ3**
   - direct include/lib path handoff vs `find_package(KarmaEngine)` handoff.
3. **Install/staging contract location**
   - where KGDK writes build outputs BZ3 consumes (for example staging prefix under `src/kgdk/build-aN/...`).
4. **Lock ownership semantics across nested wrappers**
   - whether top-level lock delegates or each virtual wrapper owns independent lock files.
5. **Docs migration policy**
   - whether to rewrite all path references immediately or keep selected legacy references for historical context.
6. **Top-level shared assets**
   - explicit ownership and duplication policy for `data/`, `demo/`, `docs/`, `scripts/`.

## Non-Goals
- Do not split into separate git repos in this track.
- Do not rename C++ namespaces/public API identifiers in this track.
- Do not redesign backend/runtime behavior.
- Do not perform unrelated refactors while path migration churn is active.

## Validation
From `m-rewrite/`:

```bash
# docs/planning
./docs/scripts/lint-project-docs.sh

# code migration passes
./abuild.py -c -d <build-dir>
```

Recommended grep gates during migration:

```bash
rg -n "src/engine/|src/game/" CMakeLists.txt src scripts docs
rg -n "src/kgdk/|src/bz3/" CMakeLists.txt src scripts docs
```

## Current Status
- `2026-02-18`: initial repo-prep migration plan created.
- `2026-02-18`: blast-radius findings and known caveats recorded.
- `2026-02-18`: no code movement performed yet.

## Open Questions
- Should KGDK and BZ3 wrappers share backend-profile vocabulary exactly, or should BZ3 expose a reduced surface?
- Should top-level `abuild.py` remain developer entrypoint after split simulation, or become a thin delegator only?
- Should `src/webserver/` remain top-level during virtual split prep, or be explicitly attached to one virtual repo for ownership clarity?

## Handoff Checklist
- [x] Scope and requested outcomes captured
- [x] Findings/caveats captured
- [x] Four-pass migration plan defined
- [x] vcpkg revisit note captured as required gate
- [ ] RP0 decisions locked
- [ ] RP1 implemented
- [ ] RP2 implemented
- [ ] RP3 implemented
