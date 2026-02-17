# KARMA Shadow Intake P0 (Integration Track, Completed 2026-02-16)

## Project Snapshot
- Current owner: `overseer`
- Status: `completed (KS0 cache-invalidation + KS1 contract/config scaffolding + KS2.1 directional GPU shadow hardening + KS2.2 local-light source contract/shading bridge + KS2.3 bounded point-shadow generation/sampling + KS3 moving-caster/light dirty-face invalidation + KS4.1 point-shadow budget/map-size sweep + defaults lock + KS4.2 closeout run + operator visual verdict accepted)`
- Immediate next task: archive snapshot complete; no active execution task.
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
- `m-rewrite/docs/archive/karma-shadow-intake-p0-completed-2026-02-16.md` (archived snapshot; active project path retired)
- `m-rewrite/docs/projects/ASSIGNMENTS.md`
- `m-rewrite/src/engine/renderer/backends/internal/directional_shadow.hpp`
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
./abuild.py -c -d <build-dir>
./scripts/run-renderer-shadow-sandbox.sh 20 16 20
./scripts/run-point-shadow-visual-closeout.sh all 20 1 20 0.9
./scripts/run-point-shadow-budget-sweep.sh 8 1024 1,2,4 2 1 20 0.9
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render bgfx --data-dir ./data --strict-config=true --user-config data/client/config.json --trace engine.sim,render.system,render.bgfx,render.mesh
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render diligent --data-dir ./data --strict-config=true --user-config data/client/config.json --trace engine.sim,render.system,render.diligent,render.mesh
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `render.system`
- `render.bgfx`
- `render.diligent`
- `render.mesh`
- `engine.sim`

## Build Dirs (Assigned)
- `<build-dir>`

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
- `2026-02-16`: `KS2.3` bounded point-shadow generation/sampling landed for selected local lights in both backends:
  - BGFX + Diligent now build CPU point-shadow atlases from selected shadow-casting point lights via rewrite-owned `BuildPointShadowMap(...)`, upload atlas textures, and bind per-light shadow-slot/matrix contracts to forward shaders.
  - local-light shading now applies point-shadow visibility per light (face selection, projected UV/depth compare, bounded 0/1 PCF) with rewrite-configured point shadow tuning (`point_*_bias*` fields).
  - runtime trace token upgraded from unimplemented posture to active/fallback diagnostics (`point_shadow_active`, `point_shadow_no_shadow_lights`, `point_shadow_no_casters`, `point_shadow_map_build_failed`, `point_shadow_upload_failed`, etc.).
  - bounded light selection + update cadence (`update_every_frames`) are active and now feed KS3 incremental refresh policy.
- `2026-02-16`: `KS3` moving-caster/light dirty-face invalidation + bounded face-budget scheduling landed in both backends:
  - shared point-shadow helper now supports incremental updates (`BuildPointShadowMap(..., previous_map, face_update_mask)`), light selection reuse (`SelectPointShadowLightIndices`), and layout-compatibility checks (`IsPointShadowMapLayoutCompatible`).
  - BGFX + Diligent now track point-shadow slot/source/light state plus moving-caster influence and only refresh dirty faces per frame under `point_faces_per_frame_budget`.
  - both backends emit deterministic trace evidence for refresh causality and budget pressure (`point shadow refresh ... dirtyFaces/updatedFaces/budget/fullRefresh/structural/movedCasters`) with status reasons (`point_shadow_faces_updated`, `point_shadow_waiting_budget`, `point_shadow_cache_hit_clean`, etc.).
  - contract-test coverage now includes incremental point-shadow behavior checks (`RunPointShadowIncrementalUpdateChecks`) to verify layout compatibility and selected-face-only updates against a cached atlas.
- `2026-02-16`: `KS4.1` tuning harness + bounded budget sweep landed:
  - `renderer_shadow_sandbox` now supports explicit point-shadow scene controls (`--point-shadow-lights`, `--point-shadow-map-size`, `--point-shadow-face-budget`, `--point-shadow-scene-motion`, etc.) and can animate a caster + point lights to exercise moving-caster/light invalidation in real runtime.
  - added `scripts/run-point-shadow-budget-sweep.sh` to run repeatable BGFX + Diligent budget sweeps with trace capture and summary CSV output.
  - bounded sweep evidence (`/tmp/point-shadow-budget-sweep-20260216T140724Z/summary.csv`, `duration=8s`, `pointMapSize=1024`, `pointLights=2`, `budgets=1/2/4`) produced:
    - BGFX: `budget=1 -> 4.25 FPS`, `budget=2 -> 3.49 FPS`, `budget=4 -> 3.90 FPS`
    - Diligent: `budget=1 -> 3.82 FPS`, `budget=2 -> 3.56 FPS`, `budget=4 -> 3.56 FPS`
  - policy lock for P0 defaults: keep `point_faces_per_frame_budget=2` (retains higher per-frame refresh cadence than `budget=1` without introducing clear cross-backend perf wins from `budget=4` in the bounded sweep window).
  - bounded map-size checkpoint landed:
    - `pointMapSize=2048` with `pointLights=2` fails deterministically in both backends with `reason=point_shadow_map_build_failed` (`/tmp/point-shadow-budget-sweep-20260216T141704Z/summary.csv`), matching the atlas-size guard in `directional_shadow.hpp` (`kMaxPointShadowPixels`).
    - `pointMapSize=2048` with `pointLights=1` is functional (`reason=point_shadow_faces_updated`) but substantially slower (`BGFX 2.20 FPS`, `Diligent 1.92 FPS`) in this bounded stress window (`/tmp/point-shadow-budget-sweep-20260216T141732Z/summary.csv`).
  - policy lock for P0 defaults: keep `point_map_size=1024` and `point_max_shadow_lights=2` as the viable bounded default pair.
