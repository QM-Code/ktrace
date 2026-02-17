# Renderer Parity (Retired)

## Project Snapshot
- Current owner: `retired/archive`
- Status: `retired/superseded (active renderer/shadow execution consolidated into docs/projects/karma-lighting-shadow-parity.md on 2026-02-17)`
- Immediate next task: do not start new work here; execute active slices in `docs/projects/karma-lighting-shadow-parity.md`.
- Validation gate: historical ledger retained for reference; active validation policy is in `docs/projects/karma-lighting-shadow-parity.md` and foundation governance docs.

## Mission
Expand renderer capability toward BGFX/Diligent parity behind stable engine contracts, then convert those capability gains into visible runtime quality improvements (shadow visibility + stable distance texture quality).

## Priority Directive (2026-02-11)
- Renderer capability integration is the top execution priority.
- R25 P0 continuity slice is complete; VQ1 diagnostics baseline and VQ2 texture minification closeout are accepted.
- Visual-quality follow-up (VQ3-VQ4) is medium-high P1 and should proceed in sequence after VQ2 acceptance.
- Prioritize deterministic visual improvements over speculative renderer feature breadth.
- This work is explicitly prioritized ahead of incremental audio/content-mount follow-up slices.
- Port capability and behavior from KARMA-REPO; do not mirror KARMA-REPO backend file organization.

## R26 Performance/GPU Offload Program (2026-02-14)
Strategic alignment:
- Track labels in this program intentionally span: `shared unblocker`, `KARMA intake`, and `m-dev parity`.
- This program expands renderer parity work; active VQ3/KARMA shadow intake execution is tracked in `docs/projects/karma-lighting-shadow-parity.md`.

Program mission additions:
1. Bring BGFX and Diligent to behavior/perf parity for shadow/lighting paths that currently diverge.
2. Close the CPU/GPU capability gap between `KARMA-REPO` and `m-rewrite` for shadow generation/sampling and closely related lighting work.
3. Mine `m-dev` rendering behavior for proven high-FPS techniques and intake candidates that fit rewrite contracts.
4. Establish policy that performance-sensitive renderer techniques ship with config toggles in `data/client/config.json` (or explicit rationale when intentionally fixed).

Execution slices:
1. `R26-A` (`shared unblocker`): Baseline/perf matrix capture
   - Capture repeatable roaming-mode metrics for BGFX + Diligent with shadows `on/off`.
   - Record `engine.sim` perf1s stats + renderer traces + key frame-time spikes.
   - Acceptance: one committed evidence summary block in this file with backend deltas and top hotspot suspects.
2. `R26-B` (`KARMA intake`): GPU shadow parity intake
   - Move rewrite shadow-map generation/sampling off CPU path toward GPU pass architecture, preserving rewrite-owned contracts.
   - Minimum expectation: remove per-frame CPU shadow rasterization as the default path in both active renderer backends.
   - Acceptance: trace/runtime evidence that shadow work moved to GPU-managed render pass/resources and FPS regression gates improve versus R26-A baseline.
3. `R26-C` (`m-dev parity`): m-dev renderer technique intake review
   - Inventory `m-dev` GPU-offloaded rendering techniques and compare against rewrite/KARMA posture.
   - Classify each candidate: `adopt now`, `adopt later`, or `reject` with rationale.
   - Acceptance: committed intake matrix in this file with concrete follow-up actions for adopt-now items.
4. `R26-D` (`shared unblocker`): Renderer config policy expansion
   - For each new/tuned performance-sensitive technique, add bounded config controls (with sane defaults and validation clamps).
   - Maintain backend-neutral naming under existing config structure.
   - Acceptance: config keys documented in this file + wired in runtime + included in operator test recipes.

R26 guardrails:
- Keep behavior/contract parity as the objective; do not mirror KARMA source layout.
- Do not land backend-specific gameplay-facing API changes.
- Keep each slice independently measurable with before/after perf evidence.

## R26-A Baseline Matrix (Completed 2026-02-14)
Strategic alignment:
- Track label: `shared unblocker`.
- Scope: roaming mode (no tank), BGFX + Diligent, shadows `OFF/ON`.

Capture notes:
- Runtime logs captured in `/tmp/r26a-renderer-baseline-20260214T064426Z/`.
- `bz3` backend override flags are parsed by shared client CLI (`--backend-render`, `--backend-ui`) when multiple corresponding backends are compiled; this track now runs one assigned runtime-select build dir plus explicit CLI backend overrides for deterministic captures.
- The first BGFX-shadows-ON run had one anomalous tail-window perf sample (`avg_fps=0.3`, `max_frame_ms=18731.69`) in `bgfx-shadow-on.log`; a same-command rerun (`bgfx-shadow-on-rerun.log`) was recorded and used for the steady-state matrix below.

R26-A commands (run from `m-rewrite/`):
```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent

timeout 25s ./<build-dir>/bz3 --backend-render bgfx --strict-config=true --config /tmp/r26a-renderer-baseline-20260214T064426Z/user-shadow-off.json -v -t engine.sim,render.bgfx
timeout 25s ./<build-dir>/bz3 --backend-render bgfx --strict-config=true --config /tmp/r26a-renderer-baseline-20260214T064426Z/user-shadow-on.json -v -t engine.sim,render.bgfx
timeout 25s ./<build-dir>/bz3 --backend-render diligent --strict-config=true --config /tmp/r26a-renderer-baseline-20260214T064426Z/user-shadow-off.json -v -t engine.sim,render.diligent
timeout 25s ./<build-dir>/bz3 --backend-render diligent --strict-config=true --config /tmp/r26a-renderer-baseline-20260214T064426Z/user-shadow-on.json -v -t engine.sim,render.diligent

# anomaly-check rerun for BGFX shadows ON
timeout 25s ./<build-dir>/bz3 --backend-render bgfx --strict-config=true --config /tmp/r26a-renderer-baseline-20260214T064426Z/user-shadow-on.json -v -t engine.sim,render.bgfx
```

Matrix method:
- Metric source: `engine.sim` `perf1s` lines.
- Aggregation: steady-state mean after dropping the first `perf1s` sample (`steady_n=22` per scenario).

| Backend | Shadows | steady mean fps | steady mean frame ms | steady mean avg_steps | steady worst `max_frame_ms` | Log |
|---|---|---:|---:|---:|---:|---|
| BGFX | OFF | 55.64 | 17.98 | 1.08 | 31.51 | `/tmp/r26a-renderer-baseline-20260214T064426Z/bgfx-shadow-off.log` |
| BGFX | ON | 29.30 | 34.24 | 2.05 | 78.50 | `/tmp/r26a-renderer-baseline-20260214T064426Z/bgfx-shadow-on-rerun.log` |
| Diligent | OFF | 40.70 | 24.60 | 1.47 | 60.50 | `/tmp/r26a-renderer-baseline-20260214T064426Z/diligent-shadow-off.log` |
| Diligent | ON | 37.49 | 26.68 | 1.60 | 49.00 | `/tmp/r26a-renderer-baseline-20260214T064426Z/diligent-shadow-on.log` |

R26-A delta summary:
- BGFX shadow cost: `-47.3%` FPS (`55.64 -> 29.30`), `+90.5%` frame time (`17.98ms -> 34.24ms`), avg sim steps nearly doubled (`1.08 -> 2.05`).
- Diligent shadow cost: `-7.9%` FPS (`40.70 -> 37.49`), `+8.5%` frame time (`24.60ms -> 26.68ms`).
- Backend parity flips by shadow state: BGFX leads Diligent with shadows OFF (`+36.7%` FPS) but trails with shadows ON (`-21.8%` FPS).

Top CPU hotspot suspects (file-level):
1. Shared CPU shadow map build + sampling internals:
   - `src/engine/renderer/backends/internal/directional_shadow.hpp` (`BuildDirectionalShadowMap`, `SampleDirectionalShadowVisibility`).
   - Why: both backends call into this path (`backend_bgfx.cpp:1290`, `backend_diligent.cpp:505`) and shadow-enable immediately increases frame cost.
2. BGFX per-frame shadow map upload/build pressure:
   - `src/engine/renderer/backends/bgfx/backend_bgfx.cpp` (`shadow_update_every_frames_`, `BuildDirectionalShadowMap`, shadow-map upload/trace at `1473`).
   - Evidence: in BGFX+ON rerun, trace reported `688` `shadow map layer=0 ... uploaded=1` entries in 25s; BGFX ON is the dominant perf cliff.
3. Diligent CPU shadow-summary/factor path still active:
   - `src/engine/renderer/backends/diligent/backend_diligent.cpp` (`BuildDirectionalShadowMap` + `shadow summary`).
   - Why: Diligent uses shared CPU map/sampling path and remains CPU-bound baseline even with shadows OFF.
4. Trace overhead is non-trivial but controlled across scenarios:
   - Per-frame `direct sampler frame` logs are high-volume in all runs; relative deltas still isolate shadow-path impact because channel policy was held constant.

R26-B implementation recommendation (ordered):
1. Add engine-owned shadow execution mode contract and config plumbing (`data/client/config.json`) with explicit modes: `cpu_reference` (existing) and `gpu_default` (new target), plus deterministic fallback.
2. Implement BGFX GPU shadow pass first:
   - depth-only shadow render target + shader sample path,
   - retain CPU reference path behind config for parity/regression.
3. Implement Diligent GPU shadow pass to parity with the same contract surface and matching acceptance gates.
4. Re-run the same R26-A matrix for before/after proof and keep BGFX-shadows-ON as the primary regression bar.

R26-B risks:
- Contract drift between backends if GPU pass logic diverges before shared semantics are finalized.
- Runtime fallback complexity (GPU path failure -> CPU path) can hide regressions unless trace reasons are explicit.
- Existing docs still include backend-override examples attached to build-specific binaries; normalize recipes so overrides are only shown where multi-backend binaries are used.

## R26-B Slice 1 Progress (In Progress, 2026-02-14)
Strategic alignment:
- Track label: `KARMA intake`.
- Scope: Diligent shadow correctness/perf parity pre-work before GPU default shadow pass.

Implemented code path updates:
1. Diligent moved to per-pixel shadow-map sampling path (parity with BGFX behavior) in `src/engine/renderer/backends/diligent/backend_diligent.cpp`.
2. Diligent shadow cadence/cache parity landed (`updateEveryFrames`, cached shadow map, rebuild/upload cadence) mirroring BGFX policy in `src/engine/renderer/backends/diligent/backend_diligent.cpp`.

Evidence logs:
- Runtime logs captured in `/tmp/r26b-shadow-cadence-20260214T082141Z/`.

Commands and outcomes (run from `m-rewrite/`):
```bash
# build gates
./abuild.py -c -d <build-dir> -b bgfx,diligent   # fail (missing layers include), then pass after fix
./abuild.py -c -d <build-dir> -b bgfx,diligent       # pass

# matrix captures (expected timeout exit 124)
timeout 25s ./<build-dir>/bz3 --backend-render bgfx --strict-config=true --config /tmp/r26b-shadow-cadence-20260214T082141Z/user-shadow-off.json -v -t engine.sim,render.bgfx
timeout 25s ./<build-dir>/bz3 --backend-render bgfx --strict-config=true --config /tmp/r26b-shadow-cadence-20260214T082141Z/user-shadow-on.json -v -t engine.sim,render.bgfx
timeout 25s ./<build-dir>/bz3 --backend-render diligent --strict-config=true --config /tmp/r26b-shadow-cadence-20260214T082141Z/user-shadow-off.json -v -t engine.sim,render.diligent
timeout 25s ./<build-dir>/bz3 --backend-render diligent --strict-config=true --config /tmp/r26b-shadow-cadence-20260214T082141Z/user-shadow-on.json -v -t engine.sim,render.diligent

# cadence A/B (shadows ON)
timeout 25s ./<build-dir>/bz3 --backend-render bgfx --strict-config=true --config /tmp/r26b-shadow-cadence-20260214T082141Z/user-shadow-on-every1.json -v -t engine.sim,render.bgfx
timeout 25s ./<build-dir>/bz3 --backend-render diligent --strict-config=true --config /tmp/r26b-shadow-cadence-20260214T082141Z/user-shadow-on-every1.json -v -t engine.sim,render.diligent
```

