# Repo Prep (KARMA + BZ3 Branch-First Split Prep)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (RP1 complete; RP2 manifest v2 approved; m-overseer branch bootstrap/prune complete; remaining execution scope is m-karma and m-bz3)`
- Immediate next task: run the pre-prune sync window (`m-rewrite` -> `m-karma` + `m-bz3`), then execute first non-destructive RP2 copy/prune batches in `m-karma` and `m-bz3`.
- Validation gate:
  - planning/doc slices: `./scripts/lint-config-projects.sh`
  - code slices: branch-local `./abuild.py -c -d <build-dir>` in target branches plus delegator smoke from `m-overseer`.

## Mission
Prepare rewrite-era code for eventual split into:
- `KARMA-GAME-DEVELOPMENT-KIT` (KARMA), and
- `BZ3-THE-GAME` (BZ3),

while keeping `m-rewrite` intact as the source baseline during migration.

This track now uses a branch-first, non-destructive strategy:
- create `m-overseer` (orchestrator/delegator), `m-karma`, and `m-bz3` first,
- copy relevant files into `m-karma`/`m-bz3` in bounded slices,
- avoid destructive moves/deletes in `m-rewrite` during this prep phase.

## Requested Scope (Locked)
1. Introduce `m-overseer/` as the dedicated orchestration/delegation branch/worktree.
2. Create `m-karma/` and `m-bz3/` branch/worktrees before any file-layout migration work.
3. Seed target branches by copying relevant files from `m-rewrite` (non-destructive), leaving `m-rewrite` engine/game trees intact.
4. Evolve build flow to branch-local ownership:
   - `m-karma`: KARMA build/output ownership,
   - `m-bz3`: BZ3 build ownership consuming KARMA outputs,
   - `m-overseer`: cross-branch delegator/orchestrator only.
5. Resolve shared-asset/docs ownership posture and lock KARMA public-header boundary for extraction readiness.

## Workspace Topology (Locked Intent)
`bz3-rewrite/` is a filesystem parent directory, not a git branch/repo name.

Target sibling directories:
- `bz3-rewrite/m-overseer/`
- `bz3-rewrite/m-karma/`
- `bz3-rewrite/m-bz3/`
- `bz3-rewrite/m-rewrite/`
- `bz3-rewrite/m-dev/`
- `bz3-rewrite/q-karma/`

Interpretation:
- `m-overseer`, `m-karma`, and `m-bz3` are separate long-lived branch worktrees (parallel directories, no branch switching workflow).
- `m-rewrite` remains intact as migration source baseline and reference branch during this prep track.
- `m-dev` remains parity/reference input.
- `q-karma` remains a separate capability-intake repo.

Implementation note (intent):
- use persistent worktrees for `m-overseer`, `m-karma`, and `m-bz3`,
- keep `bz3-rewrite/` as neutral container only,
- avoid branch-switch workflows in shared directories.

## Branch/Worktree Roles (Locked Intent)
### `m-overseer/`
- Purpose: orchestration/meta branch and delegator entrypoint.
- Contents: migration governance, orchestration docs, and branch-to-branch build delegation wrappers.
- Build role: delegates to `m-karma` and `m-bz3`; does not own component runtime code.

### `m-karma/`
- Purpose: KARMA codebase branch in near-final standalone-repo shape.
- Contents: engine/runtime SDK code and KARMA-side build/tooling.
- Build role: compile KARMA libraries and SDK artifacts.
- Future posture: ingest/selectively align relevant capabilities from `q-karma`.

### `m-bz3/`
- Purpose: BZ3 game codebase branch in near-final standalone-repo shape.
- Contents: game code and game-side build/tooling consuming KARMA outputs.
- Build role: compile game binaries linking KARMA headers/libs.

### `m-rewrite/`
- Purpose in this project: stable migration source baseline.
- Contents: current rewrite code/docs used as copy source during prep.
- Policy intent: keep intact during this track (no destructive engine/game path migration in place).

### `m-dev/`
- Purpose: legacy/reference worktree available during transition.
- Role in this project: parity/reference input only.

### `q-karma/`
- Purpose: separate repo used for ongoing capability intake into KARMA as approved.
- Role in this project: external intake source.

