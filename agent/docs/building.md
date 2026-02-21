# Overseer Execution Policy

Purpose:
- define canonical delegated execution mechanics,
- keep build/validation behavior deterministic across specialists.

## Execution Root
- Run commands from the assigned repo root for the active project.
- If launching from workspace root, use explicit repo prefixes or `cd` first.

## Build Policy
- Use `./abuild.py -c -d <build-dir>` for delegated configure/build/test flows.
- Omit `-c` only when intentionally reusing a configured build dir.
- Do not use raw `cmake -S/-B` for delegated specialist work.

Default-first backend rule:
- Most slices should omit `-b`.
- Use `-b` only for categories touched by the slice.

Examples:
- default: `./abuild.py -c -d <build-dir>`
- renderer runtime-select: `./abuild.py -c -d <build-dir> -b bgfx,diligent`
- ui runtime-select: `./abuild.py -c -d <build-dir> -b imgui,rmlui`
- physics runtime-select: `./abuild.py -c -d <build-dir> -b jolt,physx`

## Build Slot Ownership
Required in parallel delegated work:
- set agent identity: `export ABUILD_AGENT_NAME=<agent-name>`
- claim slot before first build: `./abuild.py --claim-lock -d <build-dir>`
- release on retire/transfer: `./abuild.py --release-lock -d <build-dir>`

Rules:
- use only assigned `build-*` slots,
- pass explicit build-dir args to wrapper scripts,
- `--ignore-lock` is emergency-only and requires overseer approval.

## Toolchain Policy
- Local repo `./vcpkg` is mandatory for delegated builds.
- Missing/unbootstrapped local `./vcpkg` is a blocker; escalate instead of improvising external toolchains.

## Validation Gates
Core wrappers:
- `./scripts/test-engine-backends.sh <build-dir>`
- `./scripts/test-server-net.sh <build-dir>`

Touch-scope rules:
- network/transport/protocol scope -> server-net wrapper required.
- physics/audio/backend-test wiring scope -> engine-backends wrapper required.
- cross-scope changes -> run both wrappers.
- docs-only/project-tracking edits -> wrappers optional unless project doc says otherwise.

## Demo Fixture Policy
Reusable local test/demo state must live under tracked `demo/` roots:
- `demo/communities/*`
- `demo/users/*`
- `demo/worlds/*`

Avoid relying on personal `~/.config/...` or ad-hoc `/tmp` state for durable workflows.

## KARMA -> BZ3 SDK Contract (Locked)
- Consumer integration path is package-based only:
  - `find_package(KarmaEngine CONFIG REQUIRED)`
- Diligent renderer dependency is package-based in both repos:
  - `find_package(DiligentEngine CONFIG REQUIRED)`
  - source should come from repo overlay port `vcpkg-overlays/diligentengine` (no `FetchContent` fallback)
- Raw include/lib wiring from consumer into KARMA build artifacts is disallowed.

Canonical commands:
- producer (`m-karma`):
  - `./abuild.py -c -d build-sdk --install-sdk out/karma-sdk`
- consumer (`m-bz3`):
  - `./abuild.py -c -d build-sdk --karma-sdk ../m-karma/out/karma-sdk --ignore-lock`

Required imported targets:
- `karma::engine_core`
- `karma::engine_client`

## Handoff Minimum
Every specialist handoff must include:
- files changed,
- exact commands run + outcomes,
- open risks/questions,
- project-doc and assignment-row updates.
