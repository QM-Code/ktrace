# KARMA Lighting + Shadow Parity

## Project Snapshot
- Current owner: `specialist-renderer-csm-p0s1`
- Status: `priority/in progress (P0-S1 directional CSM + P0-S2 compare-sampler intake complete; ready for P0-S3 point-shadow GPU generation)`
- Upstream snapshot: `KARMA-REPO@905b63b`
- Rewrite snapshot: `m-rewrite@7ee717f8d`
- Immediate next task: execute `P0-S3` point-shadow GPU generation path intake.
- Validation gate: one assigned runtime-select renderer profile (`bgfx,diligent`), sandbox proof recipes, runtime smoke across renderer overrides, and docs lint must pass before slice acceptance.

## Mission
Implement every lighting/shadow technique that is actively used in KARMA demo paths and still missing (or only partially implemented) in `m-rewrite`, prove each in sandbox first, then wire the proven stack into `bz3` runtime.

## Intake Stance (Locked)
1. For this track, default to algorithm/flow-first intake from `KARMA-REPO`: when KARMA already has a proven lighting/shadow implementation, port that behavior directly.
2. Adapt imported logic to rewrite contracts, naming, and generic-backend structure; do not mirror upstream file layout.
3. Do not redesign algorithms first when a KARMA implementation exists and is compatible with rewrite contracts.
4. If a KARMA algorithm/flow cannot be applied cleanly in rewrite, stop and escalate with a compatibility note; do not silently substitute a different approach.
5. Any intentional divergence from KARMA algorithm/flow requires explicit rationale plus validation evidence in the handoff.

## Foundation References
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/policy/rewrite-invariants.md`
- `docs/foundation/governance/overseer-playbook.md`
- `docs/archive/renderer-shadow-hardening-superseded-2026-02-17.md` (source of carry-over unfinished items)
- `docs/archive/renderer-parity-retired-2026-02-17.md` (historical parity/VQ ledger; retired)

## Why This Is Separate
- Shadow hardening moved from isolated bug-fix mode into a full capability-parity program.
- We now need one canonical agent start point that covers: KARMA intake gaps, sandbox proof policy, and final bz3 integration sequencing.
- This document supersedes `renderer-shadow-hardening.md` as the active intake/implementation entry point.
- This document now also carries the remaining actionable concerns from retired `renderer-parity.md` so active renderer/shadow execution stays in one place.

## Renderer-Parity Retirement Intake (2026-02-17)
- Retired file: `docs/projects/renderer-parity.md` -> `docs/archive/renderer-parity-retired-2026-02-17.md`.
- Active carry-over concern from the retired track:
  - VQ4 deterministic visual regression guardrails (`assertions/metrics + wrapper/docs alignment`).
- Carry-over closure accepted by operator:
  - locked `gpu_default` contact-edge visual closeout is accepted; no immediate follow-up adjustment is queued unless regression evidence appears.
- VQ4 strategy lock for this track:
  - primary gate uses deterministic numeric/trace-derived proxies,
  - screenshot evidence remains supplemental/operator evidence (not sole CI gate), especially while Diligent non-interactive capture remains environment-limited.

## Owned Paths
- `docs/projects/karma-lighting-shadow-parity.md`
- `src/engine/renderer/backends/directional_shadow_internal.hpp`
- `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
- `src/engine/renderer/backends/diligent/backend_diligent.cpp`
- `data/bgfx/shaders/mesh/fs_mesh.sc`
- `src/engine/renderer/tests/renderer_shadow_sandbox.cpp`
- `src/engine/renderer/render_system.cpp`
- `include/karma/renderer/backend.hpp`
- `include/karma/renderer/device.hpp`
- `src/engine/app/engine_app.cpp`
- `src/game/client/runtime.cpp`
- `docs/projects/ASSIGNMENTS.md`

Read-only comparison root:
- `../KARMA-REPO`

## Interface Boundaries
- Inputs consumed:
  - KARMA behavioral reference from `KARMA-REPO` renderer/app/demo paths.
  - Existing rewrite renderer contracts and runtime config surface.
- Outputs exposed:
  - backend-parity lighting/shadow behavior in rewrite sandbox.
  - runtime-wired controls needed for deterministic bz3 verification.
