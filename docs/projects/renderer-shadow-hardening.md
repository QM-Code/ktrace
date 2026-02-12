# Renderer Shadow Hardening

## Project Snapshot
- Current owner: `codex`
- Status: `in progress (P0; sandbox + runtime shadows now visible; alignment/quality stabilization pending)`
- Immediate next task: execute Slice 1 acceptance capture (sandbox + bz3 baseline evidence with fixed camera checkpoints) before further algorithm churn.
- Strategic alignment track: `shared unblocker (m-dev visual parity + KARMA-REPO capability intake)`
- Validation gate: required renderer builds + sandbox evidence run + runtime smoke + docs lint must pass for each accepted slice.

## Mission
Deliver stable, correctly aligned directional shadows in both `renderer_shadow_sandbox` and `bz3` across BGFX and Diligent, then harden toward a robust shadow model (distance stability, bias control, and predictable quality/perf tradeoffs).

## Why This Is Separate
Shadow defects are currently the primary visual blocker and are cross-cutting (renderer internals, shaders, config, diagnostics). Keeping this in a dedicated project avoids churn/noise in broader `renderer-parity.md` execution while preserving explicit acceptance gates.

## Owned Paths
- `docs/projects/renderer-shadow-hardening.md`
- `src/engine/renderer/backends/directional_shadow_internal.hpp`
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
  - `docs/projects/testing-ci-docs.md`
  - `docs/projects/core-engine-infrastructure.md` (renderer contract notes)

## Non-Goals
- Do not change gameplay/network semantics.
- Do not expand into unrelated renderer materials/features unless directly required for shadow correctness.
- Do not mirror KARMA file layout verbatim; intake is behavior/contract-level only.

## Validation
From `m-rewrite/`:

```bash
./bzbuild.py -c build-sdl3-bgfx-physx-imgui-sdl3audio
./bzbuild.py -c build-sdl3-diligent-physx-imgui-sdl3audio
./scripts/run-renderer-shadow-sandbox.sh 20 16 20
timeout 20s ./build-sdl3-bgfx-physx-imgui-sdl3audio/bz3 --backend-render bgfx --backend-ui imgui
timeout 20s ./build-sdl3-diligent-physx-imgui-sdl3audio/bz3 --backend-render diligent --backend-ui imgui
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `render.system`
- `render.bgfx`
- `render.diligent`
- `render.mesh`
- `engine.app`

## Build/Run Commands
From `m-rewrite/`:

```bash
./bzbuild.py -c build-sdl3-bgfx-physx-imgui-sdl3audio
./bzbuild.py -c build-sdl3-diligent-physx-imgui-sdl3audio
./scripts/run-renderer-shadow-sandbox.sh 20 16 20
```

## First Session Checklist
1. Confirm current baseline visuals in sandbox and bz3 with screenshots at fixed camera poses.
2. Capture `run-renderer-shadow-sandbox.sh` logs and verify `map_ready=1` for both backends.
3. Implement only one slice from the queue below.
4. Re-run full validation gates (both renderer build dirs + sandbox run + bz3 smoke).
5. Update this file and `docs/projects/ASSIGNMENTS.md` in the same handoff.

## Completed So Far (2026-02-11 to 2026-02-12)
- Added `renderer_shadow_sandbox` and wrapper runner (`scripts/run-renderer-shadow-sandbox.sh`) for deterministic, isolated shadow debugging.
- Corrected sandbox geometry/winding orientation issues so ground + casters render consistently during orbit.
- Restored visible shadowing path in sandbox (BGFX + Diligent) and brought bz3 from no-shadow state to visible/stable-at-distance baseline behavior in the latest operator checks.
- Repaired major regression loops (inside-out geometry view, missing ground, no-shadow path) and stabilized current state enough for iterative visual checks.

## Remaining Defects (Operator-Observed)
1. Shadow/receiver edge alignment still drifts (shadow edge does not land cleanly on caster base in close view).
2. Sandbox shadow edges remain visibly blocky/aliased in low-frequency regions.
3. Shading continuity around box faces is improved but still has occasional edge artifacts.

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
| Bias controls | Constant + raster + receiver + normal controls exposed end-to-end | Constant bias + slope-aware term only; no receiver/normal/raster control surface | Add receiver/normal + raster bias controls and propagate through rewrite contracts |
| Per-pixel backend parity | Diligent path performs per-pixel shadow compare sampling | BGFX performs per-pixel shadow-map sampling; Diligent currently applies CPU-evaluated per-draw shadow factor | Move Diligent to per-pixel shadow-map sampling for parity and edge correctness |
| Shader sampling | Comparison linear shadow sampling + cascade blending | Manual PCF filtering works but remains blocky in sandbox; edge alignment artifacts remain | Add quality ladder (map size/pcf/bias presets), then tighten receiver projection/sampling |
| Runtime config plumbing | Engine config + debug overlay shadow controls are wired to renderer (`setShadowSettings`) | `EngineConfig.shadow_map_size` / `shadow_pcf_radius` exist but are not currently wired into active runtime shadow path; backend interface has no `setShadowSettings` contract | Either wire shadow controls through rewrite renderer contracts or remove dead config knobs to avoid false expectations |
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
| Shadow topology | 4-cascade texture-array CSM in Diligent (`kShadowCascadeCount = 4`) | Single directional shadow map (`directional_shadow_internal.hpp`) | Single-map is simpler but less robust across near/far ranges. |
| Per-pixel shadowing | Diligent pixel shader performs compare-sampled shadow lookup | BGFX does per-pixel shadow sampling (`data/bgfx/shaders/mesh/fs_mesh.sc`), Diligent uses per-draw CPU receiver factor (`backend_diligent.cpp`) | Backend mismatch drives visible parity drift and edge differences. |
| Bias model | Constant + raster + receiver + normal controls are runtime-plumbed | Mostly constant + slope-aware behavior in shared path; no renderer-level receiver/normal/raster control surface | Reduced ability to tune acne vs peter-panning and contact quality. |
| Runtime control plane | `setShadowSettings` contract exists and is called from debug overlay | Backend interface has no `setShadowSettings`; only light struct fields and CLI/sandbox knobs | Harder to iterate safely in live runtime and easier to end up with dead/no-op config fields. |
| Cascade seam handling | Cascade transition blending in shader | No cascades in rewrite yet | Important once rewrite moves beyond single-map limitations. |
| Distance stability policy | Texel-snapped cascade centers + split-aware fitting | Single-map view expansion + snapping landed; current distance behavior improved and must stay gated | Stability can regress quickly without persistent acceptance checks. |
| Tooling feedback loop | Runtime debug UI + documented defaults | Sandbox/script diagnostics available; no in-runtime shadow tuning UI | Slower iteration and weaker operator-guided tuning loops. |

### Practical Intake Guidance For Future Specialist
1. Treat this discovery section as the upstream source of truth for shadow-intake context.
2. Prioritize backend parity first: move rewrite Diligent from per-draw factor to per-pixel shadow-map sampling behavior.
3. Add the missing bias control surface (receiver/normal/raster) through rewrite-owned contracts before CSM expansion.
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
| Diligent per-pixel shadow-map sampling parity | Not implemented in Diligent; BGFX uses per-pixel sampling | Implement now (P0) | Current backend mismatch is a direct source of edge/alignment divergence | Implemented: closer BGFX/Diligent parity and cleaner edge behavior. Skipped: persistent backend-specific artifacts. |
| Full bias model (receiver + normal + raster + constant) | Partial; constant + slope-aware behavior only | Implement (high priority) | Needed to tune acne vs peter-panning without over-blurring | Implemented: more controllable, cleaner contact and reduced artifacts. Skipped: brittle tuning and recurring edge defects. |
| Sampling quality ladder (map size/pcf/bias presets) | Basic manual controls; still blocky in sandbox | Implement (bounded) | Needed for deterministic quality/perf tuning and reproducible acceptance checks | Implemented: reduced blockiness and predictable perf tiers. Skipped: noisy ad hoc tuning and unstable visual baselines. |
| Runtime shadow config plumbing (`setShadowSettings`-style path) | Config fields exist, runtime wiring incomplete | Implement (preferred) | Dead knobs create false expectations and hamper repeatable testing | Implemented: reliable operator controls and reproducible experiments. Skipped without removal: confusing no-op settings and test ambiguity. |
| Runtime shadow debug overlay controls | Not implemented (CLI/log-only today) | Defer (add minimal panel after core parity) | Keep initial scope on correctness first, then optimize iteration speed | Implemented: faster triage and less turnaround per visual check. Skipped: slower manual loop but functionally acceptable short-term. |
| KARMA file-layout mirroring / full surface clone | Not applicable | Skip | Project rule is behavior/contract intake, not source layout cloning | Skipped: preserves rewrite boundaries and maintainability. |

### Intake recommendations (priority)
1. **Near-term (must-have):** Diligent per-pixel shadow-path parity + receiver/normal/raster bias model.
2. **Mid-term (high value):** cascaded shadow maps with blend transition and per-cascade fitting.
3. **Support:** runtime shadow tuning controls for rapid operator confirmation and faster defect isolation.

## Slice Queue
1. Slice 1: Baseline lock + deterministic evidence
   - Record canonical sandbox + bz3 screenshots/log tails for both backends (same camera/sun checkpoints).
   - Acceptance: no code changes; reproducible evidence package committed to docs notes.
2. Slice 2: Single-map alignment hardening
   - Tighten receiver projection math to remove base-edge offset artifacts.
   - Acceptance: close-view caster-edge alignment improved in sandbox and bz3, no regressions.
3. Slice 3: Bias model intake (KARMA-inspired)
   - Add receiver/normal/raster bias parameters through rewrite contracts and backend paths.
   - Acceptance: reduced acne/peter-panning tradeoff at identical map size and pcf settings.
4. Slice 4: Distance stability hardening
   - Improve shadow view fitting/snap strategy for far-field stability.
   - Acceptance: distant bz3 casters retain visible shadows at baseline camera pose.
5. Slice 5: Cascaded shadows intake (bounded)
   - Introduce 2-4 cascade path with transition blend, preserving rewrite-owned API boundaries.
   - Acceptance: improved far/near consistency without major perf regression.

## Open Questions
- Should rewrite expose shadow tuning in ImGui debug UI immediately (Slice 3) or keep CLI-only until CSM lands?
- Should cascade count be fixed (e.g. 4 like KARMA) or configurable with a hard upper bound?
- Should sandbox remain pure synthetic geometry, or also load a minimal world asset variant for parity checks?

## Handoff Checklist
- [ ] Slice scope completed
- [ ] Validation commands run and summarized
- [ ] Sandbox + bz3 visual evidence captured
- [ ] This file updated
- [ ] `docs/projects/ASSIGNMENTS.md` updated
- [ ] Risks/open questions listed
