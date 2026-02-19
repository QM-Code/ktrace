# Repo Cleanup (Post-Split Stabilization)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress`
- Immediate next task: document the finalized KARMA -> BZ3 SDK handoff roots and finish branch-specific script/check cleanup.
- Validation gate:
  - project tracking docs: `./scripts/lint-config-projects.sh`
  - repo build checks:
    - `m-karma`: build SDK output in `build-sdk/`
    - `m-bz3`: build against the `m-karma/build-sdk` output

## Mission
Finish the remaining practical split cleanup so `m-karma` and `m-bz3` behave like standalone repos with a clear integration contract.

## Locked Decisions (2026-02-19)
1. BZ3 consumes KARMA SDK via `find_package(KarmaEngine)` (not direct raw include/lib wiring).
2. Near-term SDK profile for BZ3 is non-Diligent (default bgfx-capable export path).
3. Diligent-enabled KARMA SDK exports remain core-only until Diligent can be package-resolved/exported relocatably in the SDK path.

## Progress Update (2026-02-19)
1. `m-karma` SDK config now loads conditional exported dependencies for Jolt and ENet.
2. The prior `Jolt::Jolt` import failure in `m-bz3` is reported as progressed past (build moved forward).
3. Added missing `find_dependency(OpenSSL REQUIRED)` in KARMA SDK config to match exported `OpenSSL::Crypto` linkage.
4. `m-bz3/build-sdk` is now reported clean against the refreshed `m-karma` SDK export.

## Remaining Work (Required)
1. Lock the KARMA -> BZ3 SDK handoff contract.
   - Implement the locked consumption path (`find_package(KarmaEngine)`) consistently in wrappers/docs.
   - Record exact handoff roots:
     - headers root,
     - libs root,
     - package config root.
2. Clean branch-specific scripts/checks.
   - Keep engine-only scripts/checks in `m-karma`.
   - Keep game-only scripts/checks in `m-bz3`.
   - Remove stale `src/engine/*` / `src/game/*` assumptions where no longer valid.
3. Update tracking docs to final state.
   - Keep this file as the active cleanup tracker.
   - Mark items complete as they land.

## Build Error Intake (Live)
Add new failures here as they are discovered during build bring-up.

| Date | Repo | Command | Failure Summary | Status | Notes |
|---|---|---|---|---|---|
| `2026-02-19` | `m-bz3` | `./abuild.py -c -d build-sdk --karma-sdk ../m-karma/out/karma-sdk/ --ignore-lock` | CMake configure failed because imported SDK linked `Jolt::Jolt` but SDK config did not load that dependency. | `resolved (progressed)` | `m-karma` SDK config updated to load Jolt/ENet dependencies; specialist reported build now progresses beyond this point. |
| `2026-02-19` | `m-karma` | SDK config generation | Exported SDK links `OpenSSL::Crypto` but package config did not explicitly load OpenSSL. | `resolved` | Added `find_dependency(OpenSSL REQUIRED)` in `cmake/KarmaEngineConfig.cmake.in`; re-export SDK before next `m-bz3` retry. |
| `2026-02-19` | `m-bz3` | `build-sdk` flow | SDK integration build is now reported clean. | `resolved` | Move focus to contract documentation + script/check ownership cleanup. |

## Follow-Up (Optional, Not Blocking Current Bring-Up)
1. Narrow KARMA installed public headers from broad `include/karma/*` to an explicit SDK allowlist.

## Handoff Checklist
- [x] `m-bz3/build-sdk` passes against `m-karma/build-sdk`
- [ ] KARMA -> BZ3 SDK contract is locked and documented
- [ ] branch-specific script/check ownership is cleaned up
- [x] all active build errors are logged and resolved or intentionally deferred