## Build Execution Policy After Cutover (Locked Intent)
1. Component builds occur in:
   - `m-karma/` for KARMA,
   - `m-bz3/` for BZ3.
2. `m-overseer/` acts as orchestrator/delegator that invokes branch-local wrappers.
3. `m-rewrite/` is not used as the primary split-execution compile root once cutover is accepted.
4. Build artifacts remain branch-local (`build-aN/`, `lib/`, staging prefixes).

## Source Preservation Policy (Required)
1. Keep `m-rewrite/src/engine/*` and `m-rewrite/src/game/*` intact for this prep track.
2. Use copy-based seeding into `m-karma` and `m-bz3`; do not `git mv`/delete those roots inside `m-rewrite`.
3. Any `m-rewrite` edits in this track are documentation/governance only unless explicitly approved otherwise.
4. Treat source-preservation drift as a blocker requiring overseer/human confirmation.

## Target Branch-Local Layout Direction
The migration target is branch-local ownership, not nested virtual repos inside `m-rewrite`.

### `m-karma/` (expected direction)
- `CMakeLists.txt`
- `README.md`
- `abuild.py`
- `build-aN/`
- `cmake/`
- `data/`
- `demo/`
- `docs/`
- `include/`
- `lib/`
- `scripts/`
- `src/` (engine/runtime ownership)
- `vcpkg/` (or approved shared alternative)

### `m-bz3/` (expected direction)
- `CMakeLists.txt`
- `README.md`
- `abuild.py`
- `build-aN/`
- `data/`
- `docs/`
- `scripts/`
- `src/` (game ownership)
- `vcpkg/` (or approved shared alternative)

### `m-overseer/` (expected direction)
- orchestration docs + policy
- delegator wrapper(s)
- no primary runtime component source ownership

## Findings (Current Blast Radius)
As of `2026-02-18`:
- `src/engine|src/game` path references: `~645` matches across `~76` files.
- docs-heavy footprint: `~340` matches across `~17` docs files.
- immediate hard-break points for branch extraction/copy:
  - top-level CMake subdirectory wiring,
  - hardcoded source lists in module CMake files,
  - scripts with fixed output paths (for example shadow-sandbox binaries),
  - governance docs encoding legacy ownership/path assumptions,
  - current install/export publishes all `include/karma/*` headers (too broad for long-term SDK boundary).

## KARMA Public Header Boundary (Required Gate)
This split must explicitly define public SDK API vs private/internal integration surface.

Current risk:
- `include/karma/*` is currently installed wholesale, exposing runtime/bootstrap internals.

### Boundary Decision (Required in RP0)
Define and lock:
1. Public header allowlist for KARMA.
2. Private/internal header list (not installed as SDK API).
3. Compatibility transition policy for headers moving from public to private.

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
- `include/karma/common/config/validation.hpp`

4. Network runtime/session/auth internals
- `include/karma/network/http/curl_global.hpp`
- `include/karma/network/client/auth/community_credentials.hpp`
- `include/karma/network/server/auth/preauth.hpp`
- `include/karma/network/server/session/hooks.hpp`
- `include/karma/network/server/session/join_runtime.hpp`
- `include/karma/network/server/session/leave_runtime.hpp`
- `include/karma/network/config/transport_mapping.hpp`

### First-Pass Public-Allowlist Direction
Likely public SDK-facing categories:
- ECS (`include/karma/ecs/*`)
- Physics API (`include/karma/physics/*`)
- Renderer API (`include/karma/renderer/*`)
- Window/events (`include/karma/window/*`)
- Input API (`include/karma/input/*`)
- UI API (`include/karma/ui/*`)
- Scene API (`include/karma/scene/*`)
- Renderer asset ingestion API (`include/karma/renderer/assets/*`)
- Core transport contracts (`include/karma/network/transport/*`)
- explicitly approved utility/config headers intended to be SDK-facing.

## Caveats (Already Known)
1. This is a branch topology + ownership migration first, not an API/namespace rename project.
2. Build caches are expected to be stale after branch-local seeding; reconfigure with `-c`.
3. Script/runtime output paths will differ between source (`m-rewrite`) and target branches.
4. Historical docs intentionally referencing external trees (for example `../m-dev/src/engine/*`) must not be blindly rewritten.