Matrix method:
- Metric source: `engine.sim` `perf1s` lines.
- Aggregation: steady-state mean after dropping first `perf1s` sample (`steady_n=22` per scenario).

| Backend | Shadows | steady mean fps | steady mean frame ms | steady mean avg_steps | steady worst `max_frame_ms` | Log |
|---|---|---:|---:|---:|---:|---|
| BGFX | OFF | 59.75 | 16.74 | 1.00 | 37.70 | `/tmp/r26b-shadow-cadence-20260214T082141Z/bgfx-shadow-off.log` |
| BGFX | ON | 34.81 | 28.73 | 1.72 | 38.90 | `/tmp/r26b-shadow-cadence-20260214T082141Z/bgfx-shadow-on.log` |
| Diligent | OFF | 59.02 | 16.95 | 1.02 | 38.84 | `/tmp/r26b-shadow-cadence-20260214T082141Z/diligent-shadow-off.log` |
| Diligent | ON | 30.75 | 32.52 | 1.95 | 38.70 | `/tmp/r26b-shadow-cadence-20260214T082141Z/diligent-shadow-on.log` |

Slice-1 deltas:
- OFF-state backend parity is now close (`59.75` vs `59.02`, Diligent `-1.2%` FPS).
- ON-state parity remains incomplete (Diligent `30.75` vs BGFX `34.81`, Diligent `-11.7%` FPS).
- Shadow tax in this run window remains large on both backends (BGFX `-41.7%`, Diligent `-47.9%`), consistent with CPU shadow-map generation still being the default path.

Cadence A/B (`updateEveryFrames=1` vs `2`, shadows ON):
- BGFX: `34.64` FPS (`every1`) -> `34.81` FPS (`every2`) (`+0.5%`).
- Diligent: `28.49` FPS (`every1`) -> `30.75` FPS (`every2`) (`+7.9%`).
- Interpretation: cadence control is active and materially beneficial in Diligent, but does not remove the core CPU shadow-map cost.

Top CPU hotspot suspects after slice 1 (file-level):
1. `src/engine/renderer/backends/internal/directional_shadow.hpp`
   - `BuildDirectionalShadowMap` remains the dominant ON-state cost center shared by BGFX and Diligent.
2. `src/engine/renderer/backends/diligent/backend_diligent.cpp`
   - Remaining per-frame shadow-map build/integration work (now cadence-gated) plus per-draw binding/submit overhead keeps ON-state behind BGFX.
3. `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
   - Same CPU shadow-map dependency still drives major ON-state tax even with update cadence.

R26-B implementation order recommendation (updated):
1. Add engine-owned shadow execution mode contract + config policy (`cpu_reference`, `gpu_default`) with explicit runtime trace reason when falling back.
2. Implement BGFX GPU depth shadow pass/resources first and keep CPU reference path gated.
3. Port same GPU-pass contract to Diligent with matching shader/resource semantics.
4. Re-run R26-A matrix recipe and keep this R26-B slice-1 matrix as intermediate regression evidence.

R26-B slice-2 risks:
- GPU resource lifecycle mismatches (resize/reload/device-loss) can cause backend-specific fallback churn unless fallback reasons are traced.
- CPU reference path drift can silently invalidate parity tests; keep one deterministic contract test/trace assertion path alive.
- If config policy is not wired first, operators cannot reliably force/verify fallback during triage.

R26-B slice-2 scaffolding landed (2026-02-14):
1. Added engine-owned shadow execution mode contract in `include/karma/renderer/types.hpp`:
   - modes: `cpu_reference`, `gpu_default`
   - parser/token helpers: `TryParseShadowExecutionMode`, `ShadowExecutionModeToken`
2. Added runtime config wiring in `src/game/client/main.cpp`:
   - reads `roamingMode.graphics.lighting.shadows.executionMode`
   - invalid values now warn and fall back deterministically.
3. Added explicit backend fallback reason traces (until GPU pass exists):
   - `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
   - `src/engine/renderer/backends/diligent/backend_diligent.cpp`
   - trace reason token: `gpu_shadow_pass_not_implemented`.
4. Added config policy defaults:
   - `data/client/config.json`
   - `data/client/config_tank_preview.json`
5. Added parser contract test coverage:
   - `src/engine/renderer/tests/directional_shadow_contract_test.cpp`.

Slice-2 scaffolding validation:
- `./abuild.py -c -d <build-dir> -b bgfx,diligent` -> pass.
- `./<build-dir>/src/engine/directional_shadow_contract_test` -> pass.
- GPU-mode fallback smoke (`executionMode=gpu_default`) logs in `/tmp/r26b-shadow-execmode-20260214T083325Z/` show:
  - BGFX fallback trace count: `1`
  - Diligent fallback trace count: `1`
  - bounded smoke exits: `137` (`timeout -k` hard-stop to avoid lingering process during unattended capture).

R26-B slice-3 BGFX GPU pass prototype landed (2026-02-14):
1. Added shared shadow-projection-only helper (no CPU rasterization):
   - `src/engine/renderer/backends/internal/directional_shadow.hpp` -> `BuildDirectionalShadowProjection(...)`.
2. Added BGFX shadow-depth shader pair + build wiring:
   - `data/bgfx/shaders/shadow/vs_shadow_depth.sc`
   - `data/bgfx/shaders/shadow/fs_shadow_depth.sc`
   - `data/bgfx/shaders/shadow/varying.def.sc`
   - `CMakeLists.txt` (`bz3_bgfx_shaders` now compiles `vk/shadow/*` bins).
3. Implemented BGFX GPU shadow pass path gated by `executionMode=gpu_default`:
   - `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
   - shadow view ordering (`shadow view=0`, main view=1)
   - R32F render-target shadow texture + framebuffer management
   - active-path trace: `gpu shadow pass size={} draws={}`.
4. CPU reference path remains intact and is still used for `executionMode=cpu_reference` or when GPU path is unavailable.

Slice-3 validation commands and outcomes (run from `m-rewrite/`):
```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent      # pass
./<build-dir>/src/engine/directional_shadow_contract_test      # pass

# BGFX CPU vs GPU mode compare (shadows ON)
timeout 25s ./<build-dir>/bz3 --backend-render bgfx --strict-config=true --config /tmp/r26b-bgfx-gpu-compare-20260214T173727Z/cpu_on.json -v -t engine.sim,render.bgfx
timeout 25s ./<build-dir>/bz3 --backend-render bgfx --strict-config=true --config /tmp/r26b-bgfx-gpu-compare-20260214T173727Z/gpu_on.json -v -t engine.sim,render.bgfx

# Diligent GPU-request fallback check (shadows ON)
timeout -k 2s 25s ./<build-dir>/bz3 --backend-render diligent --strict-config=true --config /tmp/r26b-gpu-pass-20260214T173514Z/gpu_on.json -v -t engine.sim,render.diligent
```

Slice-3 evidence:
- BGFX CPU/GPU compare logs: `/tmp/r26b-bgfx-gpu-compare-20260214T173727Z/`.
- Diligent fallback log: `/tmp/r26b-gpu-pass-20260214T173514Z/diligent-gpu-request.log`.

BGFX ON-state CPU vs GPU prototype (steady-state, `steady_n=22`):
| Backend | Mode | steady mean fps | steady mean frame ms | steady mean avg_steps | steady worst `max_frame_ms` | Log |
|---|---|---:|---:|---:|---:|---|
| BGFX | `cpu_reference` | 34.58 | 28.93 | 1.73 | 61.11 | `/tmp/r26b-bgfx-gpu-compare-20260214T173727Z/bgfx-cpu.log` |
| BGFX | `gpu_default` | 35.63 | 28.09 | 1.69 | 64.98 | `/tmp/r26b-bgfx-gpu-compare-20260214T173727Z/bgfx-gpu.log` |

Slice-3 delta summary:
- BGFX GPU prototype shows modest ON-state gain vs CPU reference (`+3.0%` FPS, `-2.9%` frame time).
- BGFX GPU trace confirms active GPU path (`gpu shadow pass size=512 draws=14`) with no BGFX fallback trace.
- Diligent still reports explicit fallback when `gpu_default` is requested (`reason=gpu_shadow_pass_not_implemented`).

Slice-3 known risks / follow-up:
- Visual parity of BGFX GPU pass still needs explicit screenshot/operator confirmation.
- GPU depth output currently uses shader linearization + min blending; this should be hardened with additional invariants.
- Diligent GPU parity was pending at the end of slice-3 and is addressed in slice-4 below.

R26-B slice-4 Diligent GPU pass prototype landed (2026-02-14):
1. Implemented Diligent GPU shadow-pass path under the same execution-mode contract in `src/engine/renderer/backends/diligent/backend_diligent.cpp`:
   - added `shadow_depth_pso` + shadow SRB (`createShadowDepthPipeline()`),
   - added render-target shadow texture lifecycle (`ensureShadowTexture(size, render_target)`),
   - added GPU pass submission (`renderGpuShadowMap(...)`) with trace token `gpu shadow pass size={} draws={}`,
   - wired `gpu_default` to projection-only build (`BuildDirectionalShadowProjection`) + GPU pass,
   - retained `cpu_reference` path (`BuildDirectionalShadowMap` + `updateShadowTexture`) as deterministic fallback path.
2. Added Diligent capability/fallback semantics to match BGFX policy:
   - fallback reason tokens now include `gpu_shadow_pipeline_unavailable` and `gpu_shadow_no_casters` when relevant.
3. Fixed Diligent GPU-path validation assertion:
   - first GPU run failed with `DvpVerifySRBCompatibility()` (`shadow_depth_pso` required SRB),
   - fixed by creating/committing `shadow_depth_srb_` before shadow draw submission.

Slice-4 validation commands and outcomes (run from `m-rewrite/`):
```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent      # pass
./<build-dir>/src/engine/directional_shadow_contract_test      # pass

# 4-way ON-state matrix (BGFX/Diligent x cpu_reference/gpu_default)
# temporary capture configs created under data/client/, then removed
timeout 25s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config ./data/client/config_r26b_cpu_capture.json -v -t engine.sim,render.bgfx
timeout 25s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config ./data/client/config_r26b_gpu_capture.json -v -t engine.sim,render.bgfx
timeout 25s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config ./data/client/config_r26b_cpu_capture.json -v -t engine.sim,render.diligent
timeout 25s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config ./data/client/config_r26b_gpu_capture.json -v -t engine.sim,render.diligent
```
- Final capture exits: all `124` (expected bounded run).
- Evidence dir: `/tmp/r26b-diligent-gpu-compare-20260214T175043Z/`.

Slice-4 4-way ON-state matrix (steady-state, drop first `perf1s`, `steady_n=22`):
| Backend | Mode | steady mean fps | steady mean frame ms | steady mean avg_steps | steady worst `max_frame_ms` | Log |
|---|---|---:|---:|---:|---:|---|
| BGFX | `cpu_reference` | 34.00 | 29.47 | 1.77 | 66.55 | `/tmp/r26b-diligent-gpu-compare-20260214T175043Z/bgfx-cpu.log` |
| BGFX | `gpu_default` | 34.90 | 28.72 | 1.72 | 64.52 | `/tmp/r26b-diligent-gpu-compare-20260214T175043Z/bgfx-gpu.log` |
| Diligent | `cpu_reference` | 29.10 | 34.52 | 2.07 | 73.64 | `/tmp/r26b-diligent-gpu-compare-20260214T175043Z/diligent-cpu.log` |
| Diligent | `gpu_default` | 33.75 | 29.68 | 1.78 | 69.28 | `/tmp/r26b-diligent-gpu-compare-20260214T175043Z/diligent-gpu.log` |

Slice-4 delta summary:
- BGFX GPU mode remains modestly better than BGFX CPU reference (`+2.6%` FPS, `-2.5%` frame time).
- Diligent GPU mode closes the major ON-state gap vs Diligent CPU reference (`+16.0%` FPS, `-14.0%` frame time).
- Trace evidence confirms active GPU pass in both backends:
  - BGFX: `gpu shadow pass size=512 draws=14`
  - Diligent: `gpu shadow pass size=512 draws=14`
- No `gpu_default -> cpu_reference` fallback trace was emitted in the final 4 captures.

Slice-4 known risks / next hardening:
- Visual parity checkpoints still need explicit screenshot/operator signoff for `BGFX gpu_default` vs `Diligent gpu_default`.
- Diligent GPU shadow texture lifecycle must be hardened for resize/device-loss and explicit failure fallback.
- GPU shadow-pass invariants should be expanded to prevent silent state regression (resource transitions + SRB readiness).

R26-B slice-5 lifecycle/fallback hardening landed (2026-02-14):
1. Hardened Diligent GPU path failure behavior in `src/engine/renderer/backends/diligent/backend_diligent.cpp`:
   - if `gpu_default` shadow pass render fails, backend now traces reason `gpu_shadow_render_failed` and deterministically falls back to CPU reference shadow build/upload in the same update window.
2. Hardened shadow resource lifecycle on swapchain resize in `src/engine/renderer/backends/diligent/backend_diligent.cpp`:
   - added `resetShadowResources()` and forced shadow cadence refresh (`shadow_frames_until_update_=0`) after resize,
   - added trace token `shadow resources reset reason=swapchain_resize`.
3. Re-ran full 4-way ON-state matrix after hardening:
   - evidence dir: `/tmp/r26b-hardening-compare-20260214T182247Z/`,
   - all four bounded runs exited `124` as expected.

Slice-5 validation commands and outcomes (run from `m-rewrite/`):
```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent      # pass
./<build-dir>/src/engine/directional_shadow_contract_test      # pass

