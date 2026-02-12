# Engine Backend Testing

## Project Snapshot
- Current owner: `unassigned`
- Status: `in progress`
- Immediate next task: keep backend tests aligned with real physics/audio implementation changes and add one assertion per new backend capability.
- Validation gate: `./scripts/test-engine-backends.sh`

## Project Linkage
- Physics project: `docs/projects/physics-backend.md`
- Audio project (reference-only closeout snapshot): `docs/archive/audio-backend-completed-2026-02-12.md`
- Governance project: `docs/projects/testing-ci-docs.md`

## Purpose
This document is the handoff point for physics/audio backend test coverage in the rewrite phase.

Use this first when touching:
- `m-rewrite/src/engine/physics/*`
- `m-rewrite/src/engine/audio/*`
- `m-rewrite/include/karma/physics/*`
- `m-rewrite/include/karma/audio/*`
- `m-rewrite/src/engine/CMakeLists.txt` (backend test registration)

## Current Engine Backend Test Suite

1. `physics_backend_parity_jolt`
- File: `m-rewrite/src/engine/physics/tests/physics_backend_parity_test.cpp`
- CTest command: `physics_backend_parity_test --backend jolt`
- Scope:
  - backend selection/init/shutdown contract checks
  - body create/destroy and transform set/get behavior
  - fixed-step gravity, ground contact, and stability sanity checks
  - reinit and repeatability checks
- Expected in normal environments: `PASS` (when Jolt backend is compiled/enabled)

2. `physics_backend_parity_physx`
- File: `m-rewrite/src/engine/physics/tests/physics_backend_parity_test.cpp`
- CTest command: `physics_backend_parity_test --backend physx`
- Scope:
  - same parity contract as Jolt variant, for PhysX backend
- Expected in normal environments: `PASS` (when PhysX backend is compiled/enabled)

3. `audio_backend_smoke_sdl3audio`
- File: `m-rewrite/src/engine/audio/tests/audio_backend_smoke_test.cpp`
- CTest command: `audio_backend_smoke_test --backend sdl3audio`
- Scope:
  - backend selection/init/shutdown contract checks
  - uninitialized API safety checks
  - voice lifecycle + reinit smoke checks
  - one-shot + looped request path smoke coverage
- Environment property:
  - `SDL_AUDIODRIVER=dummy` for headless-safe execution
- Expected in normal environments: `PASS` (when SDL3audio backend is compiled/enabled)

4. `audio_backend_smoke_miniaudio`
- File: `m-rewrite/src/engine/audio/tests/audio_backend_smoke_test.cpp`
- CTest command: `audio_backend_smoke_test --backend miniaudio`
- Scope:
  - same smoke contract as SDL3audio variant, for miniaudio backend
- Environment properties:
  - `KARMA_MINIAUDIO_FORCE_NULL=1`
  - `SDL_AUDIODRIVER=dummy`
- Expected in normal environments: `PASS` (when miniaudio backend is compiled/enabled)

## How To Run

From `m-rewrite/`:

```bash
./scripts/test-engine-backends.sh

# Equivalent explicit commands:
cmake --build build-dev --target \
  physics_backend_parity_test \
  audio_backend_smoke_test

ctest --test-dir build-dev -R "physics_backend_parity_jolt|physics_backend_parity_physx|audio_backend_smoke_sdl3audio|audio_backend_smoke_miniaudio" --output-on-failure
```

Notes:
- Some build profiles compile out specific backend tests.
- `./scripts/test-engine-backends.sh` handles this by detecting zero registered engine backend tests and exiting successfully with a skip message.
- `.github/workflows/core-test-suite.yml` runs this same wrapper (`./scripts/test-engine-backends.sh`) on pull requests and main/master pushes.

## Result Interpretation Rules

1. Any registered engine backend test fails
- Treat as actionable regression in backend contract behavior.

2. No engine backend tests are registered
- Build profile likely has those backends/tests compiled out.
- Non-actionable for runtime behavior in that profile, but should be called out in handoff notes.

## Agent Workflow Guardrails

1. Keep backend tests deterministic:
- use fixed-step simulation sequences and bounded tolerances
- avoid dependence on real audio hardware (dummy/null driver paths only)

2. Keep backend contract checks ahead of feature checks:
- if backend API/lifecycle semantics change, update tests first
- avoid silent behavior drift across Jolt/PhysX and SDL3audio/miniaudio

3. Keep this project file in sync:
- update this file when adding/removing backend tests, wrappers, or run requirements
