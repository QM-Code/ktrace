# Audio Backend

## Project Snapshot
- Current owner: `codex`
- Status: `completed` (archived closeout snapshot; non-finite `gain`/`pitch` deterministic rejection parity landed)
- Immediate next task: `none` (reference-only; reopen via a new active project doc if new audio parity/capability scope is approved).
- Validation gate: `./scripts/test-engine-backends.sh`

## Mission
Own engine audio backend behavior and contract parity (`auto|sdl3audio|miniaudio`) across client and optional server-audio paths.

## Backend Priority Policy (2026-02-11)
- `sdl3audio` is the primary/default backend for runtime behavior and feature direction.
- `miniaudio` is retained as:
  - fallback backend path,
  - secondary contract/smoke validation path.
- Do not run co-equal feature-expansion work across both backends by default.
- Expand `miniaudio` beyond shared contract parity only when:
  - a concrete platform/runtime blocker is found, or
  - a product requirement explicitly needs it.

## Primary Specs
- `docs/projects/core-engine-infrastructure.md` (audio sections)
- `docs/projects/engine-backend-testing.md`

## Why This Is Separate
Audio backend work is mostly isolated to engine audio subsystem and can run in parallel with networking, content mount, and UI projects.

## Owned Paths
- `m-rewrite/src/engine/audio/*`
- `m-rewrite/include/karma/audio/*`
- `m-rewrite/src/engine/audio/tests/*`

## Interface Boundaries
- Consumed by: `EngineApp`, `EngineServerApp`, `Game` audio events.
- Exposed contract: `AudioSystem`, backend factory, listener/voice semantics.
- Coordinate before changing:
  - `m-rewrite/src/engine/CMakeLists.txt`
  - `m-rewrite/src/game/game.cpp` (only if contract usage changes)

## Layering Contract Alignment (Implementation-Ready)

## Core Contract Boundaries
- `AudioSystem` is the only engine/game boundary for audio requests, listener updates, and voice lifecycle control.
- `VoiceId` remains opaque and engine-owned; backend device/voice handles must not cross engine/game contracts.
- Core contract scope is lifecycle (`init/update/shutdown`), listener updates, one-shot + controllable voice requests, and deterministic voice validity/control behavior.
- Core request semantics must remain deterministic for valid inputs and deterministic-failure/no-op for invalid/stale inputs.

## Optional 95%-Path Facade Boundaries
- Facade helpers (event-level playback, default bus routing, default positional helpers, voice budgeting helpers) are allowed for common workflows only.
- Facade helpers must compile to core `AudioSystem` request/voice primitives and may not bypass core contracts.
- Facade defaults must remain deterministic and backend-neutral.

## Override/Extension Limits (5% Path)
- Advanced behavior must be exposed through explicit advanced contracts (for example `AudioAdvanced`), not mixed into default-path facade APIs.
- Advanced capability use must be capability-gated and backend-neutral at call boundary.
- Disallowed:
  - backend SDK/device exposure in game code,
  - backend-specific branching in gameplay audio dispatch,
  - caller-owned backend mixer/device lifetime management.

## Validation/Failure Expectations
- Startup failures (invalid backend selection or initialization failure) must map to deterministic contract-level failures with actionable diagnostics.
- Invalid requests (including non-finite/out-of-contract parameters) must fail deterministically and must not allocate hidden voice state.
- Invalid/stale `VoiceId` operations must be deterministic no-op/failure and never crash.
- Unsupported optional capabilities must report explicit contract-level unsupported-capability failures.
- Failure diagnostics must include operation name, reason category, and `VoiceId` when applicable.

## Required Tests and Wrapper-Gate Expectations
- Required first-slice contract tests (from `core-engine-infrastructure.md`):
  - `audio_request_validation_contract_test`
  - `audio_voice_id_lifecycle_contract_test`
  - `audio_listener_sanitization_contract_test`
  - `audio_facade_boundary_contract_test`
  - `audio_override_boundary_contract_test`
  - `engine_audio_loop_contract_smoke_test`
  - `engine_headless_audio_policy_smoke_test`
- Required parity expectation:
  - audio backend parity/smoke coverage must explicitly assert deterministic request rejection + `VoiceId` lifecycle equivalence across all compiled audio backends.
- Required project-level backend validation commands:
  - `./bzbuild.py -c --test-audio build-sdl3-bgfx-jolt-imgui-sdl3audio`
  - `./bzbuild.py -c --test-audio build-sdl3-bgfx-jolt-imgui-miniaudio`
- Required wrapper-gate expectation for handoff:
  - `./scripts/test-engine-backends.sh <build-dir>` must pass for touched assigned audio build dirs.

## Non-Goals
- Netcode event semantics (which events trigger sound) unless explicitly assigned.
- Content mount/world package transfer logic.

## Validation
From `m-rewrite/`:

```bash
./bzbuild.py -c --test-audio build-sdl3-bgfx-jolt-imgui-sdl3audio
./bzbuild.py -c --test-audio build-sdl3-bgfx-jolt-imgui-miniaudio
```

## Trace Channels
- `audio.system`
- `audio.sdl3audio`
- `audio.miniaudio`

## Build/Run Commands
```bash
./bzbuild.py -c --test-audio build-sdl3-bgfx-jolt-imgui-sdl3audio
./bzbuild.py -c --test-audio build-sdl3-bgfx-jolt-imgui-miniaudio
```

## First Session Checklist
1. Read `docs/projects/core-engine-infrastructure.md` audio sections.
2. Verify current backend selection behavior in logs.
3. Make scoped changes under owned paths.
4. Run both assigned isolated audio builds with `./bzbuild.py`.
5. Record behavior/test deltas in status notes.

## Current Status
- `2026-02-10`: Synced this project doc with implementation-ready audio layering contract boundaries and required test/wrapper expectations from `docs/projects/core-engine-infrastructure.md`.
- Backend contract, selection, and smoke test harness are established.
- SDL3audio/miniaudio runtime output paths are active behind `AudioSystem` and remain headless-safe.
- Completed slice: `AudioSystem` auto-backend selection now retries the next compiled backend if the first choice fails initialization.
- Completed slice: smoke test coverage now includes deterministic auto-backend fallback verification via backend-local forced-init-fail env toggles.
- Completed slice: listener/positional semantics are now explicit in engine contract and backend behavior:
  - `PlayRequest.world_position` is optional; unset requests are non-positional.
  - finite `world_position` requests are spatialized against current listener using deterministic distance attenuation + stereo pan.
  - non-finite `world_position` requests are rejected deterministically.
  - listener vectors are sanitized before backend use.
- Completed slice: deterministic smoke assertions now lock listener/positional semantics (valid positional accepted, invalid positional rejected, listener churn with active voices remains stable) for both SDL3audio and miniaudio.
- Completed slice: multi-channel spatial routing policy is explicit and backend-consistent:
  - policy decision: keep stereo-derived fallback for channels `>=2` instead of backend-native speaker-layout mapping.
  - channels `0/1` continue to use stereo pan gains; extra channels receive the stereo average gain.
  - deterministic smoke assertions now lock this behavior to prevent silent backend drift.
- Completed slice: rejected invalid positional requests are now asserted as side-effect-free backend no-ops:
  - invalid `startVoice` and invalid `playOneShot` requests (non-finite `world_position`) must not consume voice allocation state.
- Isolated audio validation passed in assigned build dirs:
  - `build-sdl3-bgfx-jolt-imgui-sdl3audio`: `audio_backend_smoke_sdl3audio` PASS
  - `build-sdl3-bgfx-jolt-imgui-miniaudio`: `audio_backend_smoke_miniaudio` PASS
- Wrapper validation passed in assigned build dir:
  - `./scripts/test-engine-backends.sh build-sdl3-bgfx-jolt-imgui-sdl3audio` PASS (2/2)
- `2026-02-12`: Closeout slice completed for non-finite `gain`/`pitch` handling:
  - SDL3audio and miniaudio backend `AddVoice` paths now reject non-finite `gain` or `pitch` deterministically (`kInvalidVoiceId`) before clamping/mixing.
  - smoke test now asserts rejection + side-effect-free voice allocation semantics for invalid `gain`/`pitch` requests across both backends.
  - isolated audio validation passed:
    - `./bzbuild.py -c --test-audio build-sdl3-bgfx-jolt-imgui-sdl3audio`
    - `./bzbuild.py -c --test-audio build-sdl3-bgfx-jolt-imgui-miniaudio`
  - wrapper closeout passed with explicit build dirs:
    - `./scripts/test-engine-backends.sh build-sdl3-bgfx-jolt-imgui-sdl3audio` (`2/2`)
    - `./scripts/test-engine-backends.sh build-sdl3-bgfx-jolt-imgui-miniaudio` (`2/2`)
- This project should prioritize backend correctness and headless-safe determinism over feature expansion.

## Open Questions
- Should server-side audio remain strictly opt-in in all deployment profiles?
- If a future engine contract needs speaker-layout-specific behavior (rear/LFE semantics), should we introduce an explicit engine routing mode switch while preserving current default behavior?

## Handoff Checklist
- [x] Audio contract preserved.
- [x] Backend smoke tests green.
- [x] Headless-safe behavior retained.
- [x] Docs updated for contract/selection changes.
- [x] Listener/positional semantics decision is explicit and covered by deterministic smoke assertions.
- [x] Multi-channel spatial routing policy is explicit (stereo-derived fallback) and covered by deterministic smoke assertions.
- [x] Rejected invalid positional requests are asserted side-effect-free (no hidden voice allocation on no-op paths).