timeout -k 2s 25s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config ./data/client/config_r26b_cpu_capture.json -v -t engine.sim,render.bgfx
timeout -k 2s 25s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config ./data/client/config_r26b_gpu_capture.json -v -t engine.sim,render.bgfx
timeout -k 2s 25s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config ./data/client/config_r26b_cpu_capture.json -v -t engine.sim,render.diligent
timeout -k 2s 25s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config ./data/client/config_r26b_gpu_capture.json -v -t engine.sim,render.diligent
```

Slice-5 4-way ON-state matrix (steady-state, drop first `perf1s`, `steady_n=22`):
| Backend | Mode | steady mean fps | steady mean frame ms | steady mean avg_steps | steady worst `max_frame_ms` | Log |
|---|---|---:|---:|---:|---:|---|
| BGFX | `cpu_reference` | 35.31 | 28.33 | 1.70 | 38.19 | `/tmp/r26b-hardening-compare-20260214T182247Z/bgfx-cpu.log` |
| BGFX | `gpu_default` | 35.91 | 27.86 | 1.67 | 34.49 | `/tmp/r26b-hardening-compare-20260214T182247Z/bgfx-gpu.log` |
| Diligent | `cpu_reference` | 30.18 | 33.14 | 1.99 | 42.48 | `/tmp/r26b-hardening-compare-20260214T182247Z/diligent-cpu.log` |
| Diligent | `gpu_default` | 35.45 | 28.21 | 1.69 | 34.95 | `/tmp/r26b-hardening-compare-20260214T182247Z/diligent-gpu.log` |

Slice-5 delta summary:
- BGFX remains slightly better in GPU mode vs CPU reference (`+1.7%` FPS, `-1.7%` frame time).
- Diligent GPU mode remains materially improved over CPU reference (`+17.5%` FPS, `-14.9%` frame time).
- Trace evidence confirms active GPU pass in both backends:
  - BGFX: `gpu shadow pass size=512 draws=14`
  - Diligent: `gpu shadow pass size=512 draws=14`
- No fallback trace tokens were emitted in this steady-state matrix (`gpu_shadow_render_failed` absent), indicating no runtime fallback events during the sampled window.

R26-B visual closeout checkpoint (partial; tooling blocker documented, 2026-02-14):
1. Added sandbox parity control to force GPU-path capture mode:
   - `src/engine/renderer/tests/renderer_shadow_sandbox.cpp` now accepts `--shadow-execution-mode <cpu_reference|gpu_default>`.
2. Captured deterministic visual artifact for BGFX `gpu_default` (sandbox, Xvfb/X11):
   - evidence dir: `/tmp/r26b-visual-closeout-20260214T183000Z/`
   - image files:
     - `/tmp/r26b-visual-closeout-20260214T183000Z/sandbox-bgfx-gpu-t4.png`
     - `/tmp/r26b-visual-closeout-20260214T183000Z/sandbox-bgfx-gpu-t7.png`
   - image hash parity (`t4` == `t7`): `6e336f78915e36010299d0471cf3f942a9ec70d81de48e84d2fd77afc9dc69f7`
3. Diligent screenshot capture is blocked in this environment:
   - Xvfb/X11 path: Diligent Vulkan swapchain fails (`VK_ERROR_INITIALIZATION_FAILED`) in sandbox.
   - Wayland path: render succeeds, but no non-interactive screenshot pipeline is available here.
4. Wayland fallback checkpoint (trace/diagnostic parity) confirms GPU path behavior parity:
   - BGFX log: `/tmp/r26b-visual-closeout-20260214T183000Z/sandbox-bgfx-gpu-wayland.log`
   - Diligent log: `/tmp/r26b-visual-closeout-20260214T183000Z/sandbox-diligent-gpu-wayland-forced.log`
   - both backends show:
     - `gpu shadow pass size=1024 draws=3`
     - `mode=gpu_default`
     - identical sandbox shadow diagnostics (`ground_draw_factor`, `ground_grid_factor`, `flattening_gap`).

Visual closeout commands and outcomes:
```bash
# build gates
./abuild.py -c -d <build-dir> -b bgfx,diligent      # pass

# contract tests
./<build-dir>/src/engine/directional_shadow_contract_test      # pass

# BGFX screenshot capture (sandbox, gpu_default, Xvfb/X11)
xvfb-run -a --server-args="-screen 0 1280x720x24" \
  ./<build-dir>/src/engine/renderer_shadow_sandbox \
  --backend-render bgfx --duration-sec 10 --video-driver x11 \
  --shadow-execution-mode gpu_default --trace render.system,render.bgfx --verbose
# pass (images captured)

# Diligent screenshot capture attempt (sandbox, gpu_default, Xvfb/X11)
xvfb-run -a --server-args="-screen 0 1280x720x24" \
  ./<build-dir>/src/engine/renderer_shadow_sandbox \
  --backend-render diligent --duration-sec 10 --video-driver x11 \
  --shadow-execution-mode gpu_default --trace render.system,render.diligent --verbose
# fail (VK_ERROR_INITIALIZATION_FAILED on X11 swapchain in this environment)

# Wayland trace parity fallback for Diligent and BGFX (no screenshot path)
timeout -k 2s 10s ./<build-dir>/src/engine/renderer_shadow_sandbox \
  --backend-render diligent --duration-sec 8 --video-driver wayland \
  --shadow-execution-mode gpu_default --trace render.system,render.diligent --verbose
timeout -k 2s 10s ./<build-dir>/src/engine/renderer_shadow_sandbox \
  --backend-render bgfx --duration-sec 8 --video-driver wayland \
  --shadow-execution-mode gpu_default --trace render.system,render.bgfx --verbose
# both pass
```

R26-B gpu_default no-shadow regression fix attempt (pending operator visual verify, 2026-02-14):
1. Symptom reproduced by operator:
   - `cpu_reference` shows shadows in both BGFX and Diligent.
   - `gpu_default` shows no visible shadows in both BGFX and Diligent.
2. Root-cause hypothesis and fix:
   - GPU shadow depth-pass VS used clip-space Z mapping `depth * 2 - 1` unconditionally.
   - On Vulkan zero-to-one clip-depth paths this can clip away caster geometry and leave shadow maps effectively empty.
   - Landed shared clip-depth transform helper in `src/engine/renderer/backends/internal/directional_shadow.hpp`:
     - `ResolveShadowClipDepthTransform(homogeneous_depth)` -> `(scale,bias)`.
   - BGFX now derives `(scale,bias)` from `bgfx::getCaps()->homogeneousDepth` and passes via `u_shadowParams2.zw` in:
     - `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
     - `data/bgfx/shaders/shadow/vs_shadow_depth.sc`
   - Diligent shadow-depth VS now uses `u_shadowParams2.zw` (Vulkan currently resolves to `1,0`) in:
     - `src/engine/renderer/backends/diligent/backend_diligent.cpp`
3. Regression guardrails added:
   - `src/engine/renderer/tests/directional_shadow_contract_test.cpp` now includes `RunShadowClipDepthTransformChecks()`.
4. Validation after fix (run from `m-rewrite/`):
```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent      # pass
./<build-dir>/src/engine/directional_shadow_contract_test      # pass
```
5. Remaining acceptance gate:
   - operator visual confirmation that `gpu_default` shadows are restored on desktop runtime for both backends.

R26-D config-surface policy slice landed (2026-02-14):
1. Added bounded performance-sensitive CPU shadow raster budget control:
   - contract field: `triangle_budget` in `include/karma/renderer/types.hpp`
   - runtime config plumbing in `src/game/client/main.cpp`:
     - `roamingMode.graphics.lighting.shadows.triangleBudget`
   - semantics clamp wiring in `src/engine/renderer/backends/internal/directional_shadow.hpp`:
     - bounded range `[1, 65536]`, fallback `4096`.
2. Added operator-facing defaults:
   - `data/client/config.json`
   - `data/client/config_tank_preview.json`
3. Added sandbox parity control for bounded repro sweeps:
   - `src/engine/renderer/tests/renderer_shadow_sandbox.cpp`
   - new CLI arg: `--shadow-triangle-budget <1..65536>`.
4. Added regression coverage:
   - `src/engine/renderer/tests/directional_shadow_contract_test.cpp`
   - invalid triangle budget now asserted to fallback to `4096`.
5. Added runtime observability:
   - `src/engine/renderer/render_system.cpp` trace now reports `tris=<triangle_budget>` in shadow settings.
6. Shadow-map trace semantics hardened for GPU path readability:
   - `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
   - `src/engine/renderer/backends/diligent/backend_diligent.cpp`
   - trace now reports `source=cpu_raster|gpu_projection`; GPU projection mode intentionally reports `covered=-1` (CPU depth-coverage metric is not applicable).

R26-D validation commands and outcomes (run from `m-rewrite/`):
```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent      # pass
./<build-dir>/src/engine/directional_shadow_contract_test      # pass
```

R26-D perf-capture note:
- Attempted autonomous 4-way ON-state `bz3` refresh capture in this environment (`/tmp/r26b-refresh-20260214T202029Z/`) was terminated by host kills (`exit 137`) before stable `perf1s` windows; partial traces still confirmed expected shadow-path routing.
- Use operator desktop/runtime session for canonical perf matrix refresh.
- Short sandbox trace sanity (`gpu_default`) still confirmed new trace semantics despite host kills:
  - `/tmp/r26b-shadow-source-bgfx.log` -> `shadow map layer=0 source=gpu_projection ... covered=-1`
  - `/tmp/r26b-shadow-source-diligent.log` -> `shadow map layer=0 source=gpu_projection ... covered=-1`

R26-D fixed-policy notes (explicit):
1. Shadow view-fit expansion constants in `BuildDirectionalShadowProjection(...)` (for example 1.05 extent guard, depth padding factors) remain fixed-policy for now.
2. Reason: they are correctness/stability safeguards, not operator tuning controls; exposing them prematurely would create high-dimensional unstable config space during ongoing parity hardening.
3. Follow-up policy: revisit exposure only after GPU shadow depth-attachment path and parity/perf matrix stabilize.

R26-B slice-6 depth-attachment GPU shadow path landed (2026-02-15):
1. BGFX GPU shadow path now prefers depth attachment with fallback:
   - `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
   - `ensureShadowTexture(...)` now attempts `TextureFormat::D32F` for GPU render-target use when supported and falls back to `R32F` color-min path if needed.
   - `renderGpuShadowMap(...)` now runs depth-write/depth-test state when attachment is depth, and traces attachment mode.
