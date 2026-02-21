# FetchContent Fallback Rationalization (`m-karma` + `m-bz3`)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (safe fallback cleanup landed; wrapper validation complete on dedicated slots)`
- Immediate next task: decide closure/archive and whether to spin a follow-on for transport-loopback registration diagnostics in SDK consumer profiles.
- Validation gate:
  - `m-karma`: `ABUILD_AGENT_NAME=overseer-fallbacks ./abuild.py -c -d build-fallbacks-sdk -b bgfx`, `ABUILD_AGENT_NAME=overseer-fallbacks ./abuild.py -c -d build-fallbacks-static -b bgfx --sdk-linkage static`, `./scripts/test-engine-backends.sh build-fallbacks-sdk`, `ABUILD_AGENT_NAME=overseer-fallbacks ./abuild.py -d build-fallbacks-sdk --install-sdk out/karma-sdk-fallbacks`
  - `m-bz3`: `ABUILD_AGENT_NAME=overseer-fallbacks ./abuild.py -c -d build-fallbacks-sdk -b bgfx --karma-sdk ../m-karma/out/karma-sdk-fallbacks`, `ABUILD_AGENT_NAME=overseer-fallbacks KARMA_SDK_ROOT=../m-karma/out/karma-sdk-fallbacks ./scripts/test-server-net.sh build-fallbacks-sdk`
  - `m-overseer`: `./agent/scripts/lint-projects.sh`

## Mission
Document exactly which FetchContent fallbacks are still needed under SDK + vcpkg policy, which ones are not being used in real configured builds, and provide concrete instructions to remove the unnecessary fallback code safely.

## Foundation References
- `m-karma/cmake/20_dependencies.cmake`
- `m-karma/cmake/30_bgfx.cmake`
- `m-karma/cmake/31_diligent.cmake`
- `m-bz3/cmake/20_dependencies.cmake`
- `m-bz3/cmake/30_bgfx.cmake`
- `m-karma/build-sdk/CMakeCache.txt`
- `m-karma/build-sdk-static/CMakeCache.txt`
- `m-bz3/build-sdk/CMakeCache.txt`
- `m-bz3/build-a7/CMakeCache.txt`

## Why This Is Separate
This is a cross-repo dependency policy track. It is mostly CMake dependency-sourcing cleanup, not gameplay/rendering feature work.

## Owned Paths
- `m-overseer/agent/projects/fallbacks.md`
- `m-overseer/agent/projects/ASSIGNMENTS.md`
- planned implementation files:
  - `m-karma/cmake/20_dependencies.cmake`
  - `m-bz3/cmake/20_dependencies.cmake`

## Interface Boundaries
- Inputs consumed:
  - current configured build artifacts/caches (`build-sdk`, `build-sdk-static`, `build-a7`)
  - current CMake fallback logic in `20_dependencies.cmake` and renderer logic in `30_bgfx.cmake`
- Outputs exposed:
  - explicit keep/remove decision matrix for fallback logic
  - concrete implementation steps for safe cleanup
- Coordinate before changing:
  - `m-karma/cmake/30_bgfx.cmake` dual-renderer policy
  - SDK export behavior in `m-karma/src/engine/cmake/client_wiring.cmake`

## Non-Goals
- No further dependency-source migrations beyond this track's package-only cleanup and prior `stb`/`enet`/`bgfx` updates.
- No backend architecture rewrite.
- No runtime behavior changes.

## Baseline (Historical, Superseded By 2026-02-21 Changes)
Configured builds checked:
- `m-karma/build-sdk` (shared, `bgfx;diligent`)
- `m-karma/build-sdk-static` (static, `bgfx`)
- `m-bz3/build-sdk` (SDK consumer, `bgfx`)
- `m-bz3/build-a7` (SDK consumer, `bgfx`)

### Safe to Remove Now (fallback path not used)
1. `glm` fallback: **not used**
   - resolved from vcpkg (`glm_DIR=.../vcpkg_installed/.../share/glm`).
2. `assimp` fallback: **not used**
   - resolved from vcpkg (`assimp_DIR=.../share/assimp`).
3. `spdlog` fallback: **not used**
   - resolved from vcpkg (`spdlog_DIR=.../share/spdlog`).
4. `miniz` fallback: **not used**
   - resolved from vcpkg (`miniz_DIR=.../share/miniz`).
