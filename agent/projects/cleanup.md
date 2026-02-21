# Cross-Repo Cleanup Plan (`m-karma/src` + `m-bz3/src`)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (deep-dive analysis complete; execution not started)`
- Immediate next task: execute `CLN-S2` (server actor/session runtime cleanup) while deferring `CLN-S1` (`m-bz3/src/ui` integration) to late-stage project work.
- Validation gate:
  - `m-overseer`: `./scripts/lint-projects.sh`
  - `m-bz3`: `./abuild.py -c -d <bz3-build-dir>`, `./scripts/test-server-net.sh <bz3-build-dir>`, targeted `ctest` for touched contracts
  - `m-karma`: `./abuild.py -c -d <karma-build-dir>`, `./scripts/test-engine-backends.sh <karma-build-dir>`, targeted `ctest` for touched contracts

## Mission
Produce a comprehensive cleanup/refactor track across `m-karma/src` and `m-bz3/src` targeting:
- legacy/obsolete/unused code
- redundant code
- over-complex code
- unclear code and naming
- split/combine opportunities
- consistency and architecture standardization
- runtime and build-time efficiency improvements

The plan prioritizes highest-value targets first and assumes full refactor freedom.

## Foundation References
- `m-karma/src/physics/sync/ecs_sync_system.cpp:21`
- `m-karma/src/physics/tests/physics_backend_parity_test.cpp:481`
- `m-karma/src/renderer/backends/bgfx/core.cpp:109`
- `m-karma/src/renderer/backends/diligent/core.cpp:952`
- `m-karma/src/common/config/store.cpp:42`
- `m-karma/src/common/data/path_resolver.cpp:41`
- `m-karma/src/audio/backend_factory.cpp:13`
- `m-karma/src/physics/backend_factory.cpp:13`
- `m-karma/src/renderer/backend_factory.cpp:13`
- `m-karma/src/window/backend_factory.cpp:14`
- `m-karma/src/ui/backend_factory.cpp:13`
- `m-bz3/cmake/game/sources.cmake:1`
- `m-bz3/cmake/game/targets.cmake:5`
- `m-bz3/src/server/runtime/server_game.cpp:74`
- `m-bz3/src/server/domain/actor_system.cpp:113`
- `m-bz3/src/server/runtime/run.cpp:57`
- `m-bz3/src/client/game/lifecycle.cpp:153`
- `m-bz3/src/ui/frontends/imgui/console/panels/panel_community.cpp:20`
- `m-bz3/src/ui/frontends/rmlui/console/panels/panel_start_server.cpp:39`
- `m-bz3/src/ui/AGENTS.md:5`
- `m-bz3/src/ui/architecture.md:65`

## Why This Is Separate
This is an intentionally cross-cutting cleanup program, not a feature slice. It spans runtime code, build wiring, tests, documentation, naming conventions, and architecture boundaries across two repos.

## Baseline Inventory
- `m-karma/src`: `114` `.cpp`, `35` `.hpp`, ~`49,120` LOC (`.cpp/.hpp`).
- `m-bz3/src`: `146` `.cpp`, `101` `.hpp`, ~`39,651` LOC (`.cpp/.hpp`).
- Highest LOC subsystems:
  - `m-karma/src/renderer`: ~`15,518` LOC.
  - `m-karma/src/physics`: ~`14,333` LOC.
  - `m-bz3/src/ui`: ~`16,087` LOC.
  - `m-bz3/src/tests`: ~`11,032` LOC.
- Critical structural finding:
  - `m-bz3` has `48` `.cpp` files (~`13,106` LOC) not referenced by current `cmake/game/*` target wiring.
  - Unwired files are concentrated in `m-bz3/src/ui/*`.

## Comprehensive Findings

### 1) Legacy / Obsolete / Unused
- `m-bz3/src/ui/*` is effectively orphaned from active targets:
  - `m-bz3/cmake/game/sources.cmake:1` and `m-bz3/cmake/game/targets.cmake:5` wire client/server runtime without `src/ui`.
  - Non-UI runtime currently uses engine text panels in `m-bz3/src/client/game/lifecycle.cpp:153`.
- Stale pathing/docs indicate historical migration leftovers:
  - UI onboarding docs had stale `src/game/AGENTS.md` references even though `m-bz3/src/game` has no AGENTS doc.
  - `m-bz3/src/ui/AGENTS.md`, `m-bz3/src/ui/architecture.md`, and `m-bz3/src/ui/frontends/rmlui/console/panels/AGENTS.md` were normalized to `src/ui/...` paths in this pass.
