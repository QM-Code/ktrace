# Cleanup Superproject (`m-karma/src` + `m-bz3/src`)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (converted to parent superproject; active non-UI child tracks staged)`
- Immediate next task: keep `cleanup/server-actor-session-runtime.md` as highest-priority active lane while sequencing remaining cleanup child tracks.
- Validation gate: `cd m-overseer && ./agent/scripts/lint-projects.sh`.

## Mission
Coordinate cross-repo cleanup/refactor work across `m-karma/src` and `m-bz3/src` through independent, parallelizable subprojects with clear ownership boundaries and shared acceptance criteria.

## Foundation References
- `projects/cleanup/server-actor-session-runtime.md`
- `projects/ARCHIVE/cleanup-config-path-resolver-dedupe-retired-2026-02-22.md`
- `projects/cleanup/renderer-backend-core-decomposition.md`
- `projects/ARCHIVE/cleanup-test-harness-consolidation-retired-2026-02-23.md`
- `projects/ARCHIVE/cleanup-naming-directory-rationalization-retired-2026-02-23.md`
- `../docs/building.md`
- `../docs/testing.md`

## Why This Is Separate
Cleanup is cross-cutting and spans runtime behavior, build wiring, naming, tests, and architecture boundaries in multiple repos. A parent orchestration track is required to keep high-value work parallel without drifting contracts.

## Active Subproject Map
- `cleanup/server-actor-session-runtime.md` (`CLN-S2`): actor/session runtime cleanup and indexing.
- `cleanup/renderer-backend-core-decomposition.md` (`CLN-S6`): BGFX/Diligent core decomposition.

## Parallelization Lanes
1. `Server Runtime Lane`: `cleanup/server-actor-session-runtime.md`
2. `Renderer Lane`: `cleanup/renderer-backend-core-decomposition.md`

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
  - initial cleanup lanes split into independent child docs with assignment tracking.

### C1: Highest-Value Execution
- execute `CLN-S2`, then `CLN-S9` in parallel lanes.
- acceptance:
  - at least two independent lanes show validated progress.

### C2: Cross-Lane Standardization
- align factory behavior contracts, test harness strategy, and naming policy.
- acceptance:
  - no cross-lane blocker caused by naming/API drift.