5. `nlohmann_json` fallback: **not used**
   - resolved from vcpkg (`nlohmann_json_DIR=.../share/nlohmann_json`).

### Cannot Safely Remove Yet
1. `stb` fallback: **still used**
   - `stb_DIR=stb_DIR-NOTFOUND` in active caches.
   - compile include path uses `_deps/stb-src` in active builds.
   - Removing now would break image-header include resolution unless replaced by a required package source.
2. `enet` FetchContent: **still used (currently unconditional)**
   - active link lines include `_deps/enet-build/libenet.a`.
   - SDK exports rely on `enet::enet_static` target presence.
   - Removing without package replacement + target wiring updates breaks link/export contracts.
3. `bgfx` fallback: **still needed for combined renderer path**
   - `m-karma` combined builds use `_deps/bgfx-build` libraries.
   - dual-renderer path in `m-karma/cmake/30_bgfx.cmake` forces package path off when Diligent is enabled to avoid glslang target collisions.
   - bgfx-only builds can resolve from vcpkg, but combined path still needs current fallback behavior.

## Implementation Instructions (Safe Removal Only)
Apply the same cleanup in both:
- `m-karma/cmake/20_dependencies.cmake`
- `m-bz3/cmake/20_dependencies.cmake`

1. Keep `include(FetchContent)` because `stb` and `enet` still need it.
2. Replace fallback blocks with required package resolution:
   - `glm`
   - `assimp`
   - `spdlog`
   - `miniz`
   - `nlohmann_json`
3. Remove associated `FetchContent_Declare(...)` / `FetchContent_MakeAvailable(...)` fallback code for those five dependencies.
4. Keep `stb` fallback block unchanged.
5. Keep unconditional `enet` FetchContent block unchanged.
6. Do not change `m-karma/cmake/30_bgfx.cmake` in this slice.

Expected post-change behavior:
- SDK/vcpkg-configured builds continue to work.
- Non-vcpkg/non-SDK builds fail fast at configure time for these dependencies (intentional under current policy).

## Validation
```bash
# m-karma
cd m-karma
export ABUILD_AGENT_NAME=overseer-fallbacks
./abuild.py --claim-lock -d build-fallbacks-sdk
./abuild.py --claim-lock -d build-fallbacks-static
./abuild.py -c -d build-fallbacks-sdk -b bgfx
./abuild.py -c -d build-fallbacks-static -b bgfx --sdk-linkage static
./scripts/test-engine-backends.sh build-fallbacks-sdk
./abuild.py -d build-fallbacks-sdk --install-sdk out/karma-sdk-fallbacks

# m-bz3
cd ../m-bz3
export ABUILD_AGENT_NAME=overseer-fallbacks
./abuild.py --claim-lock -d build-fallbacks-sdk
./abuild.py -c -d build-fallbacks-sdk -b bgfx --karma-sdk ../m-karma/out/karma-sdk-fallbacks
KARMA_SDK_ROOT=../m-karma/out/karma-sdk-fallbacks ./scripts/test-server-net.sh build-fallbacks-sdk
./abuild.py --release-lock -d build-fallbacks-sdk

# release m-karma locks
cd ../m-karma
./abuild.py --release-lock -d build-fallbacks-sdk
./abuild.py --release-lock -d build-fallbacks-static

# project-doc lint
cd ../m-overseer
./agent/scripts/lint-projects.sh
```

## Build/Run Commands
```bash
cd m-karma
export ABUILD_AGENT_NAME=overseer-fallbacks
./abuild.py --claim-lock -d build-fallbacks-sdk
./abuild.py --claim-lock -d build-fallbacks-static
./abuild.py -c -d build-fallbacks-sdk -b bgfx
./abuild.py -c -d build-fallbacks-static -b bgfx --sdk-linkage static
./scripts/test-engine-backends.sh build-fallbacks-sdk
./abuild.py -d build-fallbacks-sdk --install-sdk out/karma-sdk-fallbacks

cd ../m-bz3
export ABUILD_AGENT_NAME=overseer-fallbacks
./abuild.py --claim-lock -d build-fallbacks-sdk
./abuild.py -c -d build-fallbacks-sdk -b bgfx --karma-sdk ../m-karma/out/karma-sdk-fallbacks
KARMA_SDK_ROOT=../m-karma/out/karma-sdk-fallbacks ./scripts/test-server-net.sh build-fallbacks-sdk
./abuild.py --release-lock -d build-fallbacks-sdk

cd ../m-karma
./abuild.py --release-lock -d build-fallbacks-sdk
./abuild.py --release-lock -d build-fallbacks-static
```

