# Cleanup Superproject (`m-karma/src` + `m-bz3/src`)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (converted to parent superproject; child tracks staged)`
- Immediate next task: dispatch one implementation slice from `cleanup/server-actor-session-runtime.md` while queuing parallel prep slices in `cleanup/physics-parity-suite.md` and `cleanup/factory-stub-standardization.md`.
- Validation gate: `cd m-overseer && ./agent/scripts/lint-projects.sh`.

## Mission
Coordinate cross-repo cleanup/refactor work across `m-karma/src` and `m-bz3/src` through independent, parallelizable subprojects with clear ownership boundaries and shared acceptance criteria.

## Foundation References
- `projects/cleanup/ui-orphaned-subsystem.md`
- `projects/cleanup/server-actor-session-runtime.md`
- `projects/cleanup/config-path-resolver-dedupe.md`
- `projects/cleanup/physics-sync-decomposition.md`
- `projects/cleanup/physics-parity-suite.md`
- `projects/cleanup/renderer-backend-core-decomposition.md`
- `projects/cleanup/ui-frontend-redundancy.md`
- `projects/cleanup/factory-stub-standardization.md`
- `projects/cleanup/test-harness-consolidation.md`
- `projects/cleanup/naming-directory-rationalization.md`
- `../docs/building.md`
- `../docs/testing.md`

## Why This Is Separate
Cleanup is cross-cutting and spans runtime behavior, build wiring, naming, tests, and architecture boundaries in multiple repos. A parent orchestration track is required to keep high-value work parallel without drifting contracts.

## Subproject Map (`CLN-S1..CLN-S10`)
- `cleanup/ui-orphaned-subsystem.md` (`CLN-S1`): deferred UI subtree integration/archive decision.
- `cleanup/server-actor-session-runtime.md` (`CLN-S2`): actor/session runtime cleanup and indexing.
- `cleanup/config-path-resolver-dedupe.md` (`CLN-S3`): config/path duplication removal.
- `cleanup/physics-sync-decomposition.md` (`CLN-S4`): `ecs_sync_system` decomposition and typed decision flow.
- `cleanup/physics-parity-suite.md` (`CLN-S5`): parity test split completion (current split scaffold follow-through).
- `cleanup/renderer-backend-core-decomposition.md` (`CLN-S6`): BGFX/Diligent core decomposition.
- `cleanup/ui-frontend-redundancy.md` (`CLN-S7`): cross-frontend UI logic extraction.
- `cleanup/factory-stub-standardization.md` (`CLN-S8`): backend factory/selector standardization.
- `cleanup/test-harness-consolidation.md` (`CLN-S9`): shared test harness utilities.
- `cleanup/naming-directory-rationalization.md` (`CLN-S10`): naming/layout normalization.

## Parallelization Lanes
1. `Server Runtime Lane`: `cleanup/server-actor-session-runtime.md`
2. `Physics Lane`: `cleanup/physics-sync-decomposition.md` + `cleanup/physics-parity-suite.md`
3. `Engine Infrastructure Lane`: `cleanup/config-path-resolver-dedupe.md` + `cleanup/factory-stub-standardization.md` + `cleanup/test-harness-consolidation.md`
4. `Renderer Lane`: `cleanup/renderer-backend-core-decomposition.md`
5. `Naming/Structure Lane`: `cleanup/naming-directory-rationalization.md`

## Interface Boundaries
- Inputs consumed:
  - child-track status, blockers, and acceptance evidence.
  - cross-repo build/test constraints from `../docs/building.md` and `../docs/testing.md`.
- Outputs exposed:
  - priority and sequencing lock across child tracks.
  - shared acceptance gates for merge readiness.
- Coordinate before changing:
  - `projects/ASSIGNMENTS.md`
  - all `projects/cleanup/*.md` files

## Milestones
### C0: Superproject Conversion (this slice)
- convert monolithic cleanup plan into parent + child docs.
- acceptance:
  - all `CLN-S1..CLN-S10` tracked as individual child docs.

### C1: Highest-Value Execution
- execute `CLN-S2`, then `CLN-S4`/`CLN-S5` or `CLN-S8` in parallel lanes.
- acceptance:
  - at least two independent lanes show validated progress.

### C2: Cross-Lane Standardization
- align factory behavior contracts, test harness strategy, and naming policy.
- acceptance:
  - no cross-lane blocker caused by naming/API drift.

### C3: Closeout
- remaining deferred/late-stage tracks (`CLN-S1`, `CLN-S7`) integrated or archived by explicit decision.
- acceptance:
  - child docs resolved and parent archived.

## Non-Goals
- Do not place implementation detail for every child track in this parent doc.
- Do not allow child tracks to silently redefine shared contracts.
- Do not merge partially validated cleanup slices that regress parity/test gates.

## Validation
```bash
cd m-overseer
./agent/scripts/lint-projects.sh
```

## Trace Channels
- `cleanup.plan`
- `cleanup.server`
- `cleanup.physics`
- `cleanup.renderer`
- `cleanup.factory`
- `cleanup.tests`

## Current Status
- `2026-02-21`: deep-dive analysis completed and `CLN-S1..CLN-S10` backlog defined.
- `2026-02-21`: source-tree CMake ownership anti-pattern removed (`src/game` and `src/engine` build wiring relocation).
- `2026-02-22`: cleanup program converted into parent/child superproject architecture.
- `2026-02-22`: `CLN-S3` advanced from extraction to placement decision; `path_utils` remains internal and `S3-3` contract-test slice is next.
- `2026-02-22`: `CLN-S3` `S3-3` contract tests landed (`data_path_contract_test`) and validated; next `CLN-S3` action is `S3-4` canonicalization dedupe follow-on.
- `2026-02-22`: `CLN-S3` `S3-4` targeted canonicalization dedupe landed (`audio/*`, `root_policy`, `cli/server/runtime_options`) and validated; next `CLN-S3` action is `S3-5` closeout decision for `directory_override`.
- `2026-02-22`: `CLN-S3` `S3-5` decision landed by migrating `src/common/data/directory_override.cpp` canonicalization to shared `path_utils::Canonicalize`; validated via `./abuild.py -c -d build-cln-s3`, `ctest --test-dir build-cln-s3 -R "data_path_contract_test" --output-on-failure`, and `./scripts/test-engine-backends.sh build-cln-s3`. CLN-S3 implementation slices are now closed.

## Open Questions
- Which two lanes should be executed concurrently after `CLN-S2` to maximize throughput without merge contention?
- Should `CLN-S7` remain fully deferred behind UI integration, or can backend-neutral presenter extraction begin early?

## Handoff Checklist
- [ ] Child docs stay aligned with parent sequencing.
- [ ] Assignment board tracks every active cleanup child project.
- [ ] Cross-lane blockers are recorded and routed.
- [ ] Parent status reflects actual child execution state.