- Legacy fallback semantics are explicitly embedded in names and tests:
  - `run_legacy_fallback_step` in `m-bz3/src/client/game/tank_motion_authority_state_machine.hpp:48`.

### 2) Redundant Code
- Five near-identical backend-factory implementations duplicate parsing/selection flow:
  - `m-karma/src/audio/backend_factory.cpp:13`
  - `m-karma/src/physics/backend_factory.cpp:13`
  - `m-karma/src/renderer/backend_factory.cpp:13`
  - `m-karma/src/window/backend_factory.cpp:14`
  - `m-karma/src/ui/backend_factory.cpp:13`
  - drift already exists (`renderer` uses `isValid()` gating, `ui` always includes software fallback, window macro naming differs from `KARMA_HAS_*` convention).
- Config/path helper duplication in two modules:
  - `TryCanonical`, `ResolveWithBase`, JSON merge, asset lookup logic duplicated between:
    - `m-karma/src/common/config/store.cpp:42`
    - `m-karma/src/common/data/path_resolver.cpp:41`
- UI frontend panel logic duplicates framework-independent behavior:
  - Same helper families (`trimCopy`, `formatExitStatus`, `guessLocalIpAddress`, `appendLog`) appear in both Start Server panels.
- Test harness helpers are repeatedly reimplemented across many test binaries (`Expect`, `FailTest`, `WaitUntil`, fixture setup patterns).

### 3) Overly Complicated
- Very large single translation units:
  - `m-karma/src/physics/tests/physics_backend_parity_test.cpp` (~`8,637` LOC).
  - `m-karma/src/physics/sync/ecs_sync_system.cpp` (~`1,438` LOC) with deep `preSimulate` decision tree.
  - `m-karma/src/renderer/backends/bgfx/core.cpp` (~`1,007` LOC).
  - `m-karma/src/renderer/backends/diligent/core.cpp` (~`956` LOC).
  - `m-bz3/src/server/runtime/run.cpp` (~`645` LOC) with large inline config materialization block.
  - `m-bz3/src/ui/frontends/*/panel_community.cpp` and `panel_start_server.cpp` are each large and duplicated by backend.

### 4) Unclear Code
- `ecs_sync_system` trace classifier surface is hard to reason about due many boolean-input classification helpers:
  - Dense enum/tag/classifier cluster from `m-karma/src/physics/sync/ecs_sync_system.cpp:21`.
- Behavior intent mismatch in naming:
  - `toSmallCaps` uppercases text in `m-bz3/src/ui/frontends/imgui/console/panels/panel_community.cpp:20`.
- Documentation/runtime mismatch in UI architecture is high:
  - docs describe a `src/game/ui` structure that does not match active runtime/build topology.

### 5) Poor Naming / Organization
- Directory naming drift:
  - `m-bz3` used to place build wiring under `src/game`; this was normalized in this pass to `cmake/game/*` and `game.hpp` moved under `src/client/game/`.
  - `m-karma` used to place SDK build wiring under `src/engine`; this was normalized in this pass to `cmake/sdk/*`.
- Generic file names reduce discoverability at scale:
  - repeated `core.cpp`, `internal.hpp`, `factory_internal.hpp`.
- Backend naming style is inconsistent (`sdl3audio` vs snake_case conventions used broadly elsewhere).

### 6) Files That Should Be Split
- `m-karma/src/physics/tests/physics_backend_parity_test.cpp`:
  - split into contract-focused files (lifecycle, runtime-command trace, ecs sync, engine sync).
- `m-karma/src/physics/sync/ecs_sync_system.cpp`:
  - split classification, reconcile policy, runtime command application, and observability.
- `m-karma/src/renderer/backends/bgfx/core.cpp` and `m-karma/src/renderer/backends/diligent/core.cpp`:
  - split init/resource-lifecycle, shadow/light preparation, submission pipeline.
- `m-bz3/src/server/runtime/run.cpp`:
  - split config loading/materialization from runtime assembly/launch.

### 7) Subfiles That Should Be Combined
- `m-bz3/src/client/game/tank_collision_*` has several tiny subfiles with high coupling and fragmented navigation:
  - candidates to merge into a tighter module boundary around collision query/runtime/stats structs.
- Tiny factory internal headers (`*/backends/factory_internal.hpp`) can be consolidated into a shared generic backend registry utility.
- Repeated stub implementations (especially physics/audio backend stubs) can be generated from one shared pattern layer.