## Interface Boundaries
- Inputs consumed:
  - source baseline from `m-rewrite` trees,
  - existing build wrapper behavior,
  - current policy/governance references.
- Outputs exposed:
  - branch-first sibling topology (`m-overseer`, `m-karma`, `m-bz3`),
  - non-destructive seeded code ownership in target branches,
  - deterministic KARMA -> BZ3 artifact contract,
  - explicit shared-asset/docs ownership policy.
- Coordinate before changing:
  - top-level and branch-local `CMakeLists.txt`
  - top-level and branch-local `abuild.py`
  - scripts under `scripts/`
  - overseer docs under `docs/` and migration tracking under `docs/projects/`

## Execution Plan
This track executes in **5 passes** (`RP0` through `RP4`) to keep rollback points clear.

### RP0: Decision Gates (Required Before Code Changes)
Goal: lock preconditions so branch seeding does not create avoidable rework.

Actions:
1. Resolve and lock decisions in **Blockers / Uncertainties**.
2. Finalize vcpkg posture and KARMA -> BZ3 artifact contract direction.
3. Freeze source copy map + grep allowlist (historical/external references that must remain unchanged).
4. Freeze KARMA public-header boundary.

Acceptance:
- required decisions are recorded and approved,
- copy map and preservation policy are approved,
- header-boundary decision is approved before install/export adjustments.

### RP1: Branch/Worktree Bootstrap (Branch-First)
Goal: establish execution topology before any migration copies.

Actions:
1. Create/verify sibling worktrees:
   - `m-overseer/`, `m-karma/`, `m-bz3/`, plus existing `m-rewrite/`, `m-dev/`, `q-karma/`.
2. Ensure baseline branch points are explicit and documented.
3. Define ownership rules per branch (who edits what, where builds run).

Acceptance:
- branch/worktree topology is operational,
- `m-overseer` exists and is documented as delegator branch,
- no copy/move churn has been applied yet.

### RP2: Non-Destructive Seeding (Copy, Do Not Move)
Goal: seed `m-karma` and `m-bz3` with owned code while keeping `m-rewrite` intact.

Actions:
1. Copy engine/runtime-relevant paths from `m-rewrite` into `m-karma`.
2. Copy game-relevant paths from `m-rewrite` into `m-bz3`.
3. Record an explicit copy manifest (source path -> target branch path) for replay/audit.
4. Do not delete/move source trees in `m-rewrite`.

Acceptance:
- target branches contain seeded code for their ownership domains,
- copy manifest is committed,
- `m-rewrite` remains intact by policy.

### RP2 Directory Copy Manifest v2 (2026-02-19, Directory-Level Only)
Policy:
- this manifest is directory-scoped; no per-file cataloging for in-flux trees.
- `m-rewrite/src/engine/physics/` is copied wholesale to `m-karma/src/physics/`.
- because `m-overseer`, `m-karma`, and `m-bz3` were branched from `m-rewrite` head, this manifest acts as the authoritative ownership/copy contract for follow-up copy/prune passes.