- Coordinate before changing:
  - `docs/foundation/governance/testing-ci-governance.md`
  - `docs/archive/renderer-parity-retired-2026-02-17.md` (historical thresholds/rubrics)
  - `docs/foundation/architecture/core-engine-contracts.md`

## Non-Goals
- Do not clone KARMA file layout or architecture verbatim.
- Do not replace a proven KARMA lighting/shadow algorithm with a rewrite-specific variant without documented rationale and evidence.
- Do not expand into unrelated gameplay/network/UI migration work.
- Do not land unproven shadow/lighting changes directly into bz3 runtime without sandbox proof.

## Sweep Result: KARMA Techniques Missing In m-rewrite

Legend:
- `Missing`: not implemented in rewrite.
- `Partial`: some plumbing exists, but behavior is not at KARMA parity.
- `Landed`: slice parity objective is accepted for this track; regression watch continues.

| Area | KARMA-REPO (active demo path) | m-rewrite state | Gap |
|---|---|---|---|
| Directional shadow topology | 4-cascade CSM array with split logic + transition blending (`include/karma/renderer/backends/diligent/backend.hpp`, `src/renderer/backends/diligent/backend_render.cpp`, `src/renderer/backends/diligent/backend_init.cpp`) | 4-cascade CSM atlas + metadata wired in rewrite shared internals and both backends (`src/engine/renderer/backends/directional_shadow_internal.hpp`, `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`, `src/engine/renderer/backends/diligent/backend_diligent.cpp`, `data/bgfx/shaders/mesh/fs_mesh.sc`) | `Partial` (topology/stability landed; downstream quality/perf work remains) |
| CSM stability policy | Texel-snapped cascade fit + cached matrices/splits + camera/light threshold invalidation (`src/renderer/backends/diligent/backend_render.cpp`) | Texel-snapped per-cascade fit + lambda splits + transition blending now implemented; single-map fallback retained for stabilization (`src/engine/renderer/backends/directional_shadow_internal.hpp`) | `Partial` (slice complete; later quality/parity deltas tracked in downstream slices) |
| Directional shadow sampling | Hardware compare sampling (`SamplerComparisonState`, `SampleCmpLevelZero`) + optional PCF loop (`src/renderer/backends/diligent/backend_init.cpp`) | Hardware compare path landed in BGFX and Diligent shadow sampling flows (sampler compare state + PCF loops maintained) (`data/bgfx/shaders/mesh/fs_mesh.sc`, `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`, `src/engine/renderer/backends/diligent/backend_diligent.cpp`) | `Landed` (`P0-S2`) |
| Point shadow sampling | Hardware compare sampling for point shadows (`SampleCmpLevelZero` on point map) | Hardware compare path landed for point shadow map sampling in both backends, including compare-capable texture/sampler setup (`data/bgfx/shaders/mesh/fs_mesh.sc`, `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`, `src/engine/renderer/backends/diligent/backend_diligent.cpp`) | `Landed` (`P0-S2`) |
| Rasterizer depth/slope bias usage | Shadow raster state consumes `shadow_raster_depth_bias` + `shadow_raster_slope_bias` (`src/renderer/backends/diligent/backend_init.cpp`) | Bias values are plumbed into semantics/uniforms, but not applied as rasterizer state in rewrite backends | `Partial` |
| Point-shadow generation path | GPU depth rendering per face with per-face DSVs + dirty-face scheduling (`include/karma/renderer/backends/diligent/backend.hpp`, `src/renderer/backends/diligent/backend_render.cpp`) | CPU rasterized atlas build (`BuildPointShadowMap`) then uploaded each update cycle (`src/engine/renderer/backends/directional_shadow_internal.hpp`, backend `BuildPointShadowMap` call sites) | `Missing` |
| Local light scalability | Forward+ local light clustering (compute path + CPU fallback), runtime tile/max controls (`src/renderer/backends/diligent/backend_init.cpp`, `src/renderer/backends/diligent/backend_render.cpp`) | Fixed-size local light array (`kMaxLocalLights = 4`) in both backends; no Forward+ path or controls | `Missing` |
| Environment lighting source | HDR environment-map pipeline: equirect -> cubemap, irradiance, prefilter, BRDF LUT, skybox render (`src/renderer/backends/diligent/backend_render.cpp`, `backend_mesh.cpp`) | Hemispherical sky/ground ambient approximation only; no env map ingestion/prefilter/BRDF LUT pipeline (`src/engine/renderer/backends/environment_lighting_internal.hpp`) | `Missing` |
| PBR + IBL shading | Shader path uses full material + IBL textures and tone mapping (`src/renderer/backends/diligent/backend_init.cpp`) | Simplified shading path; no KARMA-equivalent IBL integration | `Missing` |
| Exposure control | Runtime `setExposure` API wired from EngineConfig + debug overlay (`include/karma/renderer/backend.hpp`, `src/app/engine_app.cpp`, `src/debug/debug_overlay.cpp`) | No exposure API in rewrite backend/device interface; no exposure control plumbing | `Missing` |
| Renderer runtime control plane | `setGenerateMips`, `setEnvironmentMap`, `setAnisotropy`, `setForwardPlusSettings`, `setShadowSettings`, `setPointShadowSettings`, `setLocalLightingSettings`, `setExposure` are backend/device contracts | Rewrite interface only exposes camera, directional light, local lights, environment-lighting struct (`include/karma/renderer/backend.hpp`, `include/karma/renderer/device.hpp`) | `Missing` |
| Engine config plumbing for texture filtering | KARMA app applies anisotropy/mip flags into renderer backend on startup (`src/app/engine_app.cpp`) | Rewrite `EngineConfig` has anisotropy/mip fields, but runtime wiring into renderer APIs is absent | `Missing` |
| Runtime tuning/debug loop | KARMA debug overlay includes live shadow/local-light/forward+/exposure controls (`src/debug/debug_overlay.cpp`) | No rewrite runtime debug panel for these controls (CLI/sandbox only) | `Missing` |

