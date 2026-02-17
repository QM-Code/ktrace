# Renderer Shadow Hardening

## Project Snapshot
- Current owner: `codex`
- Status: `queued/superseded (active lighting/shadow intake moved to docs/projects/karma-lighting-shadow-parity.md on 2026-02-17)`
- Immediate next task: do not start new work here; use `docs/projects/karma-lighting-shadow-parity.md` as the canonical execution track and archive this file after final carry-over verification.
- Strategic alignment track: `shared unblocker (m-dev visual parity + KARMA-REPO capability intake)`
- Validation gate: required renderer builds + sandbox evidence run + runtime smoke + docs lint must pass for each accepted slice.

## Mission
Deliver stable, correctly aligned directional shadows in both `renderer_shadow_sandbox` and `bz3` across BGFX and Diligent, then harden toward a robust shadow model (distance stability, bias control, and predictable quality/perf tradeoffs).

## Why This Is Separate
Shadow defects are currently the primary visual blocker and are cross-cutting (renderer internals, shaders, config, diagnostics). Keeping this in a dedicated project avoids churn/noise in broader `renderer-parity.md` execution while preserving explicit acceptance gates.

## Owned Paths
- `docs/projects/renderer-shadow-hardening.md`
- `src/engine/renderer/backends/internal/directional_shadow.hpp`
- `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
- `src/engine/renderer/backends/diligent/backend_diligent.cpp`
- `data/bgfx/shaders/mesh/fs_mesh.sc`
- `src/engine/renderer/tests/renderer_shadow_sandbox.cpp`
- `scripts/run-renderer-shadow-sandbox.sh`
- `docs/projects/renderer-parity.md` (status cross-reference only)
- `docs/projects/ASSIGNMENTS.md`

## Interface Boundaries
- Inputs consumed:
  - directional light contract (`renderer::DirectionalLightData`) from scene/render system.
  - visual-quality acceptance rubric from `docs/projects/renderer-parity.md` (VQ3 shadow visibility checks).
- Outputs exposed:
  - stable backend-parity shadow behavior in sandbox and bz3.
  - deterministic diagnostics for operator-visible regression detection.
- Coordinate before changing:
  - `docs/projects/renderer-parity.md`
  - `docs/foundation/governance/testing-ci-governance.md`
  - `docs/foundation/architecture/core-engine-contracts.md` (renderer contract notes)

## Non-Goals
- Do not change gameplay/network semantics.
- Do not expand into unrelated renderer materials/features unless directly required for shadow correctness.
- Do not mirror KARMA file layout verbatim; intake is behavior/contract-level only.

## Validation
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir>
./scripts/run-renderer-shadow-sandbox.sh 20 16 20
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render bgfx -d ./data --strict-config=true --config data/client/config.json -v -t engine.sim,render.system,render.bgfx
timeout -k 2s 20s ./<build-dir>/bz3 --backend-render diligent -d ./data --strict-config=true --config data/client/config.json -v -t engine.sim,render.system,render.diligent
./scripts/run-renderer-shadow-bias-sweep.sh 16
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `render.system`
- `render.bgfx`
- `render.diligent`
- `engine.sim`
- `render.mesh`

## Build/Run Commands
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir>
./scripts/run-renderer-shadow-sandbox.sh 20 16 20
./scripts/run-renderer-shadow-bias-sweep.sh 16
```

## First Session Checklist
1. Confirm `gpu_default` runtime traces in both backends (`gpu shadow pass ... attachment=depth`).
2. Confirm `render.system` reports locked bias defaults (`recv=0.08 norm=0.35 rasterDepth=0.0000 rasterSlope=0.0000`).
3. Capture one sandbox and one roaming checkpoint for each backend.
4. Open at most one bounded visual follow-up (contact-edge/aliasing) before broader algorithm changes.
5. Re-run validation gates and update this file plus `docs/projects/ASSIGNMENTS.md` in the same handoff.

## Completed So Far (2026-02-11 to 2026-02-16)
- Brought sandbox + roaming runtime from no-shadow regressions to backend-parity visible shadows.
- Landed engine-owned shadow execution policy (`cpu_reference`, `gpu_default`) with deterministic fallback tracing.
- Landed GPU shadow pass implementation in both BGFX and Diligent with active trace evidence (`gpu shadow pass size=... draws=...`).
- Landed depth-attachment GPU path in both backends (`attachment=depth` with explicit fallback behavior).
- Landed shared bias-model controls and runtime config plumbing:
  - `receiverBiasScale`, `normalBiasScale`, `rasterDepthBias`, `rasterSlopeBias`.