#### Seed-Now Mappings (Unambiguous)
| Source (m-rewrite) | Target branch/worktree | Target path | Mode | Notes |
|---|---|---|---|---|
| `src/engine/app/` | `m-karma` | `src/app/` | `wholesale directory copy` | engine/runtime ownership |
| `src/engine/audio/` | `m-karma` | `src/audio/` | `wholesale directory copy` | engine/runtime ownership |
| `src/engine/cli/` | `m-karma` | `src/cli/` | `wholesale directory copy` | engine/runtime ownership |
| `src/engine/common/` | `m-karma` | `src/common/` | `wholesale directory copy` | engine/runtime ownership |
| `src/engine/input/` | `m-karma` | `src/input/` | `wholesale directory copy` | engine/runtime ownership |
| `src/engine/network/` | `m-karma` | `src/network/` | `wholesale directory copy` | engine/runtime ownership |
| `src/engine/physics/` | `m-karma` | `src/physics/` | `wholesale directory copy` | in-flux tree; copy as one unit (no per-file catalog) |
| `src/engine/renderer/` | `m-karma` | `src/renderer/` | `wholesale directory copy` | engine/runtime ownership |
| `src/engine/scene/` | `m-karma` | `src/scene/` | `wholesale directory copy` | engine/runtime ownership |
| `src/engine/ui/` | `m-karma` | `src/ui/` | `wholesale directory copy` | engine/runtime ownership |
| `src/engine/window/` | `m-karma` | `src/window/` | `wholesale directory copy` | engine/runtime ownership |
| `include/karma/` | `m-karma` | `include/karma/` | `wholesale directory copy` | seed as-is; public/private pruning follows KARMA header-boundary gate |
| `src/webserver/` | `m-karma` | `webserver/` | `wholesale directory copy` | general-purpose webserver owned by game devkit branch |
| `src/game/client/` | `m-bz3` | `src/client/` | `wholesale directory copy` | game ownership |
| `src/game/net/` | `m-bz3` | `src/net/` | `wholesale directory copy` | game ownership |
| `src/game/protos/` | `m-bz3` | `src/protos/` | `wholesale directory copy` | game ownership |
| `src/game/server/` | `m-bz3` | `src/server/` | `wholesale directory copy` | game ownership |
| `src/game/tests/` | `m-bz3` | `src/tests/` | `wholesale directory copy` | game ownership |
| `src/game/ui/` | `m-bz3` | `src/ui/` | `wholesale directory copy` | game ownership |
| `data/` | `m-karma` and `m-bz3` | `data/` | `dual wholesale directory copy` | keep a full copy in both branches (devkit testing + game builds) |
| `demo/` | `m-karma` and `m-bz3` | `demo/` | `dual wholesale directory copy` | keep a full copy in both branches (devkit testing + game builds) |
| `docs/` | `m-overseer` | `docs/` | `wholesale directory copy` | orchestration/governance baseline |
| `scripts/overseer-checkpoint.sh` | `m-overseer` | `(excluded)` | `do-not-copy` | m-overseer script footprint intentionally keeps setup stubs only |
| `scripts/setup.sh` | `m-karma` and `m-bz3` | `scripts/setup.sh` | `dual script copy` | branch-local setup scripts may diverge slightly by branch needs |
| `scripts/setup.bat` | `m-karma` and `m-bz3` | `scripts/setup.bat` | `dual script copy` | branch-local setup scripts may diverge slightly by branch needs |
| `scripts/setup.sh` | `m-overseer` | `scripts/setup.sh` | `stub script` | overseer stub only for now |
| `scripts/setup.bat` | `m-overseer` | `scripts/setup.bat` | `stub script` | overseer stub only for now |
| `scripts/test-engine-backends.sh` | `m-karma` | `scripts/test-engine-backends.sh` | `single-script copy` | engine/backend test gate ownership |
| `scripts/test-server-net.sh` | `m-karma` and `m-bz3` | `scripts/test-server-net.sh` | `dual script copy` | useful for both devkit testing and game-dev workflows |

#### Residual Deferred / Decision-Required Mappings
| Source (m-rewrite) | Proposed target | Status | Reason |
|---|---|---|---|
| `scripts/*` (other than setup/test wrappers/checkpoint) | `TBD by script purpose` | `defer` | remaining script ownership split is still pending |

### RP3: Branch-Local Build + Artifact Contract
Goal: make branch-local builds deterministic and connected.

Actions:
1. Stand up/adjust `m-karma` build wrapper and staged outputs.
2. Stand up/adjust `m-bz3` build wrapper consuming KARMA outputs.
3. Define and document stable KARMA -> BZ3 handoff contract:
   - include root,
   - library root,
   - package config root (if `find_package` is chosen).

Acceptance:
- `m-karma` and `m-bz3` each build in their own branches,
- BZ3 consumes KARMA outputs deterministically,
- artifact handoff contract is documented and validated.

### RP4: `m-overseer` Delegator + Docs/Policy Cutover
Goal: centralize orchestration in `m-overseer` and finalize operating posture.

Actions:
1. Implement/adjust `m-overseer` delegator flow to call sibling branch wrappers.
2. Update docs to reflect branch-first non-destructive model and branch roles.
3. Classify shared assets (`data/`, `demo/`, `docs/`, `scripts/`) with explicit source-of-truth policy.
4. Run grep/doc gates and confirm only allowlisted legacy references remain.

