# Multiplatform Build + SDK Packaging

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (SDK-only policy pivot accepted; MP0 contract lock + CMake migration planning active)`
- Immediate next task: lock SDK-only contract details and execute `MP0-SDK1` (remove `KARMA_INSTALL_ENGINE_SDK` mode branches while preserving platform linkage policy).
- Validation gate:
  - `m-overseer`: `./scripts/lint-projects.sh`
  - `m-karma`: `./abuild.py -c -d build-sdk -b bgfx,diligent --install-sdk out/karma-sdk`
  - `m-bz3`: `./build-sdk/bz3 -h` and `./build-a7/bz3 -h`

## Mission
Define and implement one maintainable, explicit SDK-only build+packaging contract for `KarmaEngine` outputs across Linux/macOS/Windows/iOS/Android so consumers can build and run without ad-hoc runtime dependency fixes.

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
- `m-karma` operates as an SDK producer only (no separate non-SDK mode branch).
- Desktop SDKs (`Linux`, `macOS`, `Windows`) default to `SHARED` `engine_client` with runtime dependency staging.
- Mobile SDKs (`iOS`, `Android`) default to `STATIC` engine libraries unless an explicit product constraint requires shared objects.
- Avoid per-consumer manual transitive runtime dependency wiring.

## SDK-Only Contract Impact
- Package/export generation is always part of the `m-karma` configure graph.
- `KarmaEngine` targets (`karma::engine_core`, `karma::engine_client`) remain the consumer-facing contract.
- Local build/test binaries still exist for engine validation, but they are treated as validation artifacts of the SDK-producing build graph.
- `abuild.py --install-sdk <prefix>` remains the explicit install step; "SDK-only" here removes build-graph mode branching, not install-prefix control.

## Required CMake Migration To Remove SDK Mode
`MP0-SDK1` file-level change plan:

1. `m-karma/cmake/10_backend_options.cmake`
- remove `option(KARMA_INSTALL_ENGINE_SDK ...)` as a build-graph mode switch.
- add/lock explicit SDK linkage policy cache variable (for example `KARMA_SDK_LINKAGE=auto|shared|static`) if not already present.

2. `m-karma/cmake/40_engine_subdir.cmake`
- replace `if(KARMA_INSTALL_ENGINE_SDK)` branching with policy-driven linkage defaults.
- enforce platform defaults:
  - desktop: `karma_engine_core` + `karma_engine_client` shared
  - mobile: static (unless explicit override is approved)

3. `m-karma/cmake/50_sdk_install_export.cmake`
- remove top-level `if(KARMA_INSTALL_ENGINE_SDK)` guard.
- always install/export SDK targets, headers, and runtime sidecar staging policy assets.

4. `m-karma/cmake/60_package_config.cmake`
- remove top-level `if(KARMA_INSTALL_ENGINE_SDK)` guard.
- always generate/install `KarmaEngineConfig.cmake` + version file + dependency contract logic.

5. `m-karma/src/engine/cmake/targets.cmake`
- keep target/export names stable (`karma::engine_core`, `karma::engine_client`).
- keep SDK artifact names stable (`libkarma_core.so`, `libkarma_client.so` on Linux).
- ensure PIC/RPATH assumptions remain valid under shared desktop defaults.

6. `m-karma/cmake/20_dependencies.cmake`
- keep static third-party targets linked into shared SDK artifacts PIC-safe (current ENet PIC enforcement is part of this policy class).

7. Documentation updates
- update `m-overseer/agent/docs/building.md` to remove references implying non-SDK mode.
- keep consumer contract docs centered on installed SDK + `find_package(KarmaEngine CONFIG REQUIRED)`.

## `m-bz3` Expected Impact
- No code-level target-name changes expected while `karma::engine_core` / `karma::engine_client` stay stable.
- `m-bz3` should continue consuming by package discovery:
  - `find_package(KarmaEngine CONFIG REQUIRED)`
  - optional `KARMA_SDK_ROOT` / `--karma-sdk` path injection for local workflows.
- Required validation after migration: re-run installed-SDK consumer smoke (`bz3 -h`, `bz3-server -h`, and server-net wrapper).

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

### MP0: SDK-Only Contract Lock + CMake Migration
- Lock SDK-only policy and linkage defaults (`auto|static|shared`) with platform mapping.
- Execute `MP0-SDK1`: remove `KARMA_INSTALL_ENGINE_SDK` mode guards from CMake package/install/export path.
- Keep exported target contract stable (`karma::engine_core`, `karma::engine_client`) through migration.
- Acceptance:
  - no CMake branch remains where package/export generation is disabled.
  - installed SDK producer+consumer smoke passes on Linux.
  - policy + migration details documented in this file.

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
- Observed failure mode: consumer executable loaded shared Karma SDK client, then failed to resolve `libassimpd.so.6`.
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
2. Execute `MP0-SDK1` CMake migration (remove SDK mode toggle branches).
3. Reproduce installed-SDK consumer smoke on Linux from clean shell.
4. Dispatch one bounded platform slice at a time (`MP1` -> `MP2` -> `MP3` -> `MP4`).
5. Add CI gate coverage (`MP5`) before declaring closeout.

## Current Status
- `2026-02-21`: project created and policy targets documented.
- `2026-02-21`: Linux local unblocker validated (`build-sdk` + `build-a7` consumer binaries run without `LD_LIBRARY_PATH`).
- `2026-02-21`: SDK artifact naming normalized to `libkarma_core.so` / `libkarma_client.so`; consumer package-target contract remains stable.
- `2026-02-21`: SDK-only policy direction accepted; `MP0-SDK1` migration plan drafted in this project doc.
- `2026-02-21`: cross-platform packaging parity still pending (`MP1+` execution not closed).

## Open Questions
- Should desktop policy allow a `STATIC` override for integrators who require single-binary deployment, or should SDK-only desktop policy be hard `SHARED`?
- Do we standardize on one SDK output layout (`lib`, `bin`, `cmake`) for all desktop platforms, or permit platform-specific layouts with a normalized CMake contract?
- Should full transitive runtime staging use manual allowlist control or automated dependency discovery with explicit exclude rules?

## Handoff Checklist
- [ ] `MP0` policy lock accepted
- [ ] `MP1` Linux hardening complete with consumer smoke gate
- [ ] `MP2` macOS packaging parity landed
- [ ] `MP3` Windows packaging parity landed
- [ ] `MP4` mobile policy alignment documented and validated
- [ ] `MP5` multi-OS CI gates active