### 8) Standardization Opportunities
- Standardize `backend_factory.cpp` behavior through one shared contract:
  - shared lowercase parse + alias policy (same canonical names and accepted spellings pattern).
  - `CompiledBackends()` order is the single source of truth for `Auto` fallback order.
  - `CreateBackend()` always sets `out_selected=Auto` first, only mutates on success, and never silently rewrites explicit preference.
  - optional post-create validator hook (`renderer::Backend::isValid()`) handled by the shared selector path.
- Keep per-subsystem factory files, but reduce them to backend spec tables plus thin wrappers over one shared selector utility.
- Normalize backend feature macro naming to one shape (`KARMA_HAS_<SUBSYSTEM>_<BACKEND>`), including window backend feature flags.
- Standardize runtime/config key loading via typed config structs rather than long imperative blocks.
- Standardize test entrypoint utilities with shared contract-test support helpers.
- Standardize naming conventions for backends and module folders.
- Standardize docs to current source layout and active build wiring.

### 9) More Intuitive Patterns
- Replace boolean-matrix classification APIs with typed decision objects/bitmask state in physics sync.
- Introduce explicit session<->actor indexing in server runtime instead of repeated scans.
- Move shared UI business logic into backend-neutral presenters/controllers and keep frontends thin adapters.
- Move large runtime configuration reads into declarative schema-style structs.

### 10) Efficiency Opportunities
- Runtime:
  - remove repeated `world->entities()` scans in `ServerGame::findActorForSession` (`m-bz3/src/server/runtime/server_game.cpp:74`) by maintaining actor index maps.
  - remove synthetic actor drift/health decay simulation from authoritative server actor tick path (`m-bz3/src/server/domain/actor_system.cpp:124`).
- Build-time:
  - move heavy inline rendering semantics from headers to `.cpp` where possible (`m-karma/src/renderer/backends/internal/*`), reducing compile fanout.
  - reduce duplicated template/utility code across modules.
- I/O/config path:
  - unify config/path resolver internals and avoid duplicated asset lookup/materialization logic.

## Priority Queue (Highest Value First)

### `CLN-S1` (P0): Resolve `m-bz3/src/ui` Orphaned Subsystem
- Scope:
  - decide `Integrate` or `Archive`.
  - if integrate: wire UI sources into build/runtime, replace temporary text-panel path in `m-bz3/src/client/game/lifecycle.cpp:153`.
  - if archive: remove dead tree from active dev path and keep only migration references.
- Why first:
  - removes largest ambiguity and dead-weight branch (`48` unwired `.cpp`, ~`13k` LOC).
- Current disposition:
  - deferred by operator direction; keep parked until late-stage integration.

### `CLN-S2` (P0): Server Actor/Session Runtime Cleanup
- Scope:
  - remove synthetic drift/health decay in `actor_system`.
  - add O(1) actor lookup indexed by session to eliminate repeated linear scans.
  - cleanly separate spawn initialization vs simulation update responsibilities.

### `CLN-S3` (P1): Deduplicate Config + Path Resolver Core
- Scope:
  - extract shared canonicalization/merge/asset-lookup helpers.
  - keep one authoritative implementation for JSON-layer merge + asset path flattening.
  - preserve external API behavior.

### `CLN-S4` (P1): Decompose Physics Sync Complexity
- Scope:
  - split `ecs_sync_system` into cohesive units.
  - replace many bool-argument classifiers with typed state.
  - isolate tracing/reporting from mutation logic.

### `CLN-S5` (P1): Split Monolithic Parity Test
- Scope:
  - split `physics_backend_parity_test.cpp` into grouped contract files sharing common fixture utilities.
  - keep backend matrix behavior and command-line backend selection semantics.

### `CLN-S6` (P1): Renderer Backend Core Decomposition
- Scope:
  - split `bgfx/core.cpp` and `diligent/core.cpp` into common architecture pieces + backend-specific bindings.
  - move large semantic helpers out of heavy headers where feasible.

### `CLN-S7` (P2): UI Frontend Redundancy Reduction
- Scope:
  - extract shared panel logic (community/start-server/settings/bindings).
  - keep framework-specific rendering/event glue only in frontend implementations.