2. Diligent GPU shadow path now uses depth-stencil attachment by default with fallback:
   - `src/engine/renderer/backends/diligent/backend_diligent.cpp`
   - `ensureShadowTexture(...)` now attempts `TEX_FORMAT_D32_FLOAT` with `BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE`, falling back to `R32_FLOAT` color-min path if depth SRV/DSV creation is unavailable.
   - `renderGpuShadowMap(...)` now binds DSV for depth path and transitions from `DEPTH_WRITE -> SHADER_RESOURCE`.
3. Diligent shadow depth pipeline moved to depth-only configuration:
   - `src/engine/renderer/backends/diligent/backend_diligent.cpp`
   - `createShadowDepthPipeline()` now uses `NumRenderTargets=0`, `DSVFormat=D32_FLOAT`, `DepthEnable=true`, `DepthWriteEnable=true`, `DepthFunc=LESS`, and no pixel shader.
4. BGFX shadow depth shader pair updated for depth-only pass:
   - `data/bgfx/shaders/shadow/vs_shadow_depth.sc`
   - `data/bgfx/shaders/shadow/fs_shadow_depth.sc`
   - `data/bgfx/shaders/shadow/varying.def.sc`
   - removed color-depth payload varying/output from the GPU shadow depth pass.
5. Observability hardening:
   - both backends now trace `gpu shadow pass size={} draws={} attachment={depth|color_min}` so depth-attachment enablement is explicit in runtime evidence.

R26-B slice-6 validation commands and outcomes (run from `m-rewrite/`):
```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent      # pass
./<build-dir>/src/engine/directional_shadow_contract_test      # pass

# matrix configs (temporary; removed after capture)
jq '.roamingMode.graphics.lighting.shadows.enabled=false' data/client/config.json > data/client/config_r26d_shadow_off.json
jq '.roamingMode.graphics.lighting.shadows.enabled=true | .roamingMode.graphics.lighting.shadows.executionMode="gpu_default"' data/client/config.json > data/client/config_r26d_shadow_on_gpu.json

timeout -k 2s 20s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config data/client/config_r26d_shadow_off.json -v -t engine.sim,render.bgfx
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config data/client/config_r26d_shadow_on_gpu.json -v -t engine.sim,render.bgfx
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config data/client/config_r26d_shadow_off.json -v -t engine.sim,render.diligent
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config data/client/config_r26d_shadow_on_gpu.json -v -t engine.sim,render.diligent

rm -f data/client/config_r26d_shadow_off.json data/client/config_r26d_shadow_on_gpu.json
```
- Final bounded run exits: all `124` (expected timeout).
- Evidence dir: `/tmp/r26d-depth-matrix-20260215T054107Z/`.

R26-B slice-6 refreshed roaming matrix (steady-state, drop first `perf1s`, `steady_n=17`):
| Backend | Shadows | Mode | steady mean fps | steady mean frame ms | steady mean avg_steps | steady worst `max_frame_ms` | Log |
|---|---|---|---:|---:|---:|---:|---|
| BGFX | OFF | n/a | 59.75 | 16.74 | 1.00 | 35.94 | `/tmp/r26d-depth-matrix-20260215T054107Z/bgfx-shadow-off.log` |
| BGFX | ON | `gpu_default` | 35.92 | 27.85 | 1.67 | 34.91 | `/tmp/r26d-depth-matrix-20260215T054107Z/bgfx-shadow-on-gpu.log` |
| Diligent | OFF | n/a | 59.28 | 16.87 | 1.01 | 41.45 | `/tmp/r26d-depth-matrix-20260215T054107Z/diligent-shadow-off.log` |
| Diligent | ON | `gpu_default` | 36.21 | 27.62 | 1.66 | 39.80 | `/tmp/r26d-depth-matrix-20260215T054107Z/diligent-shadow-on-gpu.log` |

Slice-6 delta summary:
- BGFX shadow tax (`OFF -> ON gpu_default`): `-39.9%` FPS (`59.75 -> 35.92`), `+66.4%` frame time (`16.74 -> 27.85`).
- Diligent shadow tax (`OFF -> ON gpu_default`): `-38.9%` FPS (`59.28 -> 36.21`), `+63.7%` frame time (`16.87 -> 27.62`).
- ON-state backend parity is now tight in this run (`36.21` vs `35.92`, Diligent `+0.8%` FPS).
- Trace evidence confirms depth attachment is active in both backends:
  - BGFX: `gpu shadow pass size=512 draws=14 attachment=depth`
  - Diligent: `gpu shadow pass size=512 draws=14 attachment=depth`
  - plus `shadow map layer=0 source=gpu_projection ... covered=-1 ... uploaded=1` in both logs.

R26-B slice-6 recommendation and risks:
1. Next implementation order (`R26-B` -> `R26-D` bridge):
   - add receiver/normal/raster bias model parity under shared contracts,
   - wire bounded config controls for those new performance-sensitive bias terms in `data/client/config.json`,
   - hold depth-attachment fallback behavior as fixed-policy with explicit trace evidence.
2. Risks:
   - some GPU/driver stacks may fail depth SRV/DSV creation and silently use fallback unless attachment trace is checked; keep `attachment=` trace in perf recipes.
   - depth path visual quality still depends on current bias model; without receiver/normal/raster terms, acne vs peter-panning tuning remains constrained.

R26-D bias-model policy slice landed (2026-02-15):
1. Extended shared shadow contract with bounded bias controls:
   - `include/karma/renderer/types.hpp`
   - new fields under `DirectionalLightData::ShadowDesc`:
     - `receiver_bias_scale` (default `0.08`)
     - `normal_bias_scale` (default `0.35`)
     - `raster_depth_bias` (default `0.0`)
     - `raster_slope_bias` (default `0.0`)
2. Added bounded semantics/clamp policy in shared renderer internals:
   - `src/engine/renderer/backends/internal/directional_shadow.hpp`
   - clamp ranges:
     - `receiver_bias_scale`: `[0.0, 4.0]`
     - `normal_bias_scale`: `[0.0, 8.0]`
     - `raster_depth_bias`: `[0.0, 0.02]`
     - `raster_slope_bias`: `[0.0, 8.0]`
3. Runtime config plumbing added:
   - `src/game/client/main.cpp`
   - keys:
     - `roamingMode.graphics.lighting.shadows.receiverBiasScale`
     - `roamingMode.graphics.lighting.shadows.normalBiasScale`
     - `roamingMode.graphics.lighting.shadows.rasterDepthBias`
     - `roamingMode.graphics.lighting.shadows.rasterSlopeBias`
4. Operator defaults added:
   - `data/client/config.json`
   - `data/client/config_tank_preview.json`
5. Backend parity wiring:
   - BGFX:
     - `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
     - `data/bgfx/shaders/mesh/fs_mesh.sc`
     - `data/bgfx/shaders/shadow/vs_shadow_depth.sc`
     - `data/bgfx/shaders/shadow/varying.def.sc`
   - Diligent:
     - `src/engine/renderer/backends/diligent/backend_diligent.cpp`
   - Both backends now use the same bias parameter contract in GPU sampling and depth-pass offset behavior.
6. Sandbox policy/debug control expansion:
   - `src/engine/renderer/tests/renderer_shadow_sandbox.cpp`
   - new CLI knobs:
     - `--shadow-receiver-bias <0..4>`
     - `--shadow-normal-bias <0..8>`
     - `--shadow-raster-depth-bias <0..0.02>`
     - `--shadow-raster-slope-bias <0..8>`
7. Regression coverage + observability:
   - `src/engine/renderer/tests/directional_shadow_contract_test.cpp` now validates new clamp/fallback behavior.
   - `src/engine/renderer/render_system.cpp` traces now include `recv`, `norm`, `rasterDepth`, `rasterSlope`.

R26-D bias-model validation commands and outcomes (run from `m-rewrite/`):
```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent      # pass
./<build-dir>/src/engine/directional_shadow_contract_test      # pass

timeout -k 2s 20s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config data/client/config.json -v -t engine.sim,render.bgfx
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config data/client/config.json -v -t engine.sim,render.diligent
timeout -k 2s 10s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config data/client/config.json -v -t render.system,render.bgfx
timeout -k 2s 10s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config data/client/config.json -v -t render.system,render.diligent
```
- Final bounded run exits: all `124` (expected timeout).
- Evidence logs:
  - `/tmp/r26e-bias-bgfx.log`
  - `/tmp/r26e-bias-diligent.log`
  - `/tmp/r26e-bias-bgfx-system.log`
  - `/tmp/r26e-bias-diligent-system.log`
- Key trace confirmations:
  - BGFX: `gpu shadow pass size=512 draws=14 attachment=depth`
  - Diligent: `gpu shadow pass size=512 draws=14 attachment=depth`
  - RenderSystem now reports bias fields in both backends:
    - `... shadows(... bias=... recv=... norm=... rasterDepth=... rasterSlope=... mode=gpu_default)`.

R26-D bounded bias sweep + default lock (2026-02-16):
1. Sweep goal:
   - compare bounded bias presets (`low`, `default`, `high`) in roaming mode with `gpu_default` shadows,
   - validate backend parity while keeping depth attachment active.
2. Sweep presets:
   - `low`: `recv=0.03 norm=0.12 rasterDepth=0.0000 rasterSlope=0.2`
   - `default`: `recv=0.08 norm=0.35 rasterDepth=0.0000 rasterSlope=0.0`
   - `high`: `recv=0.14 norm=0.60 rasterDepth=0.0012 rasterSlope=2.0`
3. Commands (run from `m-rewrite/`):
```bash
# temp configs (removed after sweep)
cp data/client/config.json data/client/config_r26e_bias_default.json
jq '.roamingMode.graphics.lighting.shadows.receiverBiasScale=0.03 | .roamingMode.graphics.lighting.shadows.normalBiasScale=0.12 | .roamingMode.graphics.lighting.shadows.rasterDepthBias=0.0 | .roamingMode.graphics.lighting.shadows.rasterSlopeBias=0.2' data/client/config.json > data/client/config_r26e_bias_low.json
jq '.roamingMode.graphics.lighting.shadows.receiverBiasScale=0.14 | .roamingMode.graphics.lighting.shadows.normalBiasScale=0.60 | .roamingMode.graphics.lighting.shadows.rasterDepthBias=0.0012 | .roamingMode.graphics.lighting.shadows.rasterSlopeBias=2.0' data/client/config.json > data/client/config_r26e_bias_high.json

for profile in low default high; do
  timeout -k 2s 16s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config data/client/config_r26e_bias_${profile}.json -v -t engine.sim,render.system,render.bgfx > /tmp/r26e-bias-sweep-20260216T022344Z/bgfx-${profile}.log 2>&1
  timeout -k 2s 16s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config data/client/config_r26e_bias_${profile}.json -v -t engine.sim,render.system,render.diligent > /tmp/r26e-bias-sweep-20260216T022344Z/diligent-${profile}.log 2>&1
done