## First Session Checklist
1. Reconfirm cache provenance in active build dirs.
2. Remove only the five safe fallback blocks in both repos.
3. Reconfigure both repos with wrapper commands.
4. Run required wrapper tests.
5. Update this doc and `ASSIGNMENTS.md` with final status.

## Current Status
- `2026-02-21`: Baseline captured from active build caches and link lines.
- `2026-02-21`: Safe-remove set confirmed: `glm`, `assimp`, `spdlog`, `miniz`, `nlohmann_json`.
- `2026-02-21`: Keep set confirmed: `stb`, `enet`, and conditional `bgfx` for combined renderer builds.
- `2026-02-21`: Implemented `stb` migration from FetchContent to vcpkg `Stb` (`find_package(Stb REQUIRED)` + imported `stb::stb` target) in both repos.
- `2026-02-21`: Implemented `enet` migration from FetchContent to vcpkg `unofficial-enet` (`find_package(unofficial-enet CONFIG REQUIRED)` + stable `enet::enet_static` alias) in both repos.
- `2026-02-21`: Added `enet` to `m-karma/vcpkg.json` dependencies.
- `2026-02-21`: Removed bgfx FetchContent fallback in both repos; bgfx is now `find_package(bgfx CONFIG REQUIRED)`.
- `2026-02-21`: Explicitly disabled combined renderer mode (`bgfx+diligent`) in both repos at backend option parse time.
- `2026-02-21`: Wrapper validation:
  - `m-karma`: `./abuild.py -c -d build-sdk-10 -b bgfx` passed after ENet include/API compatibility updates.
  - `m-bz3`: `./abuild.py -c -d build-sdk-10 -b bgfx` passed.
  - Combined guard validated in both repos via `./abuild.py -c -d build-combined-check -b bgfx,diligent` (fails with intentional policy error).
- `2026-02-21`: Removed remaining safe fallback blocks (`glm`, `assimp`, `spdlog`, `miniz`, `nlohmann_json`) and `include(FetchContent)` from both `m-karma/cmake/20_dependencies.cmake` and `m-bz3/cmake/20_dependencies.cmake`.
- `2026-02-21`: `m-karma` validation passed on dedicated slots:
  - `ABUILD_AGENT_NAME=overseer-fallbacks ./abuild.py -c -d build-fallbacks-sdk -b bgfx`
  - `ABUILD_AGENT_NAME=overseer-fallbacks ./abuild.py -c -d build-fallbacks-static -b bgfx --sdk-linkage static`
  - `./scripts/test-engine-backends.sh build-fallbacks-sdk` (2/2 tests passed).
- `2026-02-21`: Installed SDK for consumer validation:
  - `ABUILD_AGENT_NAME=overseer-fallbacks ./abuild.py -d build-fallbacks-sdk --install-sdk out/karma-sdk-fallbacks`
- `2026-02-21`: `m-bz3` validation passed with explicit SDK prefix:
  - `ABUILD_AGENT_NAME=overseer-fallbacks ./abuild.py -c -d build-fallbacks-sdk -b bgfx --karma-sdk ../m-karma/out/karma-sdk-fallbacks`
  - `ABUILD_AGENT_NAME=overseer-fallbacks KARMA_SDK_ROOT=../m-karma/out/karma-sdk-fallbacks ./scripts/test-server-net.sh build-fallbacks-sdk` (9/9 gameplay tests passed).
  - Transport loopback suite not registered in this profile (wrapper emitted expected diagnostic; not a gate failure).

## Open Questions
- Should `scripts/test-server-net.sh` accept/pass `--karma-sdk` (or set `KARMA_SDK_ROOT`) internally so SDK consumer slots do not require external environment setup?
- Should transport loopback registration (`network_test_support`) for this SDK consumer profile be tracked as a separate follow-on project?

## Handoff Checklist
- [x] Provenance evidence captured from configured build trees.
- [x] Keep/remove matrix documented.
- [x] Concrete implementation steps documented.
- [x] CMake cleanup edits landed in `m-karma` + `m-bz3`.
- [x] Full wrapper test suites executed (`test-engine-backends.sh`, `test-server-net.sh`).