- Added bounded sweep automation (`scripts/run-renderer-shadow-bias-sweep.sh`) and locked production defaults from sweep evidence.

## Remaining Defects (Operator-Observed)
1. Close-view contact-edge alignment still needs final visual signoff in roaming mode.
2. Low-frequency blockiness/aliasing remains visible at current map-size/PCF defaults in some checkpoints.
3. Diligent non-interactive screenshot capture remains environment-blocked on X11 (`VK_ERROR_INITIALIZATION_FAILED`), so visual proof still depends on operator desktop captures.

## Regression Watch (Resolved But Must Stay Green)
1. Prior bz3 distance-dropout behavior appears resolved in the latest operator screenshots (shadows now persist at distance in baseline view).
2. Keep explicit regression checks in every slice because this path has regressed before during shadow-fitting changes.

## KARMA-REPO Findings (Thorough Pass, 2026-02-12)
Reference root: `/home/karmak/dev/bz3-rewrite/KARMA-REPO`

### Confirmed capabilities implemented in KARMA-REPO
1. Cascaded shadow maps (4 cascades) with split blending:
   - `include/karma/renderer/backends/diligent/backend.hpp`
   - `src/renderer/backends/diligent/backend_render.cpp`
   - `src/renderer/backends/diligent/backend_init.cpp`
2. Texel-snapped stable cascade placement (camera-motion stability):
   - `src/renderer/backends/diligent/backend_render.cpp`
3. Comparison-sampler PCF with cascade-aware sampling in shader path:
   - `src/renderer/backends/diligent/backend_init.cpp`
4. Multi-term bias model (constant + receiver-plane + normal/slope + raster depth/slope bias):
   - `include/karma/app/engine_app.h`
   - `src/renderer/backends/diligent/backend_init.cpp`
   - `src/renderer/backends/diligent/backend_render.cpp`
5. Runtime tuning controls in debug overlay (map size, bias, PCF, raster bias, receiver bias, normal bias):
   - `src/debug/debug_overlay.cpp`
6. Proven defaults in example config path (`shadow_map_size=2048`, `shadow_pcf_radius=1`, `shadow_bias=0.0006`):
   - `examples/main.cpp`

### Rewrite gap assessment vs KARMA-REPO
| Area | KARMA-REPO state | m-rewrite state | Action |
|---|---|---|---|
| Shadow topology | 4-cascade CSM with blend regions | Single-map directional shadow projection | Plan staged CSM intake after current single-map alignment is locked |
| Stability against camera motion | Texel-snapped cascade centers in stable light space | Single-map texel snapping + view-footprint/depth expansion are landed; prior distance-dropout appears resolved but is regression-prone | Keep stability regression gates active and fold robust fitting into cascade intake plan |
| Bias controls | Constant + raster + receiver + normal controls exposed end-to-end | Shared bias control surface is now runtime-wired (`receiver/normal/raster` + base bias) in BGFX and Diligent | Keep defaults locked (`0.08/0.35/0.0000/0.0000`) and tune only with bounded sweep evidence |
| Per-pixel backend parity | Diligent path performs per-pixel shadow compare sampling | BGFX and Diligent both run per-pixel GPU shadow-map sampling in `gpu_default`; `cpu_reference` retained as deterministic fallback | Keep parity checks centered on `gpu_default` visual checkpoints and trace invariants |
| Shader sampling | Comparison linear shadow sampling + cascade blending | Manual PCF filtering works but remains blocky in sandbox; edge alignment artifacts remain | Add quality ladder (map size/pcf/bias presets), then tighten receiver projection/sampling |
| Runtime config plumbing | Engine config + debug overlay shadow controls are wired to renderer (`setShadowSettings`) | Rewrite runtime now wires `executionMode`, `triangleBudget`, and bias controls through engine-owned contracts and `data/client/config.json` | Continue policy: performance-sensitive knobs must be runtime-wired or explicitly fixed-policy documented |
| Tooling | Debug UI shadow tuning live in runtime | CLI/script tuning + logs; no in-runtime tuning panel | Add minimal runtime shadow debug panel in rewrite UI path (bounded scope) |

### Discovery Status (Do Not Re-Run Archaeology)
`KARMA-REPO` shadow/lighting discovery phase is complete for the known shadow-intake window. The specialist assigned to this project should use the findings below as starting context, not repeat commit-mining unless new upstream commits land.