rm -f data/client/config_r26e_bias_low.json data/client/config_r26e_bias_default.json data/client/config_r26e_bias_high.json
```
4. Sweep outcomes:
   - all bounded runs exited `124` (expected timeout).
   - evidence dir: `/tmp/r26e-bias-sweep-20260216T022344Z/`.
   - all six runs confirmed `attachment=depth`.
5. Sweep matrix (steady-state mean, drop first `perf1s`, `steady_n=13`):
| Backend | Profile | Bias tuple (`recv/norm/rDepth/rSlope`) | steady mean fps | steady mean frame ms | steady mean avg_steps | steady worst `max_frame_ms` | Log |
|---|---|---|---:|---:|---:|---:|---|
| BGFX | low | `0.03/0.12/0.0000/0.2` | 40.15 | 25.11 | 1.51 | 38.78 | `/tmp/r26e-bias-sweep-20260216T022344Z/bgfx-low.log` |
| BGFX | default | `0.08/0.35/0.0000/0.0` | 37.67 | 26.59 | 1.60 | 42.29 | `/tmp/r26e-bias-sweep-20260216T022344Z/bgfx-default.log` |
| BGFX | high | `0.14/0.60/0.0012/2.0` | 36.55 | 27.42 | 1.64 | 39.45 | `/tmp/r26e-bias-sweep-20260216T022344Z/bgfx-high.log` |
| Diligent | low | `0.03/0.12/0.0000/0.2` | 35.61 | 28.08 | 1.69 | 58.96 | `/tmp/r26e-bias-sweep-20260216T022344Z/diligent-low.log` |
| Diligent | default | `0.08/0.35/0.0000/0.0` | 36.04 | 27.75 | 1.67 | 37.28 | `/tmp/r26e-bias-sweep-20260216T022344Z/diligent-default.log` |
| Diligent | high | `0.14/0.60/0.0012/2.0` | 35.52 | 28.15 | 1.69 | 42.07 | `/tmp/r26e-bias-sweep-20260216T022344Z/diligent-high.log` |
6. Default lock decision:
   - keep `default` as production baseline (`recv=0.08 norm=0.35 rasterDepth=0.0000 rasterSlope=0.0`).
   - rationale:
     - best cross-backend consistency (Diligent default is top in this sweep; BGFX delta vs low is acceptable for parity-first preset),
     - lower worst-frame volatility than Diligent low profile (`37.28ms` vs `58.96ms` worst `max_frame_ms`),
     - avoids over-bias risk from `high` profile while preserving stable depth-attachment parity behavior.
7. Automation follow-up landed:
   - added `scripts/run-renderer-shadow-bias-sweep.sh` to run the 3-profile sweep end-to-end (temp config generation, BGFX+Diligent captures, exit codes, and summary table).
   - smoke run evidence: `/tmp/renderer-shadow-bias-sweep-20260216T025300Z/`.

R26-C m-dev intake matrix (2026-02-14):

Reference root:
- `/home/karmak/dev/bz3-rewrite/m-dev`

| Candidate technique from m-dev | Evidence (m-dev) | Classification | Rationale | Concrete follow-up |
|---|---|---|---|---|
| Lazy offscreen scene-target allocation only when post-brightness is active | `src/engine/graphics/backends/bgfx/backend.cpp` (`wantsBrightness`, `ensureSceneTarget`); `src/engine/graphics/backends/diligent/backend.cpp` (`wantsBrightness`, `ensureSceneTarget`) | `adopt now` (already aligned in R26-B) | m-dev pattern is a proven GPU-resource-lifecycle guardrail. Rewrite shadow GPU path already follows this with lazy `ensureShadowTexture(...)` + resize reset semantics. | Keep this as enforced parity invariant in R26-D docs/config notes; add a short invariant test later that resize forces shadow-resource refresh. |
| Offscreen-pass draw pruning (skip grass/expensive non-critical meshes) | `src/engine/graphics/backends/bgfx/backend.cpp:940`; `src/engine/graphics/backends/diligent/backend.cpp:2154` | `adopt later` | Rewrite shadow pass already prunes to shadow casters, but m-dev shows additional content-class pruning can reduce pass cost. Needs a backend-neutral content policy knob (not a backend heuristic). | Stage a rewrite-owned caster-policy flag in R26-D (for example, content-tag-based shadow caster filtering) after policy review. |
| Config-revision-gated lighting constant refresh to avoid per-frame config churn | `src/engine/graphics/backends/bgfx/backend.cpp:881-883` | `reject` | Rewrite already routes directional light through engine contracts each frame; no backend config polling loop exists to optimize in this spot. | No code action; document as “already solved via contract-owned light push.” |
| Aggressive model/texture cache reuse across draws | `src/engine/graphics/backends/bgfx/backend.cpp` (`modelMeshCache`, `textureCache`); `src/engine/graphics/backends/diligent/backend.cpp` (`modelMeshCache`, `textureCache`) | `reject` (already present) | Rewrite renderer already maintains mesh/material/texture lifetime caches and shadow caster reuse; direct port would duplicate existing behavior. | No code action; keep current cache-path profiling in future perf traces. |
| Legacy shadow knobs in app config without guaranteed runtime plumbing | `src/engine/app/engine_config.hpp` (`shadow_map_size`, `shadow_pcf_radius`) | `reject` (anti-pattern) | This is exactly the policy gap rewrite is trying to avoid. Performance-sensitive controls must be either runtime-wired or explicitly fixed-policy documented. | Use R26-D to keep rewrite controls explicit and validated in `data/client/config.json` docs. |

## Primary Specs
- `docs/foundation/architecture/core-engine-contracts.md` (renderer sections)
- `docs/projects/ui-integration.md` (renderer/UI coupling points)

## Why This Is Separate
Renderer feature work can proceed independently of server networking and backend audio/physics work.

## Owned Paths
- `docs/projects/renderer-parity.md`
- `m-rewrite/src/engine/renderer/*`
- renderer-related scene/bootstrap interactions in `m-rewrite/src/engine/scene/*`
- `m-rewrite/data/bgfx/shaders/mesh/*`
- `m-rewrite/src/engine/CMakeLists.txt` (renderer-only test/shader wiring if needed)
- `docs/projects/ASSIGNMENTS.md`

## Interface Boundaries
- Inputs: scene/world content and render submissions.
- Outputs: frame rendering and backend parity behavior.
- Coordinate before changing:
  - UI integration paths (`m-rewrite/src/engine/ui/*`)
  - `docs/foundation/architecture/core-engine-contracts.md` renderer sections
  - `docs/foundation/governance/engine-backend-testing.md`
  - `docs/foundation/governance/testing-ci-governance.md`

## Non-Goals
- Gameplay/network semantics.
- Physics/audio backend internals.

## Validation
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config data/client/config.json -v -t engine.sim,render.system,render.bgfx
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config data/client/config.json -v -t engine.sim,render.system,render.diligent
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `engine.sim`
- `render.system`
- `render.mesh`
- `render.bgfx`
- `render.diligent`
- `ecs.world`

## Build/Run Commands
```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config data/client/config.json -v -t engine.sim,render.system,render.bgfx
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config data/client/config.json -v -t engine.sim,render.system,render.diligent
```

## First Session Checklist
1. Read renderer sections in `docs/foundation/architecture/core-engine-contracts.md`.
2. Confirm current parity target is capability/behavior, not file mirroring.
3. Implement one capability slice at a time.
4. Validate on both render backends.
5. Update status and parity notes.

## Current Status
- `2026-02-14`: R26 performance/GPU-offload program was added to this project to unify four linked efforts under one track: BGFX-vs-Diligent parity closure, KARMA CPU->GPU shadow/lighting intake, m-dev renderer-technique intake review, and config-surface expansion policy for performance-sensitive renderer controls.
- `2026-02-12`: active shadow stabilization/investigation work (sandbox bring-up, KARMA commit-mined shadow intake findings, and staged hardening plan) moved into the dedicated shadow track now archived at `docs/archive/renderer-shadow-hardening-superseded-2026-02-17.md`; the active successor track is `docs/projects/karma-lighting-shadow-parity.md`. This file remains the parity ledger and VQ rubric host, and must be synced whenever shadow-intake slices are accepted.
- `2026-02-11`: merged former `renderer-visual-quality.md` into this project as VQ1-VQ4 follow-up slices so renderer execution remains under one track/owner.
- `2026-02-11`: VQ1 diagnostics baseline accepted as shared unblocker for measurable renderer quality outcomes (deterministic repro recipe + explicit VQ2/VQ3 thresholds) while preserving backend-parity boundaries.
- `2026-02-11`: VQ2 texture minification quality slice started: shared RGBA8 mip-chain generation/upload path and anisotropic/trilinear material sampler policy are now wired for BGFX + Diligent under shared renderer contract helpers; VQ3/VQ4 remain untouched.
- `2026-02-11`: VQ2 closeout evidence attempt reran required build/run gates successfully, but TA scoring checkpoints (`t=6s`, `t=12s`) were not observable in this non-interactive environment (no visual capture/inspection path for far-field aliasing assessment); TA values and per-checkpoint parity guardrails remain unscored, so VQ2 stays in progress.
- `2026-02-11`: VQ2 evidence-unblock slice added deterministic operator runner `./scripts/run-renderer-vq2-evidence.sh` (strict config + canonical VQ1 flags + timestamped logs + backend exit-code reporting + lingering-process verification) so acceptance is pending scored TA checkpoints only.
- `2026-02-11`: VQ2 closeout accepted from manual TA worksheet inputs: BGFX `t=6s=0`, BGFX `t=12s=0`, Diligent `t=6s=0`, Diligent `t=12s=0`; parity deltas are `0` at both checkpoints and all VQ2 pass rules are satisfied.
- `2026-02-11`: VQ3 implementation slice is in progress with shared projected directional-shadow sampling improvements wired for BGFX + Diligent and directional shadow contract tests passing in both assigned build dirs; VQ3 acceptance is blocked on manual SV worksheet scoring at checkpoints `t=6s` and `t=16s` because this environment cannot perform reliable visual scoring.
- `2026-02-11`: operator-reported VQ3 baseline failure is documented (`SV` visually in failing `0-1` range: no obvious caster->receiver shadows around brick blocks), and VQ3 remains blocked on corrective evidence plus manual SV worksheet scoring.
- `2026-02-11`: VQ3 rejection-correction follow-up identified three regression drivers in the prior slice: inverted per-draw light direction contract (side-lighting break), oversized chunk/tessellation settings (64 chunk draws + seam pressure), and non-local receiver bounds sampling per chunk (excess CPU work + diluted locality). Corrective updates now restore the original light-direction uniform contract, use chunk-index-local receiver bounds sampling, and reduce ground chunk/tessellation pressure (`chunkDraws=16`, `layer0 draws=30`, `localized delta=0.053`, `localized=1`) on both BGFX and Diligent. Frame-trace stability is mixed: BGFX improved (`avg/max dt_raw 0.0202/0.4072 -> 0.0199/0.3468`), Diligent max improved but avg remains slightly above baseline (`0.0245/0.3482 -> 0.0261/0.3456`), so VQ3 remains `not accepted`.
- `2026-02-11`: stabilization rollback pass completed for VQ3 rejection handling: directional-shadow backend/test files were restored to pre-regression baseline to remove chunk/tessellation receiver paths, per-chunk draw amplification, and light-direction instability from this slice. Validation/build/runtime/test gates pass, and VQ3 remains in progress/not accepted pending new corrective planning and operator confirmation.
- Cross-backend startup/rendering path is working.
- R1 is implemented: both BGFX and Diligent now consume shared material semantics for metallic/roughness/emissive/alpha/double-sided fields plus metallic-roughness and emissive texture influence when present.
- R2 is now landed: engine-owned `DirectionalLightData::shadow` contract fields are consumed by both BGFX and Diligent through one shared bounded shadow-map build/sample path (`directional_shadow.hpp`) with deterministic per-draw light attenuation.
- R3 is now landed: engine-owned `EnvironmentLightingData` contract is consumed by both BGFX and Diligent through shared environment semantics resolution (`environment_lighting.hpp`) including sky/ground ambient IBL proxy, roughness-aware specular boost, and sky clear-color exposure policy.
- R4 is now landed: engine-owned debug-line contract path (`DebugLineItem` + shared semantics in `debug_line.hpp`) is consumed by both BGFX and Diligent with layer-aware line submission and deterministic validation coverage in `directional_shadow_contract_test.cpp`.
- R5 material-fidelity follow-up is accepted: shared material-lighting BRDF proxy (`material_lighting.hpp`) now drives direct/ambient light scaling in both BGFX and Diligent, and texture-path handling now uses representative texture sampling plus normalized RGBA expansion for non-RGBA texture channel layouts.
- R6 normal/occlusion texture-set semantics slice is accepted: engine-owned `MaterialDesc` now carries normal/occlusion texture contract fields, shared material semantics resolve bounded `normal_variation` + `occlusion` terms, and both BGFX + Diligent consume those terms through shared material-lighting resolution.
- R7 texture-set stability follow-up is accepted: normal/occlusion semantics now use deterministic quasi-random multi-sample aggregation (`kHighFrequencySampleCount = 32`) to improve higher-frequency pattern stability, with clamp-policy behavior explicitly asserted in contract tests.
- R8 normal/detail policy refinement + occlusion edge-case parity is accepted: shared semantics now apply deadzone+variance-weighted normal-detail policy and explicit occlusion edge snapping/bias policy, with parity consumed identically by BGFX + Diligent via shared material-lighting.
- R9 normal/detail response tuning + occlusion integration edge behavior is accepted: shared semantics now derive bounded `occlusion_edge`, shared material-lighting integrates occlusion-edge ambient lift and tuned bounded normal-detail response, and parity remains consumed identically by BGFX + Diligent.
- R10 bounded texture-set lifecycle ingestion is accepted: shared material semantics now include engine-owned normal/occlusion lifecycle ingestion helpers (bounded dimensions/texel budget + normalized channel payloads), and both BGFX + Diligent create parity-aligned lifecycle texture resources for these maps without backend API leakage.
- R11 bounded shader-path consumption is accepted: shared shader-path ingestion now consumes lifecycle-ingested normal/occlusion textures through a deterministic composite-texture path, and both BGFX + Diligent consume the same bounded policy with assertion-backed parity checks.
- R12 bounded multi-sampler shader-input contract slice is accepted: shared shader-input contract resolution now supports explicit direct multi-sampler input wiring with deterministic composite fallback retained, and both BGFX + Diligent consume the same contract/fallback path with assertion-backed parity checks.
- R13 runtime direct-path enablement slice is accepted: live normal/occlusion direct multi-sampler shader consumption is now enabled in both BGFX + Diligent with deterministic R12 fallback retained; BGFX mesh shader/runtime bindings and Diligent pipeline bindings now consume the same shared engine contract path.
- R14 direct-path observability/guardrail slice is accepted: BGFX now enforces shader-asset alignment checks (source contract markers, compiled binary presence/non-empty, binary mtime >= source mtime) before enabling direct path and emits explicit disable reasons; Diligent now validates sampler-contract readiness and emits explicit direct/fallback readiness and per-frame direct-vs-fallback draw observability, with deterministic fallback preserved when guardrails disable direct mode.
- R15 direct-path readiness recovery slice is accepted: Diligent now evaluates direct-path readiness from material-pipeline sampler contract availability (with explicit line-pipeline partial-contract observability), removing the prior false-negative disable path while retaining R14 guardrails and direct/fallback invariant telemetry.
- R16 direct-path observability hardening is accepted: Diligent readiness + direct/fallback invariants are now covered by explicit assertion-backed contract tests via shared renderer-internal observability helpers, locking material-contract readiness expectations and direct-vs-fallback draw invariants outside trace-only runtime telemetry.
- R17 BGFX direct-path observability parity is accepted: BGFX now consumes the same shared observability contract helpers for readiness evaluation and direct-vs-fallback invariant assertions, mirroring Diligent-side R16 coverage while preserving deterministic fallback behavior.
- R18 BGFX direct-path guardrail packaging-resilience is accepted: shared BGFX direct-sampler alignment policy now keeps strict source-present stale-vs-source safety checks while enabling explicit source-absent deployment compatibility when runtime binary contract validity is sufficient, with deterministic readiness/disable reasons consumed through shared direct-sampler observability contract helpers and assertion-backed source-present/source-absent parity checks.
- R19 BGFX deployed-shader binary integrity hardening is accepted: source-absent BGFX direct-path readiness now requires a deterministic deployed-binary integrity contract (`.integrity` sidecar with FNV-1a64 hash token match) before enablement, with explicit integrity disable reasons propagated through shared alignment/readiness helpers while source-present stale-vs-source behavior remains unchanged.
- R20 BGFX source-absent integrity-manifest schema hardening is accepted: `.integrity` parsing now uses a versioned schema contract (`version`, `algorithm`, `hash`) with explicit parse/version/algorithm/hash failure reasons propagated through source-absent readiness gating while preserving strict source-present stale-vs-source checks and deterministic fallback behavior.
- R21 strict source-absent manifest-shape hardening is accepted: `.integrity` parsing now deterministically rejects duplicate keys, unknown keys, and invalid token/value forms with explicit propagated disable reasons, while preserving accepted R1-R20 behavior and deterministic fallback semantics.
- R22 source-absent integrity canonicalization hardening is accepted: manifest parsing now applies deterministic canonicalization boundaries (CRLF normalization with explicit noncanonical line-ending detection, strict whitespace-boundary enforcement) and propagates explicit disable reasons through integrity policy -> alignment policy -> direct-path readiness.
- R23 source-absent signed-envelope/trust-chain planning hardening is accepted: `.integrity` parsing now supports optional signed-envelope metadata guardrails (`signed_envelope`, `signature`, `trust_chain`) with deterministic parse disable reasons, and source-absent readiness now deterministically disables direct path with `source_missing_and_integrity_signed_envelope_verification_deferred` when signed-envelope metadata is declared before cryptographic verification enablement.
- R24 source-absent verification-enablement is accepted: BGFX source-absent integrity now includes signed-envelope verification plumbing (mode support checks, trust-chain root extraction, trust-root policy lookup, deterministic signature verification input composition) and propagates explicit verification-prereq reasons through integrity -> alignment -> direct-path readiness (`...verification_mode_unsupported`, `...trust_root_missing`, `...trust_chain_material_invalid`, `...signature_material_invalid`, `...signature_verification_failed`), with deterministic fallback preserved.
- R25 source-absent signature-model hardening is accepted: BGFX signed-envelope verification now enforces canonical verification-input boundaries (canonical lowercase hash tokens, explicit trust-chain signing-key identity segment `root:<id>/signing-key:<id>`, manifest mode/version/algorithm binding, and deterministic canonical payload composition inputs) while preserving trust-root allowlist guardrails; explicit deterministic disable reason `source_missing_and_integrity_signed_envelope_verification_inputs_invalid` now propagates through integrity -> alignment -> direct-path readiness.
- R22 residual risk: strict canonicalization intentionally disables direct path for manifests that include noncanonical whitespace forms (for example tabs or embedded whitespace within key/value tokens), so packaging emitters must stay canonical.
- R25 residual risk: trust-root policy remains bounded to in-engine allowlisted root IDs/secrets and signature verification remains deterministic contract hardening rather than production asymmetric cryptography.
- R25 explicit deferral: external trust-store lifecycle/rotation tooling and production asymmetric signature primitives remain deferred.
- Focused assertion-backed verification for shadow + environment contract clamping/validation behavior is in `src/engine/renderer/tests/directional_shadow_contract_test.cpp` (wired in `src/engine/CMakeLists.txt`).
- Focused assertion-backed verification now also covers R5 texture-path + material-lighting semantics, R6 normal/occlusion bounded semantics, R7 high-frequency stability + clamp-policy assertions, R8 normal/detail policy + occlusion edge-case assertions, R9 occlusion-edge integration + normal/detail response behavior, R10 lifecycle ingestion bounds/parity behavior, R11 shader-path lifecycle-consumption parity behavior, R12 multi-sampler shader-input contract/fallback equivalence behavior, R13 direct-path runtime parity preservation behavior, R14 guardrail-preserved fallback behavior, R15 readiness-recovery preservation behavior, R16 direct-path observability contracts (Diligent readiness + direct-vs-fallback invariant assertions), R17 BGFX readiness/invariant parity assertions, R18 BGFX source-present/source-absent readiness-reason policy assertions, R19 source-absent deployed-binary integrity pass/fail + readiness-reason propagation assertions, R20 versioned source-absent integrity-manifest parse/version/algorithm reason propagation assertions, R21 duplicate/unknown/invalid-manifest-shape reason propagation assertions, R22 canonicalization line-ending/whitespace boundary reason propagation assertions, R23 signed-envelope parse/deferred-verification reason propagation assertions, R24 signed-envelope verification mode/trust-root/trust-chain/signature-material/signature-verification reason propagation assertions, and R25 signed-envelope canonical verification-input boundary reason propagation assertions (`...verification_inputs_invalid`) through the same shared observability helpers in `src/engine/renderer/tests/directional_shadow_contract_test.cpp`.
- Shared material-semantics validation remains in place; R1 semantics behavior is preserved while R2 applies only directional direct-light attenuation.
- Explicitly deferred after R3: full cubemap/image-backed environment lifecycle and shader sampling parity (current R3 is one bounded procedural/environment-parameter slice).

## VQ1 Deterministic Diagnostics Baseline (Accepted 2026-02-11)
Strategic alignment:
- Track: shared unblocker.
- Purpose: convert accepted renderer parity capability into measurable runtime quality outcomes for VQ2 (texture minification quality) and VQ3 (visible directional shadows), without changing runtime behavior in this slice.

Baseline scene/camera/runtime settings:
- Scene/content source: `data/client/config.json` -> `assets.models.world = models/world.glb`.
- Camera start pose: `data/client/config.json` -> `roamingMode.camera.roaming.StartPosition = [0.5, 13, 25]`, `StartTarget = [0.472, 12.652, 24.061]`.
- Runtime mode: roaming scene startup, `imgui` UI backend, backend-specific renderer override (`bgfx` or `diligent`).
- Config isolation: use a temporary user config file so local user overrides do not affect diagnostics.

Deterministic repro recipe (copy/paste runnable):
```bash
cd /home/karmak/dev/bz3-rewrite/m-rewrite
cat > /tmp/bz3-vq1-user-config.json <<'JSON'
{
  "DataDir": "/home/karmak/dev/bz3-rewrite/m-rewrite/data"
}
JSON

# BGFX baseline capture window (20s)
timeout 20s ./<build-dir>/bz3 --backend-render bgfx \
  --strict-config=true \
  --config /tmp/bz3-vq1-user-config.json \
  -v -t engine.app,render.mesh,render.bgfx

# Diligent baseline capture window (20s)
timeout 20s ./<build-dir>/bz3 --backend-render diligent \
  --strict-config=true \
  --config /tmp/bz3-vq1-user-config.json \
  -v -t engine.app,render.mesh,render.diligent
```

Deterministic camera path for scoring:
1. `Phase A (t=0s-8s)`: no input (hold start pose).
2. `Phase B (t=8s-14s)`: hold `W` continuously.
3. `Phase C (t=14s-20s)`: release all movement input (stabilize).

VQ1 scoring rubric (used by VQ2/VQ3 acceptance):
- Distant texture aliasing/grain score (`TA`):
  - `0` = none, `1` = minor, `2` = obvious, `3` = severe.
  - Sample checkpoints: `t=6s` (Phase A) and `t=12s` (Phase B), center-to-upper-ground far-field region.
- Shadow caster/receiver visibility score (`SV`):
  - `0` = no obvious shadow pair, `1` = faint/ambiguous, `2` = clear caster->receiver shadow, `3` = clear + stable.
  - Sample checkpoints: `t=6s` (Phase A) and `t=16s` (Phase C), obvious terrain/object receiver region in front-half of view.

Explicit future pass/fail thresholds:
- VQ2 pass criteria (must satisfy on both backends):
  - `TA <= 1` at both checkpoints.
  - backend parity guardrail: `|TA_bgfx - TA_diligent| <= 1` per checkpoint.
  - fail if either backend records `TA >= 2` at any checkpoint.
- VQ3 pass criteria (must satisfy on both backends):
  - `SV >= 2` at both checkpoints.
  - no full dropout (`SV = 0`) at either checkpoint.
  - backend parity guardrail: `|SV_bgfx - SV_diligent| <= 1` per checkpoint.

VQ1 slice acceptance criteria:
- Repro recipe is fully runnable with explicit commands, config isolation, and timing phases.
- Scoring rubric is explicit and shared across BGFX + Diligent.
- VQ2 and VQ3 pass/fail thresholds are documented and bounded.
- No runtime behavior change is introduced in this VQ1 slice.

## VQ2 Operator Worksheet (Manual TA Scoring)
Run deterministic evidence capture:

```bash
cd /home/karmak/dev/bz3-rewrite/m-rewrite
./scripts/run-renderer-vq2-evidence.sh
```

Runner output contract:
- uses only assigned runtime-select `<build-dir>` plus explicit renderer backend override (`--backend-render bgfx|diligent`).
- writes timestamped per-backend logs under `/tmp/vq2-renderer-evidence-<UTC_TIMESTAMP>/`.
- prints explicit backend exit codes plus post-run child-process verification status.

TA worksheet (fill from captured runs):

| Checkpoint | TA_bgfx | TA_diligent | Parity delta \|TA_bgfx-TA_diligent\| | Backend TA rule (`<= 1`) | Parity rule (`<= 1`) | Checkpoint result |
|---|---:|---:|---:|---|---|---|
| `t=6s` | 0 | 0 | 0 | Pass | Pass | Pass |
| `t=12s` | 0 | 0 | 0 | Pass | Pass | Pass |

VQ2 decision rules:
- pass only if `TA <= 1` at both checkpoints on both backends.
- parity guardrail must hold at each checkpoint: `|TA_bgfx - TA_diligent| <= 1`.
- fail if any checkpoint on either backend records `TA >= 2`.

VQ2 closeout decision (`2026-02-11`, manual visual worksheet inputs):
- Rule check: `TA <= 1` at both checkpoints on both backends -> `Pass` (`0, 0, 0, 0`).
- Rule check: parity guardrail `|TA_bgfx - TA_diligent| <= 1` per checkpoint -> `Pass` (`0` at `t=6s`, `0` at `t=12s`).
- Rule check: fail-if-any-`TA >= 2` -> `Pass` (no checkpoint >= 2).
- Decision: `VQ2 Accepted`.

## VQ3 Operator Worksheet (Manual SV Scoring)
Use the accepted VQ1 deterministic repro recipe and scoring checkpoints (`t=6s`, `t=16s`) for both backends.

Baseline failure case (`2026-02-11`, operator report):
- No obvious cast shadows under/around brick blocks in default roaming scene; treat baseline SV as failing (`0-1` range) until corrected.

Corrective observability run commands (copy/paste):
```bash
cd /home/karmak/dev/bz3-rewrite/m-rewrite
timeout 20s ./<build-dir>/bz3 --backend-render bgfx --strict-config=true --config /tmp/bz3-vq1-user-config.json -v -t engine.app,render.system,render.bgfx,render.mesh
timeout 20s ./<build-dir>/bz3 --backend-render diligent --strict-config=true --config /tmp/bz3-vq1-user-config.json -v -t engine.app,render.system,render.diligent,render.mesh
```

Corrective observability evidence (`2026-02-11`, latest corrective slice):
- BGFX trace: `shadow obs backend=bgfx layer=0 mapReady=1 casters=15 considered=15 sampledDraws=30 factorDraws=30 visCombined(min=0.078 avg=0.673) visReceiver(min=0.000 avg=0.810) shadowFactor(min=0.401 max=0.975)`.
- Diligent trace: `shadow obs backend=diligent layer=0 mapReady=1 casters=15 considered=15 sampledDraws=30 factorDraws=30 visCombined(min=0.078 avg=0.673) visReceiver(min=0.000 avg=0.810) shadowFactor(min=0.401 max=0.975)`.
- BGFX ground before/after diagnostic: `shadow ground diag backend=bgfx ... beforeUniform ... factor=0.901 ... afterChunked=1 chunkDraws=16 ... factorRange(min=0.864 max=0.917 delta=0.053) ... localized=1`.
- Diligent ground before/after diagnostic: `shadow ground diag backend=diligent ... beforeUniform ... factor=0.901 ... afterChunked=1 chunkDraws=16 ... factorRange(min=0.864 max=0.917 delta=0.053) ... localized=1`.

SV worksheet (fill from manual visual run):

| Checkpoint | SV_bgfx | SV_diligent | Parity delta \|SV_bgfx-SV_diligent\| | Backend SV rule (`>= 2`) | No-dropout rule (`SV != 0`) | Parity rule (`<= 1`) | Checkpoint result |
|---|---:|---:|---:|---|---|---|---|
| `t=6s` | pending | pending | pending | Pending | Pending | Pending | Pending |
| `t=16s` | pending | pending | pending | Pending | Pending | Pending | Pending |

VQ3 decision rules:
- pass only if `SV >= 2` at both checkpoints on both backends.
- fail if any checkpoint on either backend has full dropout (`SV = 0`).
- parity guardrail must hold at each checkpoint: `|SV_bgfx - SV_diligent| <= 1`.

VQ3 closeout decision (`2026-02-11`, current state):
- Rule checks: `Pending manual visual scoring inputs` (corrective technical evidence now confirms localized ground shadow-factor variation is active on both backends).
- Decision: `Not accepted yet (in progress)` until SV worksheet values are captured for BGFX + Diligent at `t=6s` and `t=16s`.

## Capability Gap Checklist (2026-02-10 Baseline)
| Capability | KARMA-REPO Reference | `m-rewrite` Current State | Gap |
|---|---|---|---|
| Material shading pipeline (PBR fields consumed in shader path) | `KARMA-REPO/src/renderer/backends/diligent/backend_init.cpp`, `KARMA-REPO/src/renderer/backends/diligent/backend_render.cpp` | R1 wired shared material semantics, and R5 now adds shared BRDF-proxy material-lighting resolution consumed identically by BGFX+Diligent; full physically based BRDF/shader-model parity is still pending. | Medium |
| Shadow map pass + sampling | `KARMA-REPO/src/renderer/backends/diligent/backend_init.cpp`, `KARMA-REPO/src/renderer/backends/diligent/backend_render.cpp` | R2 landed one engine-owned directional-light shadow contract slice with shared bounded CPU shadow-map generation + PCF sampling consumed by BGFX+Diligent; full GPU depth-pass/shader-sampled parity is still pending. | Medium |
| Environment map / skybox / IBL | `KARMA-REPO/src/renderer/backends/diligent/backend_render.cpp` | R3 landed engine-owned environment lighting semantics (sky/ground ambient IBL proxy + sky clear policy + specular boost) with BGFX+Diligent parity; full cubemap/image-backed environment map lifecycle/sampling is still pending. | Medium |
| Rich material texture set lifecycle (beyond albedo) | `KARMA-REPO/src/renderer/backends/diligent/backend_mesh.cpp`, `KARMA-REPO/src/renderer/backends/diligent/backend_textures.cpp` | R5 improves representative sampling + channel-normalized RGBA expansion; R6 adds shared bounded normal/occlusion semantics; R7 adds deterministic multi-sample stability treatment; R8 adds normal/detail policy refinement plus occlusion edge-case handling with assertion-backed coverage consumed by BGFX+Diligent material lighting. Full texture-set lifecycle and shader sampling breadth still incomplete. | Medium |
| Debug line rendering path | `KARMA-REPO/src/renderer/backends/diligent/backend_render.cpp` | R4 landed one engine-owned debug-line contract path with shared line semantics consumed by BGFX+Diligent; broader visual-regression parity coverage is still pending. | Low |
| Renderer-driven UI draw path internals | `KARMA-REPO/src/renderer/backends/diligent/backend_ui.cpp` | UI is handled through rewrite UI systems; renderer-specific parity details are intentionally selective. | Medium |
| Diligent Linux X11 swapchain stability | `m-rewrite/src/engine/renderer/backends/diligent/backend_diligent.cpp` | Explicit TODO notes current X11 init issue; Wayland path works. | Medium |

## Priority Slice Queue
1. R2: Shadow-capable directional light contract slice (engine-owned contract + BGFX/Diligent parity behavior). `Completed 2026-02-10` (bounded CPU slice + shared contract validation path).
2. R3: Environment/skybox/IBL slice behind engine contract. `Completed 2026-02-10` (shared environment semantics + clear policy; non-cubemap slice).
3. R4: Renderer utilities parity slice (line rendering and/or other debug-draw needs as contract-safe). `Completed 2026-02-10` (debug-line contract path + BGFX/Diligent consumption).
4. Follow-up material fidelity: evolve R1 semantics toward fuller BRDF/texture-path parity without backend leakage. `Completed 2026-02-10`
5. Follow-up texture-set semantics: normal/occlusion bounded contract parity with BGFX+Diligent consumption. `Completed 2026-02-10`
6. Next texture-set/shader-sampling increment: broaden bounded texture-set coverage (for example normal/occlusion stability under higher-frequency sample patterns and clamp-policy assertions) while preserving accepted R1/R2/R3/R4/R5/R6 behavior. `Accepted 2026-02-10`
7. Next texture-set/shader-sampling increment: continue bounded texture-set coverage (for example normal/detail policy refinement and occlusion edge-case parity) while preserving accepted R1/R2/R3/R4/R5/R6/R7 behavior. `Accepted 2026-02-10`
8. Next texture-set/shader-sampling increment: continue bounded texture-set coverage (for example normal/detail response tuning and occlusion integration edge behavior) while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8 behavior. `Accepted 2026-02-10`
9. R10 bounded texture-set lifecycle increment: wire normal/occlusion asset ingestion through engine material contract paths and lock parity assertions while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9 behavior. `Accepted 2026-02-10`
10. R11 bounded shader-path consumption slice: consume ingested normal/occlusion lifecycle texture resources in backend shader paths with BGFX/Diligent parity assertions while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10 behavior. `Accepted 2026-02-10`
11. R12 bounded multi-sampler shader-input contract slice: add direct normal/occlusion shader-input plumbing with explicit R11 composite fallback and parity assertions across BGFX + Diligent while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11 behavior. `Accepted 2026-02-10`
12. R13 runtime shader-binding enablement slice: turn on live direct multi-sampler normal/occlusion shader consumption in BGFX + Diligent with deterministic fallback guardrails and parity assertions while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12 behavior. `Accepted 2026-02-10`
13. R14 direct-path observability and shader-asset alignment slice: add runtime observability/guardrails that verify direct-path shader capability and asset binary alignment (especially BGFX shader binaries) while preserving deterministic fallback behavior and accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13 behavior. `Accepted 2026-02-10`
14. R15 direct-path readiness recovery slice: resolve Diligent sampler-contract availability false-negative(s) without relaxing R14 safety gates, and add explicit path-readiness assertions to lock intended direct-vs-fallback behavior under both backends while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14 behavior. `Accepted 2026-02-11`
15. R16 direct-path observability hardening slice: move direct-vs-fallback invariant checks from runtime trace-only observability into explicit assertion-backed renderer tests (with Diligent material-contract readiness expectations) while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14/R15 behavior. `Accepted 2026-02-11`
16. R17 direct-path observability parity slice: apply shared observability helper usage + assertion-backed readiness/invariant coverage for BGFX direct/fallback paths to match Diligent-side contract coverage while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14/R15/R16 behavior. `Accepted 2026-02-11`
17. R18 BGFX direct-path guardrail packaging-resilience slice: preserve shader-alignment safety while avoiding false direct-path disable in source-absent packaging workflows (keep strict stale-vs-source checks when source exists; define explicit source-absent readiness/disable reasons and assertion-backed coverage) while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14/R15/R16/R17 behavior. `Accepted 2026-02-11`
18. R19 BGFX deployed-shader binary integrity hardening slice: add explicit deployed-binary integrity contract checks (manifest/hash/signature-style validation) for source-absent direct-path readiness, with deterministic disable reasons and assertion-backed coverage, while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14/R15/R16/R17/R18 behavior. `Accepted 2026-02-11`
19. R20 BGFX deployed-shader integrity-manifest contract hardening slice: add versioned `.integrity` manifest schema parsing (algorithm/version token + deterministic parse-failure reasons) with assertion-backed coverage for source-absent readiness/disable propagation, while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14/R15/R16/R17/R18/R19 behavior. `Accepted 2026-02-11`
20. R21 BGFX integrity-manifest shape-validation hardening slice: add strict manifest shape validation (duplicate-key/unknown-key/invalid-token handling) with deterministic disable reasons and assertion-backed propagation coverage for source-absent readiness, while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14/R15/R16/R17/R18/R19/R20 behavior. `Accepted 2026-02-11`
21. R22 BGFX source-absent integrity canonicalization hardening slice: add deterministic manifest canonicalization rules (including normalized line endings/whitespace handling for hash contract inputs) with explicit disable reasons and assertion-backed coverage, while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14/R15/R16/R17/R18/R19/R20/R21 behavior. `Accepted 2026-02-11`
22. R23 BGFX source-absent integrity signed-envelope/trust-chain planning hardening slice: codify contract guardrails and deterministic disable-reason propagation for optional signed-envelope metadata (without enabling cryptographic verification yet), while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14/R15/R16/R17/R18/R19/R20/R21/R22 behavior. `Accepted 2026-02-11`
23. R24 BGFX source-absent integrity verification-enablement slice: add signed-envelope verification plumbing and trust-root policy checks for source-absent readiness, with deterministic disable reasons preserved when verification prerequisites are unavailable, while preserving accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14/R15/R16/R17/R18/R19/R20/R21/R22/R23 behavior. `Accepted 2026-02-11`
24. R25 BGFX source-absent integrity signature-model hardening slice: codify canonical asymmetric-signature verification contract inputs/validation boundaries for signed-envelope metadata (without introducing external trust-store rotation tooling yet), preserving deterministic disable reasons and accepted R1/R2/R3/R4/R5/R6/R7/R8/R9/R10/R11/R12/R13/R14/R15/R16/R17/R18/R19/R20/R21/R22/R23/R24 behavior. `Accepted 2026-02-11` (canonical verification-input boundary checks + deterministic `...verification_inputs_invalid` propagation).
25. VQ1 visual-quality diagnostics baseline slice: capture deterministic repro settings and concrete acceptance thresholds for distant texture aliasing/grain and obvious shadow caster/receiver visibility in roaming scenes. `Accepted 2026-02-11` (deterministic repro recipe + phased camera-path scoring rubric + explicit VQ2/VQ3 thresholds).
26. VQ2 texture minification quality slice: add mip-chain generation/upload plus trilinear/anisotropic sampler policy across BGFX + Diligent material texture paths (including fallback/composite paths) with parity guardrails. `Accepted 2026-02-11` (manual worksheet TA scores: BGFX `0/0`, Diligent `0/0`, parity deltas `0/0`, all VQ2 rules passed).
27. VQ3 visible directional shadowing slice: evolve bounded directional shadow path toward backend-parity projected shadow-map pass with per-pixel depth sampling (bias + bounded PCF), preserving deterministic fallback policy and contract boundaries. `Accepted 2026-02-17` (active implementation moved to `docs/projects/karma-lighting-shadow-parity.md`; operator confirmed contact-edge closeout on locked `gpu_default` defaults; mirror follow-up accepted outcomes into this parity ledger).
28. VQ4 visual regression guardrail slice: add deterministic visual-quality assertions/metrics and align wrapper/testing docs with new renderer quality expectations. `Queued 2026-02-11`
29. R26-A baseline matrix slice (`shared unblocker`): record roaming-mode FPS/frame-time matrix across BGFX+Diligent with shadows on/off, including trace evidence for top CPU bottlenecks. `Queued 2026-02-14`
30. R26-B GPU shadow parity intake slice (`KARMA intake`): replace rewrite default CPU shadow-map generation/sampling path with GPU-pass-based shadow map generation and GPU sampling parity across active backends. `Queued 2026-02-14`
31. R26-C legacy renderer intake slice (`m-dev parity`): audit `m-dev` renderer offload techniques, classify adopt-now/later/reject, and attach concrete implementation follow-ups for adopt-now items. `Queued 2026-02-14`
32. R26-D renderer config-surface policy slice (`shared unblocker`): ensure newly introduced performance-sensitive renderer techniques are exposed as bounded config options (or explicitly documented as fixed-policy decisions) in `data/client/config.json` and runtime plumbing. `In progress 2026-02-17` (`triangleBudget` + receiver/normal/raster bias controls landed; bounded sweep/default-lock evidence captured; operator visual closeout accepted`)

## Active Specialist Packet (R2)
```text
Read in order:
1) AGENTS.md
2) docs/foundation/policy/execution-policy.md
3) docs/projects/AGENTS.md
4) docs/projects/ASSIGNMENTS.md
5) docs/projects/renderer-parity.md

Take ownership of: docs/projects/renderer-parity.md

Goal:
- Implement R2 directional-light shadow capability parity across BGFX and Diligent using current rewrite contracts.

Scope:
- Add one engine-owned directional-light shadow contract path usable by both backends.
- Implement one bounded shadow map generation + sampling behavior slice in BGFX and Diligent.
- Keep shadow behavior deterministic and aligned across both backends for the same scene inputs.
- Preserve completed R1 material semantics behavior.
- Add focused assertion-backed verification (renderer/backend/system level) for the new shadow semantics.
- Keep behavior engine-owned and backend-internal; no backend API leakage into game code.

Constraints:
- Stay within owned paths and interface boundaries in docs/projects/renderer-parity.md.
- No unrelated subsystem changes.
- Preserve engine/game and backend exposure boundaries from AGENTS.md.
- Use abuild.py only. Do not run raw cmake -S/-B directly.
- Use only assigned build dirs:
  - <build-dir>

Validation (required):
- cd m-rewrite && ./abuild.py -c -d <build-dir> -b bgfx,diligent
- cd m-rewrite && timeout 20s ./<build-dir>/bz3 --backend-render bgfx
- cd m-rewrite && timeout 20s ./<build-dir>/bz3 --backend-render diligent

Docs updates (required):
- Update docs/projects/renderer-parity.md Project Snapshot + status/handoff checklist.
- Update docs/projects/ASSIGNMENTS.md row (owner/status/next-task/last-update).

Handoff must include:
- files changed
- exact commands run + results
- remaining risks/open questions
- explicit statement of what was intentionally deferred to R3 (environment/IBL)
```

## R1 Acceptance Gate (Must Pass)
- Scope slice is limited to R1 material semantics:
  - `metallic_factor`, `roughness_factor`, `emissive_color`
  - `alpha_mode`, `alpha_cutoff`, `double_sided`
  - `metallic_roughness` and `emissive` texture consumption when present
- Behavior is implemented in both BGFX and Diligent backends for the same contract fields (no one-backend-only path).
- Deterministic verification exists for each semantics group:
  - either direct renderer tests, or stable assertion-backed checks at backend/system level.
- Required validation commands are run exactly as listed in this project file and included in handoff with outcomes.
- No backend API leakage into game code:
  - no backend-specific includes/types/functions added under `src/game/*` or game-facing engine contracts.
- No architectural drift:
  - capability parity is delivered without mirroring KARMA-REPO file layout.
- Docs closure is complete:
  - `docs/projects/renderer-parity.md` snapshot/status/checklist updated,
  - `docs/projects/ASSIGNMENTS.md` row updated with next concrete renderer task and current date.

## R1 Handoff Review Rubric (Overseer)
- `Accept` only when all acceptance-gate items above are satisfied with explicit evidence in handoff.
- `Revise` when any of the following occurs:
  - semantics are only partially wired in one backend,
  - tests are missing or non-deterministic,
  - runtime commands were not run or output is ambiguous,
  - unrelated subsystem edits are mixed into the slice.
- Required handoff evidence format:
  - file list with paths,
  - exact commands with pass/fail outcomes,
  - concise residual risks/open questions,
  - explicit statement of what was intentionally not changed.
- Escalate to integration-owner decision if:
  - implementing one material semantic requires widening renderer contract surface,
  - BGFX and Diligent cannot match behavior without a documented policy choice.

## R2 Pre-Scope Boundaries (Post-R1)
- R2 is restricted to a shadow-capable directional-light contract slice.
- R2 should not include environment/IBL work (reserved for R3).
- R2 should not introduce backend API exposure into game paths.
- R2 should land one capability slice with BGFX + Diligent validation in isolated renderer build dirs.

## Open Questions
- Which missing features are highest impact for gameplay/UI parity (for example: environment, shadows, material coverage)?
- Are any missing capabilities blocked by scene/content pipeline assumptions outside renderer code?
- Should dedicated renderer assertions for R1 semantics groups be landed inside R2 or delegated to `docs/foundation/governance/engine-backend-testing.md` immediately after R2?
- Should anisotropic filtering default above `1x` globally or per material class once VQ2 begins?
- Which visual regression strategy is preferred for CI after VQ4 (image snapshots vs numeric metrics vs trace-derived proxies)?

## Handoff Checklist
- [x] Behavior checked on both backends.
- [x] No backend API leakage into game code.
- [x] Missing capability inventory updated.
- [x] R2 directional-light shadow contract + bounded map generation/sampling landed for BGFX+Diligent.
- [x] R3 environment/IBL contract slice landed for BGFX+Diligent.
- [x] R4 debug-line rendering contract slice landed for BGFX+Diligent.
- [x] R5 material-fidelity follow-up slice accepted for BGFX+Diligent (shared BRDF proxy + texture-path improvements).
- [x] R6 normal/occlusion texture-set semantics slice accepted for BGFX+Diligent (bounded contract + validation).
- [x] R7 texture-set stability follow-up slice accepted for BGFX+Diligent (higher-frequency stability + clamp-policy assertions).
- [x] R8 normal/detail policy refinement + occlusion edge-case parity slice accepted for BGFX+Diligent.
- [x] R9 normal/detail response tuning + occlusion integration edge behavior slice accepted for BGFX+Diligent.
- [x] R10 bounded texture-set lifecycle ingestion slice accepted for BGFX+Diligent.
- [x] R11 bounded shader-path lifecycle-consumption slice accepted for BGFX+Diligent.
- [x] R12 bounded multi-sampler shader-input contract slice accepted for BGFX+Diligent.
- [x] R13 runtime direct-path multi-sampler shader-binding slice accepted for BGFX+Diligent.
- [x] R14 direct-path observability + shader-asset alignment guardrail slice accepted for BGFX+Diligent.
- [x] R15 direct-path readiness recovery slice accepted for BGFX+Diligent.
- [x] R16 direct-path observability hardening slice accepted for BGFX+Diligent.
- [x] R17 BGFX direct-path observability parity slice accepted for BGFX+Diligent.
- [x] R18 BGFX direct-path guardrail packaging-resilience slice accepted for BGFX+Diligent.
- [x] R19 BGFX deployed-shader binary integrity hardening slice accepted for BGFX+Diligent.
- [x] R20 BGFX deployed-shader integrity-manifest contract hardening slice accepted for BGFX+Diligent.
- [x] R21 BGFX integrity-manifest shape-validation hardening slice accepted for BGFX+Diligent.
- [x] R22 BGFX source-absent integrity canonicalization hardening slice accepted for BGFX+Diligent.
- [x] R23 BGFX source-absent integrity signed-envelope/trust-chain planning hardening slice accepted for BGFX+Diligent.
- [x] R24 BGFX source-absent integrity verification-enablement slice accepted for BGFX+Diligent.
- [x] R25 BGFX source-absent integrity signature-model hardening slice completed and accepted.
- [x] VQ1 diagnostics baseline completed with deterministic repro settings + acceptance thresholds.
- [x] VQ2 kickoff landed: shared mip-chain generation/upload + sampler-policy parity wiring for BGFX/Diligent is in place (acceptance closeout still pending).
- [x] VQ2 evidence runner/worksheet is in place (`scripts/run-renderer-vq2-evidence.sh`) for deterministic closeout capture.
- [x] VQ2 texture minification quality improvements completed with BGFX/Diligent parity. (`Accepted 2026-02-11`: TA scores BGFX `0/0`, Diligent `0/0`, parity deltas `0/0`.)
- [x] VQ3 visible directional shadowing improvements completed with BGFX/Diligent parity. (`Accepted 2026-02-17`: operator confirmed contact-edge closeout for locked `gpu_default` defaults; no follow-up adjustment required in this parity ledger slice.)
- [ ] VQ4 deterministic visual regression guardrails + wrapper/docs updates completed.
- [x] Post-R3 deferrals are explicitly documented.