## Already Landed In m-rewrite (Keep Green)
- GPU directional shadow pass (`gpu_default`) in BGFX and Diligent.
- Directional 4-cascade CSM metadata path (lambda splits, cascade blend, texel-snapped fit) with explicit single-map fallback mode during stabilization.
- Shared shadow bias semantics and config keys (constant/receiver/normal/raster fields).
- Multi-point shadow selection with dirty-face scheduling and face-budget control.
- Local light shadow-lift parameters and AO-local-light modulation knobs.
- Sandbox support for multi-point-light motion, pause/resume (`space`), and diagnostics.

## Normalized Carry-Over Items (from renderer-shadow-hardening.md)
1. Contact-edge visual closeout is accepted on locked `gpu_default` defaults (`2026-02-17`); reopen only on explicit regression evidence.
2. Low-frequency blockiness/aliasing remains a quality risk and must be measured after compare-sampler/CSM intake.
3. Diligent non-interactive screenshot capture remains environment-blocked (`VK_ERROR_INITIALIZATION_FAILED` on X11 in headless capture); operator desktop evidence remains required for screenshot-specific proof.
4. Regression watch stays active for prior distance-dropout behavior; every slice must include an explicit distance-persistence check.
5. Runtime debug UI timing question is now explicit: defer full panel until core parity slices land, then add bounded panel for maintainability.
6. Cascade-count policy decision is now attached to CSM slice acceptance (`fixed 4 first`, optional configurability only after parity proof).
7. VQ4 deterministic visual regression guardrails are now tracked here (not in retired `renderer-parity.md`).

## Execution Plan

### P0-S0: Baseline Lock + Regression Harness
- Lock current known-good sandbox recipe and traces as baseline.
- Keep current face-budget behavior documented:
  - dynamic scenes require budget proportional to active shadowed point lights (`faces = 6 * active_lights`).
- Acceptance:
  - baseline screenshots/traces for BGFX + Diligent captured and linked.
  - no regressions against current “working” sandbox command family.

### P0-S0b: VQ4 Deterministic Visual Regression Guardrails (carry-over)
- Add one deterministic visual-regression guard execution path for BGFX + Diligent that is scriptable/headless-friendly and uses explicit build-dir inputs.
- Encode pass/fail checks with numeric/trace proxies tied to accepted runtime expectations (for example active `gpu_default` path, no unintended fallback tokens, and bounded shadow diagnostics).
- Align wrapper/docs/CI posture in the same slice (or document explicit standalone guard posture when wrapper integration is intentionally deferred).
- Acceptance:
  - guard command(s) are copy-pastable and deterministic,
  - pass/fail conditions are explicit and documented in this file,
  - testing governance docs are updated in the same handoff.