### KARMA Commit Timeline (Shadow/Lighting Evolution)
| Commit | Stage | Extracted technical signal |
|---|---|---|
| `e656e02` | Early directional-shadow baseline | Establishes directional-light + shadow configuration plumbing (`setShadowSettings` path and config defaults) and core shadow render-path framing. |
| `4484618` | Single-map shadow pipeline maturation | Adds/solidifies shadow map + sampler resource binding in Diligent render path and light-view fitting groundwork. |
| `ed9c006` | Lighting response integration | Tightens lit shading response and keeps directional shadow usage in active render path while model/material pipeline evolves. |
| `ad0e279` | Runtime tuning + observability pass | Adds debug-overlay controls for scene/light properties and continues hardening renderer-side shadow setup behavior. |
| `521f60c` | Stabilization + defaults lock | Introduces expanded bias controls (constant + raster + receiver + normal), wires them end-to-end, and sets known-good runtime defaults. |
| `9b517f1` | CSM completion | Lands 4-cascade texture-array shadows with split blending, texel snapping stabilization, and comparison-sampler PCF in Diligent. |

### How KARMA Solved Lighting/Shadows (Extracted Patterns)
1. End-to-end control plumbing was treated as required, not optional.
   `EngineConfig` -> app startup -> renderer device interface (`setShadowSettings`) -> backend runtime.
2. Shadow tuning was exposed live in runtime UI.
   Debug overlay includes map size, constant bias, PCF radius, raster depth/slope bias, receiver bias scale, and normal bias scale.
3. Stabilization came from light-space fitting discipline.
   Stable light-view basis, texel-snapped cascade centers, conservative depth padding, and split logic tied to camera near/far.
4. Bias was multi-term and contextual.
   Constant bias + receiver-plane derivative bias + slope/normal-driven offsets + rasterizer bias controls.
5. Shadow sampling quality used hardware compare sampling in the pixel shader.
   `SampleCmpLevelZero` with optional PCF loop and cascade-transition blending reduced seam/flicker artifacts.
6. CSM was not just split rendering; it included blend policy and per-cascade metadata.
   Split lambda, cascade UV projections, cascade world-texel tracking, and transition fraction to avoid hard seam jumps.
7. Runtime defaults were explicitly documented and versioned in source/docs.
   This reduced operator ambiguity and regression churn during iteration.

### Deep Difference Map (KARMA vs m-rewrite, Code-Level)
| Topic | KARMA-REPO implementation | m-rewrite implementation | Why this matters |
|---|---|---|---|
| Shadow topology | 4-cascade texture-array CSM in Diligent (`kShadowCascadeCount = 4`) | Single directional shadow map (`directional_shadow.hpp`) | Single-map is simpler but less robust across near/far ranges. |
| Per-pixel shadowing | Diligent pixel shader performs compare-sampled shadow lookup | BGFX + Diligent now both run per-pixel GPU sampling in `gpu_default`; CPU reference remains available for deterministic fallback/debug | Backend parity gap for primary path is closed; visual-closeout risk is now quality tuning, not architecture mismatch. |
| Bias model | Constant + raster + receiver + normal controls are runtime-plumbed | Same control family is now present and runtime-plumbed in rewrite (`receiver/normal/raster` + base bias) | Tuning space now matches intended parity posture; remaining work is default quality/signoff. |
| Runtime control plane | `setShadowSettings` contract exists and is called from debug overlay | Backend interface has no `setShadowSettings`; only light struct fields and CLI/sandbox knobs | Harder to iterate safely in live runtime and easier to end up with dead/no-op config fields. |
| Cascade seam handling | Cascade transition blending in shader | No cascades in rewrite yet | Important once rewrite moves beyond single-map limitations. |
| Distance stability policy | Texel-snapped cascade centers + split-aware fitting | Single-map view expansion + snapping landed; current distance behavior improved and must stay gated | Stability can regress quickly without persistent acceptance checks. |
| Tooling feedback loop | Runtime debug UI + documented defaults | Sandbox/script diagnostics available; no in-runtime shadow tuning UI | Slower iteration and weaker operator-guided tuning loops. |

### Practical Intake Guidance For Future Specialist
1. Treat this discovery section as the upstream source of truth for shadow-intake context.
2. Backend GPU shadow parity and bias-control plumbing are already landed; do not re-open those slices unless regressions are proven.
3. Prioritize visual closeout and quality tuning using the existing bounded controls before CSM expansion.
4. Keep the single-map path green while adding features.
   Preserve current distance-stability baseline and re-run sandbox + bz3 visual checkpoints each slice.
