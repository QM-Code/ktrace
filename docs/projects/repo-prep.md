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
- `KARMA-GAME-DEVELOPMENT-KIT` (KARMA), and
- `BZ3-THE-GAME` (BZ3),

while still working inside one monorepo.

This track explicitly simulates split-repo development by introducing two virtual repo roots under `src/` and making top-level build orchestration invoke each virtual repo's local build wrapper.

This project now also includes the pre-repo-split branch/worktree preparation step:
- run KARMA and BZ3 as separate long-lived branches in separate sibling directories,
- keep `m-rewrite` as rewrite/orchestration-only,
- preserve `m-dev` and `q-karma` as external intake/reference inputs.

## Requested Scope (Locked)
1. Rename:
   - `src/engine/` -> `src/karma/`
   - `src/game/` -> `src/bz3/`
2. Create full virtual repo structure for both sides and move current source trees into nested `src/` folders (`src/karma/src/...`, `src/bz3/src/...`).
3. Modify top-level `abuild.py` to orchestrate two-stage build:
   - first build KARMA in `src/karma/`,
   - then build BZ3 in `src/bz3/` against freshly built KARMA headers/libs.
4. Resolve top-level overlap (primarily `docs/` and `data/`) so ownership and migration posture are explicit.
5. After virtual-split layout is stable, run branch/worktree preparation with separate sibling directories:
   - `m-karma/` (KARMA branch worktree),
   - `m-bz3/` (BZ3 branch worktree),
   - `m-rewrite/` (rewrite/orchestrator branch worktree),
   - `m-dev/` (read-only reference branch worktree for rewrite flow),
   - `q-karma/` (separate repo for capability intake).

## Workspace Topology (Locked Intent)
`bz3-rewrite/` is a filesystem parent directory, not a git branch/repo name.

Target sibling directories:
- `bz3-rewrite/m-karma/`
- `bz3-rewrite/m-bz3/`
- `bz3-rewrite/m-rewrite/`
- `bz3-rewrite/m-dev/`
- `bz3-rewrite/q-karma/`

Interpretation:
- `m-karma`, `m-bz3`, and `m-rewrite` are separate branch worktrees (parallel directories, no branch switching workflow).
- `m-dev` remains available for rewrite parity/reference pull decisions (primarily read-only posture for rewrite agents).
- `q-karma` remains a separate repo used for m-karma capability intake tracking.

Implementation note (intent):
- use persistent branch worktrees for `m-karma`, `m-bz3`, and `m-rewrite` so each has an always-on directory.
- avoid normal development via branch switching in a shared directory.
- keep `bz3-rewrite/` as a neutral filesystem container only.

## Target Virtual Layout

### `m-rewrite/src/karma/` (virtual KARMA repo)
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
- `build-aN/` (game builds linking KARMA; gitignored artifact dirs)
- `data/` (game data)
- `docs/`
- `scripts/`
- `src/` (current `m-rewrite/src/game/*` content)
- `vcpkg/`

## Branch/Worktree Roles (Locked Intent)
### `m-karma/`
- Purpose: KARMA codebase branch in near-final standalone-repo shape.
- Contents: engine/runtime SDK code only (no rewrite governance content).
- Build role: compile KARMA libraries and SDK artifacts.
- Future posture: ingest/selectively align relevant capabilities from `q-karma`.

### `m-bz3/`
- Purpose: BZ3 game codebase branch in near-final standalone-repo shape.
- Contents: game code only, consuming KARMA outputs.
- Build role: compile game binaries linking KARMA libraries/headers.
- Knowledge boundary intent: may consult KARMA behavior/contract expectations; does not carry rewrite-overseer governance context.

### `m-rewrite/`
- Purpose: rewrite orchestration/meta branch.
- Contents: rewrite-specific docs/instructions + top-level orchestration wrappers.
- Build role after cutover: delegate only; no direct production compile ownership.
- Policy intent: rewrite-only governance artifacts should not be required in final KARMA/BZ3 repos.

### `m-dev/`
- Purpose: legacy/reference branch worktree available during transition.
- Role in this project: reference input for parity/migration decisions, not primary implementation target.

### `q-karma/`
- Purpose: separate repo used for ongoing capability intake into KARMA as approved.
- Role in this project: external intake source, not rewritten in this track.

## Build Execution Policy After Worktree Cutover (Locked Intent)
1. Actual component builds occur in:
   - `m-karma/` for KARMA,
   - `m-bz3/` for BZ3.
2. `m-rewrite/` should not be treated as a direct compile root after cutover.
3. `m-rewrite/abuild.py` acts as an orchestrator/delegator that invokes branch-local wrappers.
4. Build artifacts (`build-aN/`, `lib/`) remain branch-local and are not shared by ad-hoc path assumptions.