### P0-S1: Directional CSM Intake (KARMA parity)
- Add 4-cascade directional shadow topology and per-cascade metadata.
- Add split policy (lambda), cascade transition blending, and texel-snapped cascade fit.
- Keep single-map fallback path behind an explicit mode while stabilizing.
- Acceptance:
  - moving-camera sandbox shows no cascade seam pops.
  - near/far shadow stability better than current single-map baseline.

### P0-S1 Session Update (2026-02-17)
- Implemented shared directional CSM set (`4` cascades) with:
  - split-lambda partitioning,
  - texel-snapped cascade center fit,
  - transition blending metadata,
  - atlas tiling metadata + per-cascade UV projection.
- Integrated cascade rendering/sampling in BGFX and Diligent backend paths while retaining explicit single-map fallback conversion for stabilization.
- Validation status this session:
  - `./abuild.py -c -d build-a5 -b bgfx,diligent`: pass.
  - `./build-a5/src/engine/renderer_shadow_sandbox --backend-render bgfx ...`: pass.
  - `./build-a5/src/engine/renderer_shadow_sandbox --backend-render diligent ...`: passes when run with `SDL_VIDEODRIVER=wayland` in this environment.
  - runtime smoke canonical recipe updated to current CLI (`--data-dir`, `--user-config`, `--trace`); both backend runs complete to expected timeout (`EXIT:124`) with sustained render traces.

### P0-S2: Compare-Sampler Shadow Sampling
- Replace manual `step`-based directional/point shadow compare with hardware compare sampling path (per backend capability).
- Keep bounded PCF radius controls.
- Acceptance:
  - visible reduction in blocky penumbra/seam artifacts in both backends.
  - no detached-shadow regressions in moving-point-light sandbox.

### P0-S2 Session Update (2026-02-17)
- Ported KARMA compare-sampler flow into rewrite backend seams:
  - BGFX shader path now uses compare samplers (`shadow2D`) for directional + point maps in base and PCF kernels.
  - BGFX shadow/point texture creation now sets compare-capable sampler flags and uses depth-compatible formats for compare sampling.
  - Diligent shader path now uses `SamplerComparisonState` + `SampleCmpLevelZero` for directional + point maps in base and PCF kernels.
  - Diligent immutable samplers for shadow maps now use comparison filtering (`LESS_EQUAL`) with clamp addressing.
- Validation status this session:
  - `./abuild.py -c -d build-a5 -b bgfx,diligent`: pass.
  - `./build-a5/src/engine/renderer_shadow_sandbox --backend-render bgfx ...`: pass.
  - `SDL_VIDEODRIVER=wayland ./build-a5/src/engine/renderer_shadow_sandbox --backend-render diligent ...`: pass.
  - `timeout -k 2s 20s ./build-a5/bz3 --backend-render bgfx ...`: expected timeout pass (`EXIT:124`) with sustained render traces.
  - `SDL_VIDEODRIVER=wayland timeout -k 2s 20s ./build-a5/bz3 --backend-render diligent ...`: expected timeout pass (`EXIT:124`) with sustained render traces.
  - `./docs/scripts/lint-project-docs.sh`: pass.
- Operator visual verification during run:
  - BGFX: shadows visible again after compare-sampler state fix.
  - Diligent: shadows visible and stable.

### P0-S3: Point Shadow GPU Generation Path
- Replace CPU `BuildPointShadowMap` raster path with GPU face rendering/update scheduling.
- Keep dirty-face scheduler + budget policy.
- Acceptance:
  - measured frame-time improvement at equivalent quality over CPU atlas path.
  - shadow placement remains stable under motion (lights + casters moving).

### P0-S4: Renderer Control Plane Parity
- Introduce rewrite-owned equivalents for missing runtime renderer controls:
  - environment map, anisotropy, mip generation, shadow settings, point shadow settings, local-light settings, forward+ settings, exposure.
- Wire from runtime config and engine app startup.
- Acceptance:
  - no dead knobs: every exposed config field is proven active via trace or functional change.

