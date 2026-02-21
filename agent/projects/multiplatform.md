# Multiplatform Build + SDK Packaging

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (Linux runtime-link regression fixed locally; cross-platform packaging contract not yet standardized)`
- Immediate next task: publish `MP0` linkage policy contract and dispatch a bounded `MP1` hardening slice for full Linux runtime dependency staging.
- Validation gate:
  - `m-overseer`: `./scripts/lint-projects.sh`
  - `m-karma`: `./abuild.py -c -d build-sdk -b bgfx,diligent --install-sdk out/karma-sdk`
  - `m-bz3`: `./build-sdk/bz3 -h` and `./build-a7/bz3 -h`

## Mission
Define and implement one maintainable, explicit build+packaging contract for `KarmaEngine` SDK outputs across Linux/macOS/Windows/iOS/Android so consumers can build and run without ad-hoc runtime dependency fixes.

## Foundation References
- `m-karma/CMakeLists.txt`
- `m-karma/src/engine/CMakeLists.txt`
- `m-karma/cmake/KarmaEngineConfig.cmake.in`
- `m-bz3/CMakeLists.txt`
- `m-overseer/agent/docs/building.md`
- `m-overseer/agent/projects/ARCHIVE/cmake.md`

## Why This Is Separate
- This is cross-repo and cross-platform infrastructure work, not a gameplay/render feature slice.
- Runtime packaging defects here block multiple downstream tracks (`m-bz3`, future SDK consumers).
- Policy decisions (shared vs static, dependency staging, CI gates) must be durable and centralized.

## Build Policy Objective (Decision Target)
Use a hybrid linkage policy with one exported target contract:
- Keep `karma::engine_client` and `karma::engine_core` stable for consumers.
- Desktop SDKs (`Linux`, `macOS`, `Windows`) default to `SHARED` `engine_client` with runtime dependency staging.
- Mobile SDKs (`iOS`, `Android`) default to `STATIC` engine libraries unless an explicit product constraint requires shared objects.
- Avoid per-consumer manual transitive runtime dependency wiring.

## Platform Packaging Contract
| Platform | Preferred SDK linkage | Runtime dependency strategy | Loader/path strategy | Validation evidence |
|---|---|---|---|---|
| Linux | `SHARED` client | Stage required `.so` transitive deps into SDK `lib` | `INSTALL_RPATH=$ORIGIN` on SDK `.so` | `ldd` on SDK `.so` and consumer executable has no `not found` |
| macOS | `SHARED` client (desktop apps) | Stage required `.dylib` deps in SDK payload/app bundle | `@loader_path`/`@rpath` install-name policy | `otool -L` shows relocatable paths; app launch smoke |
| Windows | `SHARED` client | Stage required `.dll` files alongside app/SDK bin payload | standard DLL search with app-local staging | `dumpbin /dependents` or equivalent + launch smoke |
| iOS | `STATIC` | no runtime dependency staging (link-time closure required) | N/A for app-local dylib staging | archive/Xcode build + device/sim smoke |
| Android | `STATIC` preferred (`SHARED` only when needed) | if shared, package per-ABI `.so` into APK/AAB | ABI-scoped `jniLibs` / Gradle packaging | per-ABI build + launch smoke |

## Owned Paths
- `m-overseer/agent/projects/multiplatform.md`
- `m-overseer/agent/projects/ASSIGNMENTS.md`
- `m-karma/CMakeLists.txt`
- `m-karma/src/engine/CMakeLists.txt`
- `m-karma/cmake/KarmaEngineConfig.cmake.in`
- `m-karma/.github/workflows/*` (or successor CI)
- `m-bz3/CMakeLists.txt`
- `m-bz3/.github/workflows/*` (consumer SDK smoke)

## Interface Boundaries
- Inputs consumed:
  - current SDK export/install behavior in `m-karma`
  - current SDK consumption path in `m-bz3` via `find_package(KarmaEngine CONFIG REQUIRED)`
- Outputs exposed:
  - stable cross-platform linkage/packaging policy
  - reproducible SDK install payload rules
  - CI smoke gates that verify installed-SDK runtime viability
- Coordinate before changing:
  - `KarmaEngine` exported target names
  - consumer package contract in `KarmaEngineConfig.cmake.in`
  - `abuild.py` profile defaults and backend selector expectations

## Non-Goals
- No redesign of renderer/game/runtime behavior.
- No backend feature parity work.
- No dependency-version refresh campaign beyond packaging requirements.
- No one-off local `LD_LIBRARY_PATH`/`PATH` workaround policy as final solution.

## Execution Plan

### MP0: Linkage Contract Lock
- Introduce explicit SDK linkage policy values (`auto|static|shared`) with platform defaults.
- Document final contract in this file and top-level README snippets.
- Acceptance:
  - policy documented and reflected in CMake options and package docs.

### MP1: Linux Runtime Packaging Hardening
- Keep installed shared client relocatable (`$ORIGIN`).
- Stage required runtime `.so` dependencies in SDK payload (current local unblocker includes `assimp`).
- Add installed-SDK smoke test that launches a consumer binary without environment hacks.
- Acceptance:
  - consumer `bz3 -h` works from clean shell using installed SDK path only.

### MP2: macOS Runtime Packaging Parity
- Implement `@loader_path`/`@rpath` policy and dependency staging for SDK/app payload.
- Add macOS smoke test for installed SDK consumer.
- Acceptance:
  - no absolute/local-machine dylib paths in installed artifacts.

### MP3: Windows Runtime Packaging Parity
- Stage runtime DLL set for SDK consumers.
- Ensure CMake export/import library behavior is deterministic for shared mode.
- Add Windows installed-SDK smoke test.
- Acceptance:
  - consumer runs with app-local DLL payload, no global PATH requirements.

### MP4: Mobile Policy Alignment (iOS/Android)
- iOS: static packaging contract, framework/archive guidance, no transitive runtime staging assumptions.
- Android: static-first policy; if shared path is needed, define ABI packaging contract.
- Acceptance:
  - reproducible reference build instructions for both mobile targets.

### MP5: CI Gate Coverage
- Expand CI from current Linux-only baseline to include installed-SDK consumer smoke on desktop platforms.
- Require gate pass before accepting build-system packaging changes.
- Acceptance:
  - packaging regressions caught pre-merge.

## Local Regression Context (Linux)
- Observed failure mode: consumer executable loaded `libkarma_engine_client.so`, then failed to resolve `libassimpd.so.6`.
- Root cause: shared SDK client had no runtime path/dependency staging contract for transitive shared libs.
- Local unblocker landed:
  - set install runtime path for shared client (`$ORIGIN` on Linux),
  - stage `libassimp*.so*` in SDK `lib` payload for Linux shared SDK runs.
- This unblocks local development but does not complete MP2/MP3/MP4 platform policy work.

## Validation
From `m-karma/` and `m-bz3/`:

```bash
# Linux SDK producer
cd m-karma
export ABUILD_AGENT_NAME=<agent-name>
./abuild.py --claim-lock -d build-sdk
./abuild.py -c -d build-sdk -b bgfx,diligent --install-sdk out/karma-sdk
./abuild.py --release-lock -d build-sdk

# Linux consumer smoke (no LD_LIBRARY_PATH workaround)
cd ../m-bz3
./build-sdk/bz3 -h
./build-a7/bz3 -h

# Overseer docs
cd ../m-overseer
./scripts/lint-projects.sh
```

## First Session Checklist
1. Confirm current linkage mode and package contract in `m-karma` CMake files.
2. Reproduce installed-SDK consumer smoke on Linux from clean shell.
3. Lock `MP0` policy choices before platform-specific implementation slices.
4. Dispatch one bounded platform slice at a time (`MP1` -> `MP2` -> `MP3` -> `MP4`).
5. Add CI gate coverage (`MP5`) before declaring closeout.

## Current Status
- `2026-02-21`: project created and policy targets documented.
- `2026-02-21`: Linux local unblocker validated (`build-sdk` + `build-a7` consumer binaries run without `LD_LIBRARY_PATH`).
- `2026-02-21`: cross-platform packaging parity still pending (`MP2+`).

## Open Questions
- Should desktop policy allow a `STATIC` override for integrators who require single-binary deployment?
- Do we standardize on one SDK output layout (`lib`, `bin`, `cmake`) for all desktop platforms, or permit platform-specific layouts with a normalized CMake contract?
- Should full transitive runtime staging use manual allowlist control or automated dependency discovery with explicit exclude rules?

## Handoff Checklist
- [ ] `MP0` policy lock accepted
- [ ] `MP1` Linux hardening complete with consumer smoke gate
- [ ] `MP2` macOS packaging parity landed
- [ ] `MP3` Windows packaging parity landed
- [ ] `MP4` mobile policy alignment documented and validated
- [ ] `MP5` multi-OS CI gates active
