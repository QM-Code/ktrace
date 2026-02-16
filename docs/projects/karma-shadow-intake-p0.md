# KARMA Shadow Intake P0 (Integration Track)

## Project Snapshot
- Current owner: `overseer`
- Status: `priority/in progress (KS0 cache-invalidation + KS1 contract/config scaffolding + KS2.1 directional GPU shadow hardening + KS2.2 local-light source contract/shading bridge landed)`
- Immediate next task: continue `KS2` by implementing bounded point-shadow map generation/sampling for selected shadow-casting local lights in both backends.
- Validation gate: renderer build gates in both assigned build dirs, sandbox/runtime trace evidence, and docs lint.

## Mission
Intake high-value shadowing capability updates from `KARMA-REPO` (latest upstream: `20bdf28`, `905b63b`) into `m-rewrite` under rewrite-owned contracts, without mirroring KARMA file layout.

P0 objective:
- preserve current accepted directional `gpu_default` behavior,
- add bounded point-shadow capability + refresh behavior where it improves visible parity and stability,
- keep backend parity across BGFX + Diligent.

## Foundation References
- `docs/foundation/policy/rewrite-invariants.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/policy/decisions-log.md`
- `docs/projects/renderer-parity.md`
- `docs/projects/renderer-shadow-hardening.md`

## Why This Is Separate
This intake cuts across renderer contracts, backend internals, config policy, and visual acceptance evidence. Keeping it in a dedicated integration track avoids destabilizing active closeout work while preserving explicit adopt/defer boundaries for KARMA deltas.

## Strategic Labeling
- Primary track label: `KARMA intake`
- Secondary track label: `shared unblocker` (renderer visual parity and stability)

## Scope Intake (From KARMA Commits)
Adopt now (P0 bounded scope):
1. Point-light shadow map generation/sampling for shadow-casting point lights (bounded light count and update budget).
2. Moving-caster-aware point-shadow refresh invalidation (dirty only impacted faces/slots).
3. Runtime policy/config plumbing for point-shadow bias controls and local-light interaction terms needed for deterministic tuning.
4. Trace visibility for update reasons (cache hit/miss, dirty face updates, fallback causes).

Defer (not P0 in this track):
1. Radar camera shader-override demo workflow.
2. Broad camera override shader system beyond what is needed for shadowing acceptance.
3. Full renderer-pipeline refactors not required for shadow quality/stability closeout.