### P1-S1: Environment/IBL Intake
- Add HDR env-map ingestion + irradiance/prefilter/BRDF LUT + skybox path.
- Bind into material lighting path in both backends (or document bounded backend staging if unavoidable).
- Acceptance:
  - sandbox/world checkpoints show expected image-based ambient/specular response.

### P1-S2: Forward+ Local-Light Scalability
- Add clustered/Forward+ local-light path (with bounded fallback).
- Remove `kMaxLocalLights=4` practical ceiling in primary path.
- Acceptance:
  - local-light stress scenes retain stable frame-time and visual parity.

### P1-S3: Runtime Debug Control Surface
- Add bounded rewrite debug panel for shadow/lighting parity controls and perf diagnostics.
- Acceptance:
  - operator can tune and verify key knobs in-runtime without CLI restarts.

### P2-S1: bz3 Runtime Wiring
- After sandbox parity proof, wire full lighting/shadow stack into bz3 runtime scene flow.
- Acceptance:
  - roaming bz3 checkpoints match sandbox-proven behavior for directional + point shadows and lighting quality/perf envelopes.

## Validation
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent

# Canonical sandbox parity recipes (backend-specific)
# Keep default UI/physics/audio/platform backends for this track.
./<build-dir>/src/engine/renderer_shadow_sandbox \
  --backend-render bgfx --duration-sec 30 --ground-tiles 1 --ground-extent 20 \
  --shadow-map-size 2048 --shadow-pcf 2 --shadow-strength 0.85 --shadow-execution-mode gpu_default \
  --point-shadow-lights 2 --point-shadow-map-size 256 --point-shadow-max-lights 2 \
  --point-shadow-light-range 14 --point-shadow-light-intensity 2 \
  --point-shadow-scene-motion --point-shadow-motion-speed 0.9

./<build-dir>/src/engine/renderer_shadow_sandbox \
  --backend-render diligent --duration-sec 30 --ground-tiles 1 --ground-extent 20 \
  --shadow-map-size 2048 --shadow-pcf 2 --shadow-strength 0.85 --shadow-execution-mode gpu_default \
  --point-shadow-lights 2 --point-shadow-map-size 256 --point-shadow-max-lights 2 \
  --point-shadow-light-range 14 --point-shadow-light-intensity 2 \
  --point-shadow-scene-motion --point-shadow-motion-speed 0.9
# If Diligent surface init fails on Wayland/X11 autodetect, force Wayland:
# SDL_VIDEODRIVER=wayland ./<build-dir>/src/engine/renderer_shadow_sandbox --backend-render diligent ...

# Runtime smoke
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render bgfx --data-dir ./data --strict-config=true --user-config data/client/config.json --trace engine.sim,render.system,render.bgfx
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render diligent --data-dir ./data --strict-config=true --user-config data/client/config.json --trace engine.sim,render.system,render.diligent

./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `render.system`
- `render.bgfx`
- `render.diligent`
- `engine.sim`
- `render.mesh`

## First Session Checklist
1. Read this file + `docs/archive/renderer-shadow-hardening-superseded-2026-02-17.md` carry-over section only.
2. Re-run canonical sandbox command for both backends and capture baseline evidence.
3. Pick exactly one active slice from this plan and scope it narrowly.
4. Land code + validation + doc updates in same handoff.
5. Update `docs/projects/ASSIGNMENTS.md` status/next task.

## Open Questions
- For CSM, should rewrite lock to 4 cascades first (KARMA parity) or expose cascade count immediately?
- For `P0-S3`, should CPU point-shadow atlas generation remain as an explicit fallback mode during GPU-generation stabilization?
- At what slice do we require world-asset parity captures in addition to synthetic sandbox captures?
- For VQ4 guardrails, should enforcement be integrated into an existing wrapper immediately or shipped first as a required standalone guard command with CI follow-up?

## Handoff Checklist
- [x] Active slice completed
- [x] Builds/tests run and summarized
- [x] Sandbox evidence captured for both backends
- [x] Runtime smoke completed (or blocker documented)
- [x] This file updated
- [x] `docs/projects/ASSIGNMENTS.md` updated
- [x] Remaining risks/open questions listed