## `m-rewrite` End-State Intent (Pre-Final Split)
After RP4, `m-rewrite/` is intentionally lightweight and rewrite-specific:
- keep: rewrite orchestration docs + delegator tooling.
- remove/avoid as primary ownership: component source trees, component build directories, component-local vcpkg/cmake/data/demo trees.
- practical rule: if it is required to compile KARMA or BZ3 directly, it should live in `m-karma/` or `m-bz3/`, not in `m-rewrite/`.

## Agent-Scope Intent (Operational)
1. KARMA-focused agents operate from `m-karma/` and should not rely on rewrite-specific docs/process as primary guidance.
2. BZ3-focused agents operate from `m-bz3/`; KARMA is an external dependency surface, not a co-owned source tree.
3. Rewrite overseer agents operate from `m-rewrite/` with cross-worktree awareness and coordination authority.
4. `m-dev/` remains reference-oriented for rewrite/overseer flow unless explicitly reassigned.

## Findings (Current Blast Radius)
As of `2026-02-18`:
- `src/engine|src/game` path references: `~645` matches across `~76` files.
- Docs-heavy footprint: `~340` matches across `~17` docs files.
- Immediate hard-break points:
  - top-level CMake subdirectory wiring (`add_subdirectory(src/engine)`, `add_subdirectory(src/game)`),
  - large hardcoded source lists in current module CMake files,
  - scripts that hardcode output paths (for example shadow-sandbox binaries),
  - governance/project docs that encode path ownership rules.
  - current install/export behavior publishes all headers under `include/karma/*` (`CMakeLists.txt` install directory export), which is broader than intended long-term SDK surface.

## KARMA Public Header Boundary (New Gate)
This split must explicitly define what is public SDK API vs internal integration surface.

Current risk:
- `include/karma/*` is currently installed wholesale, so host/bootstrap/runtime-internal headers are externally visible by default.

### Boundary Decision (Required in RP0)
Define and lock:
1. Public header allowlist for KARMA.
2. Private/internal header list (not installed as SDK API).
3. Transition policy for currently-public headers that move to private.

### First-Pass Private/Internal Candidates
These should default to private unless explicitly promoted:

1. App bootstrap/runner/config glue
- `include/karma/app/shared/bootstrap.hpp`
- `include/karma/app/shared/backend_resolution.hpp`
- `include/karma/app/shared/simulation_clock.hpp`
- `include/karma/app/client/bootstrap.hpp`
- `include/karma/app/client/runner.hpp`
- `include/karma/app/server/bootstrap.hpp`
- `include/karma/app/server/runner.hpp`

2. CLI parsing/option internals
- `include/karma/cli/shared/parse.hpp`
- `include/karma/cli/client/*`
- `include/karma/cli/server/*`

3. Host-argv/config override helpers
- `include/karma/common/data/directory_override.hpp`
- `include/karma/common/config/validation.hpp` (specifically app/product required-key sets)

4. Network runtime/session/auth internals
- `include/karma/network/http/curl_global.hpp`
- `include/karma/network/client/auth/community_credentials.hpp`
- `include/karma/network/server/auth/preauth.hpp`
- `include/karma/network/server/session/hooks.hpp`
- `include/karma/network/server/session/join_runtime.hpp`
- `include/karma/network/server/session/leave_runtime.hpp`
- `include/karma/network/config/transport_mapping.hpp`

### First-Pass Public-Allowlist Direction
These are the likely public SDK-facing categories:
- ECS (`include/karma/ecs/*`)
- Physics runtime API (`include/karma/physics/*`)
- Renderer API (`include/karma/renderer/*`)
- Window system/events (`include/karma/window/*`)
- Input API (`include/karma/input/*`)
- UI API (`include/karma/ui/*`)
- Scene API (`include/karma/scene/*`)
- Renderer asset ingestion API (`include/karma/renderer/assets/*`)
- Core transport contracts (`include/karma/network/transport/*`)
- Content/data/config utilities that are intentionally SDK-facing (to be explicitly confirmed per header).

### Required Follow-up Before Final Split
1. Replace blanket include install/export with an explicit public-header allowlist install.
2. Move private headers to non-installed include roots or internal paths.
3. Document compatibility posture for any header removed from public SDK.
4. Gate repo extraction on boundary freeze and include/install verification.

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
  - sibling branch/worktree operating topology (`m-karma`, `m-bz3`, `m-rewrite`, `m-dev`, `q-karma`),
  - explicit overlap policy for shared top-level assets/docs.
- Coordinate before changing:
  - `CMakeLists.txt`
  - top-level `abuild.py`
  - `src/kgdk/CMakeLists.txt` and `src/bz3/CMakeLists.txt` (after move)
  - scripts under `scripts/`
  - policy/governance docs under `docs/foundation/`

## Execution Plan

This track now executes in **5 passes** (`RP0` through `RP4`) for lower risk and clearer rollback points.

### RP0: Decision Gates (Pass 1, Required Before Code Movement)
Goal: lock preconditions so path moves do not create avoidable rework.

Actions:
1. Resolve and lock decisions in **Blockers / Uncertainties** below.
2. Finalize vcpkg posture and KARMA -> BZ3 handoff contract.
3. Freeze move map + grep allowlist (historical external references that must remain unchanged).
4. Freeze KARMA public header boundary (allowlist/private list + transition policy).