## Owned Paths
- `m-rewrite/docs/projects/karma-shadow-intake-p0.md`
- `m-rewrite/docs/projects/ASSIGNMENTS.md`
- `m-rewrite/src/engine/renderer/backends/directional_shadow_internal.hpp`
- `m-rewrite/src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
- `m-rewrite/src/engine/renderer/backends/diligent/backend_diligent.cpp`
- `m-rewrite/include/karma/renderer/types.hpp`
- `m-rewrite/src/engine/renderer/tests/*`
- `m-rewrite/data/client/config.json`

## Interface Boundaries
- Inputs consumed:
  - KARMA commit intent from `20bdf28` and `905b63b`.
  - Existing shadow contracts/acceptance criteria from `renderer-parity.md` and `renderer-shadow-hardening.md`.
- Outputs exposed:
  - rewrite-owned point-shadow contract behavior with backend parity,
  - deterministic update/fallback diagnostics and bounded config surface.
- Coordinate before changing:
  - `m-rewrite/docs/projects/renderer-parity.md`
  - `m-rewrite/docs/projects/renderer-shadow-hardening.md`
  - `m-rewrite/docs/foundation/architecture/core-engine-contracts.md`

## Non-Goals
- Do not mirror KARMA source layout/abstractions one-to-one.
- Do not expand into gameplay/netcode/UI behavior.
- Do not start platform-backend expansion.
- Do not open unbounded renderer-material feature work outside shadow correctness/stability.

## Execution Slices
1. `KS1` contract + config slice:
   - define bounded point-shadow settings and defaults under rewrite config policy.
   - add parser/contract tests for settings and clamps.
2. `KS2` backend implementation slice:
   - implement point-shadow map generation/sampling parity in BGFX + Diligent with bounded light budget.
3. `KS3` refresh invalidation slice:
   - implement moving-caster and light-motion dirtying policy with per-frame budgeted updates.
4. `KS4` tuning + closeout slice:
   - lock defaults from bounded sweep evidence and record operator visual checkpoints.

## Acceptance Criteria
1. Both renderer build dirs compile and run with point-shadow path enabled.
2. Point-shadow refresh occurs when relevant moving casters/lights invalidate cached faces.
3. Trace evidence includes clear update/fallback reason tokens.
4. Directional-shadow existing `gpu_default` behavior remains green (no regression).
5. Visual parity checkpoints are documented for BGFX + Diligent.

## Validation
From `m-rewrite/`:

```bash
./bzbuild.py -c build-sdl3-bgfx-physx-imgui-sdl3audio
./bzbuild.py -c build-sdl3-diligent-physx-imgui-sdl3audio
./scripts/run-renderer-shadow-sandbox.sh 20 16 20
timeout -k 2s 20s ./build-sdl3-bgfx-physx-imgui-sdl3audio/bz3 -d ./data --strict-config=true --config data/client/config.json -v -t engine.sim,render.system,render.bgfx,render.mesh
timeout -k 2s 20s ./build-sdl3-diligent-physx-imgui-sdl3audio/bz3 -d ./data --strict-config=true --config data/client/config.json -v -t engine.sim,render.system,render.diligent,render.mesh
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `render.system`
- `render.bgfx`
- `render.diligent`
- `render.mesh`
- `engine.sim`

## Build Dirs (Assigned)
- `build-sdl3-bgfx-physx-imgui-sdl3audio`
- `build-sdl3-diligent-physx-imgui-sdl3audio`

## Current Status
- `2026-02-16`: P0 integration track created from latest KARMA shadowing deltas.
- `2026-02-16`: Upstream intake candidates classified:
  - adopt-now: point-shadow map path, dirty refresh policy for moving casters, bounded runtime tuning controls.
  - deferred: radar/camera shader-override demo path and non-essential wide renderer refactors.
- `2026-02-16`: `KS0` landed (behavior-first parity intake from KARMA cache policy):
  - both BGFX and Diligent now force directional shadow-map refresh when camera/light/caster inputs change, instead of relying on cadence-only rebuilds.
  - adopted thresholds mirror KARMA posture (`position ~= 0.12`, `direction ~= 0.3 deg`) with transform-delta detection per submitted shadow caster sequence.
  - this is an intake bridge for shadow stability and responsiveness; point-shadow topology itself is still pending `KS1/KS2`.
- `2026-02-16`: `KS0.1` directional quality baseline aligned toward KARMA demo posture:
  - updated rewrite runtime config defaults used by `bz3` (`data/client/config.json`) to increase directional shadow fidelity (`mapSize=2048`, `pcfRadius=2`, `updateEveryFrames=1`).
  - operator runtime check confirmed visibly improved shadow quality in `bz3` while staying on the existing `gpu_projection` directional path.
- `2026-02-16`: `KS0.2` quality/perf rebalance after operator FPS feedback:
  - reduced directional shadow runtime defaults to `mapSize=1024`, `pcfRadius=1` while keeping `updateEveryFrames=1`.
  - intent: preserve clear quality gain over legacy `512/1` while removing the heavy frame-time cost from `2048/2`.
- `2026-02-16`: `KS0.3` operator quality lock:
  - directional runtime defaults restored/locked at `mapSize=2048`, `pcfRadius=2`, `updateEveryFrames=1` per operator directive to avoid quality regression while intake continues.
- `2026-02-16`: `KS1` contract/config scaffolding landed:
  - extended rewrite directional-shadow runtime contract with bounded point-shadow/local-light tuning fields (map size, shadow-light cap, face budget, bias/tuning controls, AO/local-light interaction and directional-lift control).
  - wired parser support from client config (`roamingMode.graphics.lighting.shadows.*`) and expanded `render.system` trace payload to emit the new control surface for deterministic evidence capture.
  - added contract-test coverage for clamp/fallback behavior over new fields in `directional_shadow_contract_test`.
- `2026-02-16`: `KS2.1` directional GPU shadow hardening landed (KARMA-aligned sub-slice while point-light source contract is pending):
  - both BGFX and Diligent now compute mesh-local shadow caster bounding spheres and cull out-of-shadow-volume casters during GPU shadow submission.
  - both backends now emit `gpu shadow pass ... culled=<n>` trace evidence.
  - directional PCF sampling shader path now uses bounded radius-specialized loops (`0/1/2/3/4`) with early return for non-lit surfaces, reducing per-pixel shadow sampling overhead without lowering configured quality settings.
- `2026-02-16`: `KS2.2` local-light source contract/shading bridge landed (point-shadow pass itself still pending):
  - renderer contract now exposes rewrite-owned local light sources (`LightData`) through `GraphicsDevice`/`RenderSystem`, with scene-owned light ingestion via `scene::LightComponent`.
  - BGFX + Diligent forward shaders now consume bounded local light uniforms (max 4) and apply KARMA-aligned attenuation plus `localLightDirectionalShadowLiftStrength` modulation over directional shadowing.
  - both backends now emit `point shadow status ... reason=<token>` trace evidence (`point_shadow_pass_unimplemented`, `point_shadow_no_shadow_lights`, etc.) to keep KS2 point-shadow rollout observable while the generation/sampling pass is still in flight.
- Validation evidence (`m-rewrite/`):
  - `./scripts/test-engine-backends.sh build-sdl3-bgfx-physx-imgui-sdl3audio` ✅
  - `./scripts/test-engine-backends.sh build-sdl3-diligent-physx-imgui-sdl3audio` ✅

## Open Questions
- Should point-shadow rendering be default-on in rewrite or rollout behind explicit config until closeout evidence is complete?
- Keep hard cap at 2 shadow-casting point lights for P0, or expose bounded config now?
- Should local-light directional-shadow lift be in KS2 or deferred to KS4 tuning once baseline parity is confirmed?

## Handoff Checklist
- [ ] Slice boundary respected (`KS1`..`KS4`)
- [ ] Required build/runtime evidence recorded
- [ ] Regression checks for directional `gpu_default` path recorded
- [ ] `ASSIGNMENTS.md` updated
- [ ] Risks/open questions updated