5. Only then stage cascades.
   Bring in split/blend behavior incrementally with bounded scope and measurable acceptance gates.
6. If blocked, use KARMA as behavioral reference, not as a file-layout template.
   Mirror intent and invariants, not directory structure.

### KARMA Feature Disposition Matrix (Explicit)
| Feature | m-rewrite extent today | Decision | Why | Impact when implemented (or if skipped) |
|---|---|---|---|---|
| Cascaded shadow maps (2-4 cascades + blend) | Not implemented; current path is single-map directional shadow | Defer until single-map alignment + backend parity are locked | Avoid adding major topology churn while near-field correctness is still being stabilized | Implemented: better near/far consistency and less far-field loss. Skipped long-term: single-map quality/perf tradeoff remains constrained. |
| Texel-snapped stable fitting | Partially implemented for single-map (`view expansion + snapping` landed) | Implement (continue hardening and keep regression gates) | Prior distance instability appears resolved but has regressed before | Implemented/hardened: lower shimmer/crawl and stable distant shadows. Skipped/regressed: distance dropout risk returns. |
| Diligent per-pixel shadow-map sampling parity | Landed; `gpu_default` now runs GPU pass + per-pixel sampling in both backends | Keep green (regression gate) | Previous backend mismatch was a direct source of parity drift | Implemented: parity architecture is aligned. Regressed: backend-specific artifacts return quickly. |
| Full bias model (receiver + normal + raster + constant) | Landed and runtime-wired in shared contracts/config | Keep locked defaults + bounded sweep policy | Needed to tune acne vs peter-panning without over-blurring | Implemented: controlled tuning space exists; skip/revert would reintroduce brittle tradeoffs. |
| Sampling quality ladder (map size/pcf/bias presets) | Basic manual controls; still blocky in sandbox | Implement (bounded) | Needed for deterministic quality/perf tuning and reproducible acceptance checks | If implemented: reduced blockiness and predictable perf tiers. If skipped: noisy ad hoc tuning and unstable visual baselines. |
| Runtime shadow config plumbing (`setShadowSettings`-style path) | Runtime wiring for `executionMode`, `triangleBudget`, and bias controls is active | Keep policy enforced | Dead knobs create false expectations and hamper repeatable testing | Implemented: reliable operator controls and reproducible experiments. |
| Runtime shadow debug overlay controls | Not implemented (CLI/log-only today) | Defer (add minimal panel after core parity) | Keep initial scope on correctness first, then optimize iteration speed | If implemented: faster triage and less turnaround per visual check. If skipped: slower manual loop but functionally acceptable short-term. |
| KARMA file-layout mirroring / full surface clone | Not applicable | Skip | Project rule is behavior/contract intake, not source layout cloning | Skipped: preserves rewrite boundaries and maintainability. |

### Intake recommendations (priority)
1. **Near-term (must-have):** complete operator visual closeout for locked `gpu_default` defaults and record canonical screenshots/traces for both backends.
2. **Mid-term (high value):** tighten single-map contact-edge quality with one bounded adjustment (receiver projection/quality preset) before topology changes.
3. **Next major step:** stage cascaded shadows (2-4 cascades + blend) only after single-map closeout is accepted.

## Slice Queue
1. Slice 1: Baseline lock + deterministic evidence. `Completed`
   - Canonical sandbox/runtime traces now show GPU path active in both backends.
2. Slice 2: Single-map alignment hardening. `In progress`
   - Focus: close-view contact-edge quality and low-frequency blockiness.
3. Slice 3: Bias model intake (KARMA-inspired). `Completed`
   - Receiver/normal/raster controls landed, runtime-wired, and bounded.
4. Slice 4: Distance stability hardening. `Completed with regression watch`
   - View-fit/snap path is stable in current evidence; keep watchdog captures active.
5. Slice 5: Cascaded shadows intake (bounded). `Queued`
   - Start only after Slice 2 visual closeout is accepted.

## Open Questions
- Should rewrite expose shadow tuning in ImGui debug UI before CSM intake, or keep CLI/script-only for one more slice?
- Should cascade count be fixed (e.g. 4 like KARMA) or configurable with a hard upper bound?
- Should sandbox remain pure synthetic geometry, or also load a minimal world asset variant for parity checks?

## Handoff Checklist
- [ ] Slice scope completed
- [ ] Validation commands run and summarized
- [ ] Sandbox + bz3 visual evidence captured
- [ ] This file updated
- [ ] `docs/projects/ASSIGNMENTS.md` updated
- [ ] Risks/open questions listed