### C3: Closeout
- remaining active lanes integrated or archived by explicit decision.
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
- `2026-02-22`: `CLN-S1` and `CLN-S7` UI-focused child docs removed from cleanup scope by operator direction; active UI work continues under `projects/ui.md` lanes.
- `2026-02-22`: `CLN-S3` advanced from extraction to placement decision; `path_utils` remains internal and `S3-3` contract-test slice is next.
- `2026-02-22`: `CLN-S3` `S3-3` contract tests landed (`data_path_contract_test`) and validated; next `CLN-S3` action is `S3-4` canonicalization dedupe follow-on.
- `2026-02-22`: `CLN-S3` `S3-4` targeted canonicalization dedupe landed (`audio/*`, `root_policy`, `cli/server/runtime_options`) and validated; next `CLN-S3` action is `S3-5` closeout decision for `directory_override`.
- `2026-02-22`: `CLN-S3` `S3-5` decision landed by migrating `src/common/data/directory_override.cpp` canonicalization to shared `path_utils::Canonicalize`; validated via `./abuild.py -c -d build-cln-s3`, `ctest --test-dir build-cln-s3 -R "data_path_contract_test" --output-on-failure`, and `./scripts/test-engine-backends.sh build-cln-s3`. CLN-S3 implementation slices are now closed.
- `2026-02-22`: `CLN-S3` lane marked complete in project tracking (`ASSIGNMENTS.md`) before archival handoff sequencing.
- `2026-02-22`: `CLN-S3` closeout completed; subproject archived as `projects/ARCHIVE/cleanup-config-path-resolver-dedupe-retired-2026-02-22.md`.
- `2026-02-22`: `CLN-S8` `S8-1` landed by introducing shared selector utility `src/common/backend/selector.hpp` and migrating `src/audio/backend_factory.cpp` as reference usage; validated via `./abuild.py -c -d build-cln-s8`, `./scripts/test-engine-backends.sh build-cln-s8`, and `ctest --test-dir build-cln-s8 -R "physics_backend_parity_.*|audio_backend_smoke_.*" --output-on-failure`.
- `2026-02-22`: `CLN-S8` `S8-2` landed by migrating `src/physics/backend_factory.cpp` to shared selector helpers in `src/common/backend/selector.hpp`; validated via `./abuild.py -c -d build-cln-s8`, `./scripts/test-engine-backends.sh build-cln-s8`, and `ctest --test-dir build-cln-s8 -R "physics_backend_parity_.*|audio_backend_smoke_.*" --output-on-failure`.
- `2026-02-22`: `CLN-S8` `S8-3` landed by migrating `src/window/backend_factory.cpp` to shared selector helpers in `src/common/backend/selector.hpp`; validated via `./abuild.py -c -d build-cln-s8`, `./scripts/test-engine-backends.sh build-cln-s8`, and `ctest --test-dir build-cln-s8 -R "physics_backend_parity_.*|audio_backend_smoke_.*" --output-on-failure`.
- `2026-02-22`: `CLN-S8` `S8-4` landed by migrating `src/ui/backend_factory.cpp` to shared selector helpers in `src/common/backend/selector.hpp` while preserving `software-overlay` alias parsing; validated via `./abuild.py -c -d build-cln-s8`, `./scripts/test-engine-backends.sh build-cln-s8`, and `ctest --test-dir build-cln-s8 -R "physics_backend_parity_.*|audio_backend_smoke_.*" --output-on-failure`.
- `2026-02-22`: `CLN-S8` `S8-5` landed by migrating `src/renderer/backend_factory.cpp` to shared selector helpers in `src/common/backend/selector.hpp` while preserving `backend->isValid()` acceptance semantics; validated via `./abuild.py -c -d build-cln-s8`, `./scripts/test-engine-backends.sh build-cln-s8`, and `ctest --test-dir build-cln-s8 -R "physics_backend_parity_.*|audio_backend_smoke_.*" --output-on-failure`.
- `2026-02-22`: `CLN-S8` `S8-6` landed by adding `backend_selector_contract_test` (`src/common/tests/backend_selector_contract_test.cpp`) and wiring `cmake/sdk/tests.cmake` test registration; validated via `./abuild.py -c -d build-cln-s8`, `./scripts/test-engine-backends.sh build-cln-s8`, and `ctest --test-dir build-cln-s8 -R "backend_selector_contract_test|physics_backend_parity_.*|audio_backend_smoke_.*" --output-on-failure`. CLN-S8 rollout is now closed behavior-neutrally.
- `2026-02-22`: `CLN-S8` closeout completed; subproject archived as `projects/ARCHIVE/cleanup-factory-stub-standardization-retired-2026-02-22.md` and removed from active assignment tracking.
- `2026-02-22`: `CLN-S4` closeout completed; subproject archived as `projects/ARCHIVE/cleanup-physics-sync-decomposition-retired-2026-02-22.md`.
- `2026-02-23`: `CLN-S9` advanced from queued prep to active execution: `m-bz3` shared test harness modules landed (`src/tests/support/test_harness.hpp`, `src/tests/support/loopback_harness.hpp`), broad test migration validated (`ctest` `30/30`), and next step is mirrored `m-karma` parity/gating follow-on.
- `2026-02-23`: `CLN-S9` parity follow-on landed in `m-karma` (`src/tests/support/test_harness.hpp` + targeted suite migrations), validated with targeted contract tests (`5/5`), leaving only closeout decisions (client transport diagnostics helper exception + m-bz3 loopback target-gating policy).
- `2026-02-23`: `CLN-S9` closeout decisions documented (loopback gating rationale in `m-bz3/cmake/targets/tests.cmake`; client transport diagnostics-helper exception in `m-karma/src/network/tests/contracts/client_transport_contract_test.cpp`); lane is now closeout-ready pending archival call.
- `2026-02-23`: `CLN-S9` closeout completed by operator decision not to export `network_test_support` through Karma SDK targets; subproject archived as `projects/ARCHIVE/cleanup-test-harness-consolidation-retired-2026-02-23.md`.
- `2026-02-23`: `CLN-S10` closeout completed by operator direction; subproject archived as `projects/ARCHIVE/cleanup-naming-directory-rationalization-retired-2026-02-23.md` and removed from active assignment tracking.

## Open Questions
- Should `CLN-S6` be activated in parallel with `CLN-S2` now that `CLN-S10` is closed, or remain queued until `CLN-S2` clears?

## Handoff Checklist
- [ ] Child docs stay aligned with parent sequencing.
- [ ] Assignment board tracks every active cleanup child project.
- [ ] Cross-lane blockers are recorded and routed.
- [ ] Parent status reflects actual child execution state.