- `2026-02-16`: `KS4.2` operator-closeout runner landed (visual-signoff assist):
  - added `scripts/run-point-shadow-visual-closeout.sh` to run locked-default checkpoint windows (`bgfx` then `diligent`) with trace/log capture and automatic Diligent X11->Wayland fallback retry.
  - dry-run evidence (`./scripts/run-point-shadow-visual-closeout.sh all 6 1 20 0.9`) produced active status in both backends (`reason=point_shadow_faces_updated`) with logs under `/tmp/point-shadow-visual-closeout-20260216T142516Z/`.
  - operator-closeout run executed with review-grade duration (`./scripts/run-point-shadow-visual-closeout.sh all 20 1 20 0.9`):
    - BGFX: `status=point_shadow_faces_updated`, log `/tmp/point-shadow-visual-closeout-20260216T142607Z/visual-bgfx.log`
    - Diligent: `status=point_shadow_faces_updated` via Wayland fallback, log `/tmp/point-shadow-visual-closeout-20260216T142607Z/visual-diligent-wayland-retry.log`
    - closeout bundle root: `/tmp/point-shadow-visual-closeout-20260216T142607Z/`
- `2026-02-16`: Operator visual verdict accepted from interactive sandbox validation:
  - detached/phantom point-shadow artifacts were resolved by projection-side behind-face rejection in point-shadow projection and bounded face-budget scheduling consistency.
  - accepted interactive point-shadow posture in sandbox: `shadowExecutionMode=gpu_default`, `pointMapSize=256`, `pointShadowLights=2`, and effective `pointFacesPerFrameBudget=12` (`6 * active shadowed lights`).
  - user-confirmed quality/performance observation: `pointMapSize=1024` incurs a large frame-time cost; `pointMapSize=256` is the accepted default for interactive point-shadow iteration.
- `2026-02-16`: Project closed and prepared for archive under `docs/archive/`.
- Validation evidence (`m-rewrite/`):
  - `./abuild.py -c -d <build-dir>` ✅
  - `./<build-dir>/src/engine/directional_shadow_contract_test` ✅
  - `./scripts/run-point-shadow-budget-sweep.sh 8 1024 1,2,4 2 1 20 0.9` (summary at `/tmp/point-shadow-budget-sweep-20260216T140724Z/summary.csv`; trace confirms active point-shadow updates under animated point-light scene) ✅
  - `./scripts/run-point-shadow-budget-sweep.sh 8 1024 2 2 1 20 0.9` (summary at `/tmp/point-shadow-budget-sweep-20260216T141641Z/summary.csv`; locked-budget replay) ✅
  - `./scripts/run-point-shadow-budget-sweep.sh 8 2048 2 2 1 20 0.9` (summary at `/tmp/point-shadow-budget-sweep-20260216T141704Z/summary.csv`; confirms `point_shadow_map_build_failed` at `2048 x 2 lights`) ✅
  - `./scripts/run-point-shadow-budget-sweep.sh 8 2048 2 1 1 20 0.9` (summary at `/tmp/point-shadow-budget-sweep-20260216T141732Z/summary.csv`; confirms `2048` viability only with `1` shadow light and high frame cost) ✅
  - `./scripts/run-point-shadow-visual-closeout.sh all 6 1 20 0.9` (logs at `/tmp/point-shadow-visual-closeout-20260216T142516Z/`; both backends reached `point_shadow_faces_updated`) ✅
  - `./scripts/run-point-shadow-visual-closeout.sh all 20 1 20 0.9` (logs at `/tmp/point-shadow-visual-closeout-20260216T142607Z/`; both backends reached `point_shadow_faces_updated`) ✅
  - `timeout -k 2s 20s ./<build-dir>/bz3 --backend-render bgfx --data-dir ./data --strict-config=true --user-config data/client/config.json --trace engine.sim,render.system,render.bgfx,render.mesh` (timeout expected; traces confirmed `point shadow status ... reason=point_shadow_no_shadow_lights` and `point shadow refresh ... dirtyFaces=0 updatedFaces=0 budget=2`) ✅
  - `timeout -k 2s 20s ./<build-dir>/bz3 --backend-render diligent --data-dir ./data --strict-config=true --user-config data/client/config.json --trace engine.sim,render.system,render.diligent,render.mesh` (timeout expected; traces confirmed `point shadow status ... reason=point_shadow_no_shadow_lights` and `point shadow refresh ... dirtyFaces=0 updatedFaces=0 budget=2`) ✅

## Open Questions (Resolved)
- Keep rollout behind explicit config for P0 runtime behavior; do not force point-shadow default-on globally.
- Keep default shadow-casting point-light cap at `2` for P0 quality/perf posture while preserving bounded config exposure (`0..4`) for explicit tuning.
- Use face-budget policy `6 * active shadowed point lights` (for example `2 -> 12`, `3 -> 18`) to avoid stale/detached motion artifacts under animated lights/casters.

## Handoff Checklist
- [x] Slice boundary respected (`KS1`..`KS4`)
- [x] Required build/runtime evidence recorded
- [x] Regression checks for directional `gpu_default` path recorded
- [x] `ASSIGNMENTS.md` updated
- [x] Risks/open questions updated