### `CLN-S8` (P2): Factory + Stub Standardization (`backend_factory.cpp` normalization)
- Scope:
  - introduce shared backend selector utility (`parse/name/compiled/create` path) with:
    - per-backend spec entries: `kind`, canonical name, aliases, compiled flag, create thunk, optional validate thunk.
    - shared parse helper to remove repeated `Lower()` + manual `if` chains.
    - shared `CreateBackend` loop used by audio/physics/renderer/window/ui with subsystem-specific wrapper signatures.
  - preserve existing subsystem-specific behavior intentionally:
    - renderer keeps post-create validity check (`isValid()`).
    - ui keeps guaranteed software fallback in compiled list.
    - window keeps `CreateWindow` convenience wrapper.
  - generate or template repetitive stub behavior for unavailable backends.
  - add backend-factory contract tests that assert identical semantics across subsystems:
    - parse aliases/canonical names.
    - compiled ordering and `Auto` fallback selection.
    - explicit selection failure semantics when backend not compiled or invalid.
    - `out_selected` update contract.

### `CLN-S9` (P2): Test Harness Consolidation
- Scope:
  - create shared utilities for `Expect`/`Fail`/`WaitUntil`/fixture setup for transport/runtime integration suites.

### `CLN-S10` (P3): Naming + Directory Rationalization
- Scope:
  - align folder names with actual ownership and runtime usage.
  - replace ambiguous generic filenames where practical.
  - normalize backend naming style.

## Validation Strategy
```bash
# m-overseer doc hygiene
cd m-overseer
./scripts/lint-projects.sh

# m-bz3 (when touched)
cd ../m-bz3
export ABUILD_AGENT_NAME=specialist-cleanup
./abuild.py --claim-lock -d <bz3-build-dir>
./abuild.py -c -d <bz3-build-dir>
./scripts/test-server-net.sh <bz3-build-dir>
ctest --test-dir <bz3-build-dir> -R "server_net_contract_test|server_runtime_.*|transport_.*|client_runtime_cli_contract_test" --output-on-failure
./abuild.py --release-lock -d <bz3-build-dir>

# m-karma (when touched)
cd ../m-karma
export ABUILD_AGENT_NAME=specialist-cleanup
./abuild.py --claim-lock -d <karma-build-dir>
./abuild.py -c -d <karma-build-dir>
./scripts/test-engine-backends.sh <karma-build-dir>
ctest --test-dir <karma-build-dir> -R "physics_backend_parity_.*|.*transport_contract_test|directional_shadow_contract_test" --output-on-failure
./abuild.py --release-lock -d <karma-build-dir>
```

## Current Status
- `2026-02-21`: Cross-repo deep-dive analysis completed for `m-karma/src` and `m-bz3/src`.
- `2026-02-21`: Highest-value blocker identified: large unwired `m-bz3/src/ui` codepath.
- `2026-02-21`: Operator direction accepted: `m-bz3/src/ui` cleanup/integration deferred to late-stage work; active cleanup starts at `CLN-S2`.
- `2026-02-21`: Prioritized cleanup queue (`CLN-S1`..`CLN-S10`) defined.
- `2026-02-21`: Normalized stale `src/game/AGENTS.md` references in `m-bz3/src/ui` docs to current `src/ui/...` paths.
- `2026-02-21`: Captured concrete `backend_factory.cpp` normalization plan and behavior contract under `CLN-S8`.
- `2026-02-21`: Removed source-tree CMake ownership anti-patterns:
  - `m-bz3/src/game/CMakeLists.txt` removed; build wiring moved to `m-bz3/cmake/game/*`; `m-bz3/src/game/game.hpp` moved to `m-bz3/src/client/game/game.hpp`.
  - `m-karma/src/engine/CMakeLists.txt` removed; SDK build wiring moved to `m-karma/cmake/sdk/*`; `m-karma/cmake/40_sdk_subdir.cmake` now includes `cmake/sdk/*` directly.

## Open Questions
- Do we want to preserve manual/standalone test binaries, or adopt a shared lightweight test harness layer first?
- For naming rationalization, should we enforce one repository-wide convention immediately or phase it per subsystem?
- For backend naming strings, do we keep legacy CLI tokens exactly (e.g. `sdl3audio`) or add canonical+alias pairs (`sdl3-audio`) while preserving compatibility?

## Handoff Checklist
- [x] Deep-dive findings captured with concrete file references.
- [x] Priority-ordered cleanup backlog defined.
- [x] Validation strategy documented.
- [ ] `CLN-S1` deferred/parked for late-stage integration.
- [ ] First implementation slice completed and validated.
- [ ] Project archived after cleanup backlog closure.
