# Cleanup S3 (`CLN-S3`): Config + Path Resolver Deduplication

## Project Snapshot
- Current owner: `codex`
- Status: `in progress (S3-5 decision executed; CLN-S3 implementation slices are closed behavior-neutrally)`
- Immediate next task: hand off CLN-S3 closeout state to cleanup parent sequencing and reallocate engine-infrastructure lane capacity.
- Validation gate: `cd m-karma && ./abuild.py -c -d <karma-build-dir>`.

## Mission
Remove duplicated config/path resolution logic and keep one authoritative implementation for canonicalization, merge behavior, and asset lookup flattening.

## Foundation References
- `projects/cleanup.md`
- `m-karma/src/common/config/store.cpp`
- `m-karma/src/common/data/path_resolver.cpp`

## Why This Is Separate
This is an engine utility-layer refactor that can proceed independently from server runtime, renderer core, and UI work.

## Owned Paths
- `m-karma/src/common/config/*`
- `m-karma/src/common/data/*`
- `m-overseer/agent/projects/cleanup/config-path-resolver-dedupe.md`

## Interface Boundaries
- Inputs consumed:
  - existing config/path call-site behavior expectations.
- Outputs exposed:
  - stable shared utility contract for canonicalization/merge resolution.
- Coordinate before changing:
  - `projects/cleanup/factory-stub-standardization.md`
  - `projects/cleanup/naming-directory-rationalization.md`

## Non-Goals
- Do not change public config key semantics.
- Do not broaden into unrelated content pipeline redesign.

## Validation
```bash
cd m-karma
./abuild.py -c -d <karma-build-dir>
./scripts/test-engine-backends.sh <karma-build-dir>
```

## Trace Channels
- `cleanup.s3`
- `config.store`
- `data.path`

## Build/Run Commands
```bash
cd m-karma
./abuild.py -c -d <karma-build-dir>
```

## Current Status
- `2026-02-21`: identified duplicate helper families and designated as `P1` lane.
- `2026-02-22`: moved under cleanup superproject child structure.
- `2026-02-22`: completed duplicate-helper inventory and extracted shared `common/data/path_utils` helpers for canonicalization, base-relative resolution, JSON merge, and asset entry flattening; migrated `config/store.cpp` and `data/path_resolver.cpp` to the shared surface; validated with `./abuild.py -c -d build-cln-s3` and `./scripts/test-engine-backends.sh build-cln-s3`.
- `2026-02-22`: captured `S3-2` placement decision: keep `path_utils` internal under `m-karma/src/common/data` (non-SDK/private header), with public behavior continuing through `karma/common/data/path_resolver.hpp` and `ConfigStore` interfaces.
- `2026-02-22`: completed `S3-3` by adding `src/common/tests/data_path_contract_test.cpp` and wiring `data_path_contract_test` in `cmake/sdk/tests.cmake`; validated with `./abuild.py -c -d build-cln-s3`, `ctest --test-dir build-cln-s3 -R "data_path_contract_test" --output-on-failure`, and `./scripts/test-engine-backends.sh build-cln-s3`.
- `2026-02-22`: completed `S3-4` by migrating canonicalization call sites in `src/audio/backends/{sdl3audio,miniaudio}.cpp`, `src/common/data/root_policy.cpp`, and `src/cli/server/runtime_options.cpp` to `path_utils::Canonicalize`; validated with `./abuild.py -c -d build-cln-s3`, `ctest --test-dir build-cln-s3 -R "data_path_contract_test" --output-on-failure`, and `./scripts/test-engine-backends.sh build-cln-s3`.
- `2026-02-22`: completed `S3-5` decision by migrating `src/common/data/directory_override.cpp` canonicalization to shared `path_utils::Canonicalize` (removing the last local weak/absolute canonicalization helper in CLN-S3 scope); validated with `./abuild.py -c -d build-cln-s3`, `ctest --test-dir build-cln-s3 -R "data_path_contract_test" --output-on-failure`, and `./scripts/test-engine-backends.sh build-cln-s3`.

## Decision (`S3-2`)
- Decision: keep `path_utils` in `common/data` as an internal utility surface for engine implementation files.
- Rationale: direct usage is internal (`src/common/config/store.cpp` + `src/common/data/path_resolver.cpp`), public consumers already rely on `karma/common/data/path_resolver.hpp`, and promoting a new SDK-visible utility module would widen API surface without a demonstrated external consumer need.
- Deferred follow-on: additional `TryCanonical`-style duplication in audio/root-policy/CLI paths is tracked for a separate dedupe wave after contract behavior is codified.

## Contract Test Scope (`S3-3`)
- Add a dedicated data/config contract test target that validates:
- `Canonicalize`: weakly-canonical success path and absolute-path fallback path.
- `MergeJsonObjects`: deep object merge behavior and non-object overwrite behavior.
- `CollectAssetEntries`: dotted key flattening and `baseDir`-relative resolution behavior.
- Keep tests at the `path_resolver`/`ConfigStore` contract seam so `path_utils` remains private implementation detail.

## Decision (`S3-5`)
- Decision: fold `src/common/data/directory_override.cpp` canonicalization into shared `path_utils::Canonicalize`.
- Rationale: the local helper duplicated the same weakly-canonical + absolute fallback semantics already codified and contract-tested under `path_utils`; consolidation removes duplicated behavior while preserving file-level caller contracts.
- Scope boundary: CLN-S3 now treats canonicalization dedupe as closed. Any future dedupe beyond this path requires new evidence of semantic drift or additional duplication outside current owned paths.

## Open Questions
- None. `S3-5` resolved the final canonicalization scope question by migrating `directory_override.cpp` to shared `path_utils`.

## Handoff Checklist
- [x] Duplicate helper inventory complete.
- [x] Shared utility extracted and call sites migrated.
- [x] Placement decision captured (`path_utils` internal, non-SDK).
- [x] Engine build/tests pass with no behavior regressions.
- [x] `S3-3` contract-test implementation slice landed and validated.
- [x] `S3-4` targeted canonicalization dedupe landed and validated.
- [x] `S3-5` decision executed: `directory_override.cpp` canonicalization now uses shared `path_utils::Canonicalize`.