Acceptance:
- daily coordination entrypoint is `m-overseer`,
- component compile ownership is branch-local (`m-karma`, `m-bz3`),
- docs/policy consistently reflect non-destructive `m-rewrite` preservation.

## Blockers / Uncertainties (Decide Before Code Changes)
1. **vcpkg strategy (locked 2026-02-19)**
   - use independent branch-local trees: `m-karma/vcpkg` and `m-bz3/vcpkg`.
   - no shared top-level vcpkg for normal development/validation.
   - `m-overseer` does not own a vcpkg/build environment.
2. **KARMA consumption contract for BZ3**
   - direct include/lib handoff vs `find_package(KarmaEngine)`.
3. **Install/staging contract location**
   - where KARMA outputs are staged for BZ3 consumption.
4. **Lock ownership semantics across wrappers**
   - delegator-managed locks vs independent branch-local locks.
5. **Docs migration policy**
   - what stays in `m-rewrite` docs vs what moves/duplicates into `m-overseer`/component branches.
6. **Shared assets policy**
   - `data/` and `demo/` dual-copy policy is locked; residual ownership split for remaining `scripts/*` and docs distribution still needs explicit freeze.
7. **Branch/worktree creation procedure**
   - exact commands and ownership policy for `m-overseer`/`m-karma`/`m-bz3`.
8. **Operational compile cutover date**
   - when `m-overseer` becomes the canonical orchestration entrypoint.
9. **KARMA public-header boundary**
   - final allowlist/private list and compatibility rules.
10. **`m-rewrite` preservation exceptions**
   - explicit list of allowed non-destructive updates (if any) beyond docs/governance.

## Non-Goals
- Do not split into separate git repos in this track.
- Do not rename C++ namespaces/public API identifiers in this track.
- Do not redesign backend/runtime behavior in this track.
- Do not perform destructive engine/game path migration inside `m-rewrite` during prep.

## Validation
From the active branch roots:

```bash
# branch-local build validation (run in m-karma or m-bz3, not m-overseer)
./abuild.py -c -d <build-dir>

# optional source-preservation sanity during migration planning
git -C ../m-rewrite status --short

# m-overseer smoke
./scripts/setup.sh
```

Recommended grep gates:

```bash
rg -n "src/engine/|src/game/" CMakeLists.txt src scripts docs
rg -n "m-overseer|m-karma|m-bz3" docs scripts
```

## Current Status
- `2026-02-18`: project plan created and blast-radius findings recorded.
- `2026-02-18`: branch-first strategy locked (`m-overseer` + `m-karma` + `m-bz3` before migration copies).
- `2026-02-18`: non-destructive copy policy locked (keep `m-rewrite` intact during prep).
- `2026-02-19`: RP1 branch/worktree bootstrap completed (`m-overseer`, `m-karma`, `m-bz3` created as sibling worktrees).
- `2026-02-19`: RP2 directory-level copy manifest updated to v2 with human-approved mapping decisions (`src/webserver`, `data/`, `demo/`, `setup/test scripts`).
- `2026-02-19`: partial RP2 execution completed for `m-overseer` beta prep (`docs/` trimmed to overseer+repo-prep set, setup scripts replaced with explicit stubs, non-setup scripts removed).
- `2026-02-19`: `m-overseer` branch was pruned to orchestration-only footprint (no build/runtime source trees; docs reduced to overseer + repo-prep tracking set).
- `2026-02-19`: vcpkg posture locked to branch-local standalone trees (`m-karma/vcpkg`, `m-bz3/vcpkg`); shared top-level vcpkg is not used for normal development/validation.
- `2026-02-19`: `m-overseer` scope accepted as complete for this track; remaining execution work is in `m-karma` and `m-bz3`.

## Open Questions
- Should KARMA and BZ3 wrappers share backend-profile vocabulary exactly, or should BZ3 expose a reduced surface?
- Should `m-overseer` delegator remain a thin wrapper only, or also host release/checkpoint orchestration logic?

## Handoff Checklist
- [x] Scope and requested outcomes captured
- [x] Findings/caveats captured
- [x] Five-pass migration plan defined
- [x] Branch-first + non-destructive strategy captured
- [ ] RP0 decisions locked
- [x] RP1 implemented
- [ ] RP2 implemented
- [ ] RP3 implemented
- [ ] RP4 implemented