Acceptance:
- required gates are explicitly decided and recorded.
- migration map is approved before touching filesystem paths.
- KARMA public header boundary is documented and approved before install/export adjustments.

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
   - `src/kgdk/abuild.py` for KARMA-only build,
   - `src/bz3/abuild.py` for BZ3-only build consuming KARMA output.
2. Refactor top-level `abuild.py` to orchestrate:
   - phase A: invoke `src/kgdk/abuild.py`,
   - phase B: invoke `src/bz3/abuild.py` with KARMA prefix/include/lib locations.
3. Define and document stable handoff artifact contract from KARMA -> BZ3:
   - include root,
   - library root,
   - package config root (if `find_package` path is used).

Acceptance:
- top-level wrapper performs two-stage build end-to-end.
- BZ3 build consumes KARMA outputs from the same run.
- artifact handoff from KARMA -> BZ3 is deterministic and documented.

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

### RP4: Branch/Worktree Isolation Cutover (Pass 5)
Goal: run the pre-split branch/worktree model in daily operation with explicit role boundaries.

Actions:
1. Establish/verify sibling worktree directories under parent `bz3-rewrite/`:
   - `m-karma/`, `m-bz3/`, `m-rewrite/`, `m-dev/`, and separate repo `q-karma/`.
2. Ensure no branch-switch workflow is required for normal development:
   - each active codebase has its own persistent directory.
3. Move compile responsibility to:
   - `m-karma/` for KARMA,
   - `m-bz3/` for BZ3.
4. Keep `m-rewrite/` as orchestration/governance:
   - no primary component compile ownership,
   - delegates builds into `m-karma/` and `m-bz3/`.
5. Lock reference posture:
   - `m-dev/` as migration/parity reference input,
   - `q-karma/` as capability-intake input for KARMA.

Acceptance:
- five-directory sibling topology is operational and documented.
- compile workflows run from `m-karma/` and `m-bz3/`, not directly from `m-rewrite/`.
- role boundaries for all five directories are explicit and enforced in docs/wrappers.

## Blockers / Uncertainties (Decide Before Code Changes)
1. **vcpkg strategy (required gate)**
   - decide between:
     - temporarily shared top-level vcpkg for both virtual repos, or
     - fully independent `src/kgdk/vcpkg` and `src/bz3/vcpkg` now.
   - note: revisit/lock this before any code movement.
2. **KARMA consumption contract for BZ3**
   - direct include/lib path handoff vs `find_package(KarmaEngine)` handoff.
3. **Install/staging contract location**
   - where KARMA writes build outputs BZ3 consumes (for example staging prefix under `src/kgdk/build-aN/...`).
4. **Lock ownership semantics across nested wrappers**
   - whether top-level lock delegates or each virtual wrapper owns independent lock files.
5. **Docs migration policy**
   - whether to rewrite all path references immediately or keep selected legacy references for historical context.
6. **Top-level shared assets**
   - explicit ownership and duplication policy for `data/`, `demo/`, `docs/`, `scripts/`.
7. **Branch/worktree creation strategy**
   - exact creation approach for `m-karma`/`m-bz3`/`m-rewrite` directories (worktree commands and ownership policy).
8. **Operational compile cutover date**
   - when `m-rewrite` stops being a compile root and becomes delegator-only.
9. **KARMA public header boundary**
   - exact allowlist/private list and compatibility policy for de-publicizing current headers.

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

# post-RP4 operating posture (examples)
# run KARMA build from m-karma/
# run BZ3 build from m-bz3/
# run orchestrated delegate flow from m-rewrite/ only as coordinator
```

Recommended grep gates during migration:

```bash
rg -n "src/engine/|src/game/" CMakeLists.txt src scripts docs
rg -n "src/kgdk/|src/bz3/" CMakeLists.txt src scripts docs
```

## Current Status
- `2026-02-18`: initial repo-prep migration plan created.
- `2026-02-18`: blast-radius findings and known caveats recorded.
- `2026-02-18`: sibling directory + branch/worktree intent locked in project doc (no branch-switch workflow intent).
- `2026-02-18`: no code movement performed yet.

## Open Questions
- Should KARMA and BZ3 wrappers share backend-profile vocabulary exactly, or should BZ3 expose a reduced surface?
- Should top-level `abuild.py` remain developer entrypoint after split simulation, or become a thin delegator only?
- Should `src/webserver/` remain top-level during virtual split prep, or be explicitly attached to one virtual repo for ownership clarity?

## Handoff Checklist
- [x] Scope and requested outcomes captured
- [x] Findings/caveats captured
- [x] Five-pass migration plan defined
- [x] vcpkg revisit note captured as required gate
- [ ] RP0 decisions locked
- [ ] RP1 implemented
- [ ] RP2 implemented
- [ ] RP3 implemented
- [ ] RP4 implemented
