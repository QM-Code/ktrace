# Renderer Parity

## Project Snapshot
- Current owner: `specialist-renderer-parity`
- Status: `priority/in progress (R1-R25 accepted; VQ1-VQ2 accepted; VQ3 next; VQ4 queued)`
- Immediate next task: execute VQ3 visible directional shadowing slice against the accepted VQ1 rubric while keeping VQ4 untouched/queued.
- Validation gate: both assigned renderer build dirs via `./bzbuild.py` plus both client runs listed in this file; run docs lint whenever this project doc or assignment board is updated.

## Mission
Expand renderer capability toward BGFX/Diligent parity behind stable engine contracts, then convert those capability gains into visible runtime quality improvements (shadow visibility + stable distance texture quality).

## Priority Directive (2026-02-11)
- Renderer capability integration is the top execution priority.
- R25 P0 continuity slice is complete; VQ1 diagnostics baseline and VQ2 texture minification closeout are accepted.
- Visual-quality follow-up (VQ3-VQ4) is medium-high P1 and should proceed in sequence after VQ2 acceptance.
- Prioritize deterministic visual improvements over speculative renderer feature breadth.
- This work is explicitly prioritized ahead of incremental audio/content-mount follow-up slices.
- Port capability and behavior from KARMA-REPO; do not mirror KARMA-REPO backend file organization.

## Primary Specs
- `docs/projects/core-engine-infrastructure.md` (renderer sections)
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
  - `docs/projects/core-engine-infrastructure.md` renderer sections
  - `docs/projects/engine-backend-testing.md`
  - `docs/projects/testing-ci-docs.md`

## Non-Goals
- Gameplay/network semantics.
- Physics/audio backend internals.

## Validation
From `m-rewrite/`:

```bash
./bzbuild.py -c build-sdl3-bgfx-physx-imgui-sdl3audio
./bzbuild.py -c build-sdl3-diligent-physx-imgui-sdl3audio
timeout 20s ./build-sdl3-bgfx-physx-imgui-sdl3audio/bz3 --backend-render bgfx --backend-ui imgui
timeout 20s ./build-sdl3-diligent-physx-imgui-sdl3audio/bz3 --backend-render diligent --backend-ui imgui
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `engine.app`
- `render.mesh`
- `render.bgfx`
- `render.diligent`
- `ecs.world`

## Build/Run Commands
```bash
./bzbuild.py -c build-sdl3-bgfx-physx-imgui-sdl3audio
./bzbuild.py -c build-sdl3-diligent-physx-imgui-sdl3audio
timeout 20s ./build-sdl3-bgfx-physx-imgui-sdl3audio/bz3 --backend-render bgfx --backend-ui imgui
timeout 20s ./build-sdl3-diligent-physx-imgui-sdl3audio/bz3 --backend-render diligent --backend-ui imgui
```

## First Session Checklist
1. Read renderer sections in `docs/projects/core-engine-infrastructure.md`.
2. Confirm current parity target is capability/behavior, not file mirroring.
3. Implement one capability slice at a time.
4. Validate on both render backends.
5. Update status and parity notes.

## Current Status
- `2026-02-11`: merged former `renderer-visual-quality.md` into this project as VQ1-VQ4 follow-up slices so renderer execution remains under one track/owner.
- `2026-02-11`: VQ1 diagnostics baseline accepted as shared unblocker for measurable renderer quality outcomes (deterministic repro recipe + explicit VQ2/VQ3 thresholds) while preserving backend-parity boundaries.
- `2026-02-11`: VQ2 texture minification quality slice started: shared RGBA8 mip-chain generation/upload path and anisotropic/trilinear material sampler policy are now wired for BGFX + Diligent under shared renderer contract helpers; VQ3/VQ4 remain untouched.
- `2026-02-11`: VQ2 closeout evidence attempt reran required build/run gates successfully, but TA scoring checkpoints (`t=6s`, `t=12s`) were not observable in this non-interactive environment (no visual capture/inspection path for far-field aliasing assessment); TA values and per-checkpoint parity guardrails remain unscored, so VQ2 stays in progress.
- `2026-02-11`: VQ2 evidence-unblock slice added deterministic operator runner `./scripts/run-renderer-vq2-evidence.sh` (strict config + canonical VQ1 flags + timestamped logs + backend exit-code reporting + lingering-process verification) so acceptance is pending scored TA checkpoints only.
- `2026-02-11`: VQ2 closeout accepted from manual TA worksheet inputs: BGFX `t=6s=0`, BGFX `t=12s=0`, Diligent `t=6s=0`, Diligent `t=12s=0`; parity deltas are `0` at both checkpoints and all VQ2 pass rules are satisfied.
- Cross-backend startup/rendering path is working.
- R1 is implemented: both BGFX and Diligent now consume shared material semantics for metallic/roughness/emissive/alpha/double-sided fields plus metallic-roughness and emissive texture influence when present.
- R2 is now landed: engine-owned `DirectionalLightData::shadow` contract fields are consumed by both BGFX and Diligent through one shared bounded shadow-map build/sample path (`directional_shadow_internal.hpp`) with deterministic per-draw light attenuation.
- R3 is now landed: engine-owned `EnvironmentLightingData` contract is consumed by both BGFX and Diligent through shared environment semantics resolution (`environment_lighting_internal.hpp`) including sky/ground ambient IBL proxy, roughness-aware specular boost, and sky clear-color exposure policy.
- R4 is now landed: engine-owned debug-line contract path (`DebugLineItem` + shared semantics in `debug_line_internal.hpp`) is consumed by both BGFX and Diligent with layer-aware line submission and deterministic validation coverage in `directional_shadow_contract_test.cpp`.
- R5 material-fidelity follow-up is accepted: shared material-lighting BRDF proxy (`material_lighting_internal.hpp`) now drives direct/ambient light scaling in both BGFX and Diligent, and texture-path handling now uses representative texture sampling plus normalized RGBA expansion for non-RGBA texture channel layouts.
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
timeout 20s ./build-sdl3-bgfx-physx-imgui-sdl3audio/bz3 \
  --backend-render bgfx \
  --backend-ui imgui \
  --strict-config=true \
  --config /tmp/bz3-vq1-user-config.json \
  -v -t engine.app,render.mesh,render.bgfx

# Diligent baseline capture window (20s)
timeout 20s ./build-sdl3-diligent-physx-imgui-sdl3audio/bz3 \
  --backend-render diligent \
  --backend-ui imgui \
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
- uses only `build-sdl3-bgfx-physx-imgui-sdl3audio` and `build-sdl3-diligent-physx-imgui-sdl3audio`.
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
27. VQ3 visible directional shadowing slice: evolve bounded directional shadow path toward backend-parity projected shadow-map pass with per-pixel depth sampling (bias + bounded PCF), preserving deterministic fallback policy and contract boundaries. `Next 2026-02-11`
28. VQ4 visual regression guardrail slice: add deterministic visual-quality assertions/metrics and align wrapper/testing docs with new renderer quality expectations. `Queued 2026-02-11`

## Active Specialist Packet (R2)
```text
Read in order:
1) AGENTS.md
2) docs/AGENTS.md
3) docs/projects/README.md
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
- Use bzbuild.py only. Do not run raw cmake -S/-B directly.
- Use only assigned build dirs:
  - build-sdl3-bgfx-physx-imgui-sdl3audio
  - build-sdl3-diligent-physx-imgui-sdl3audio

Validation (required):
- cd m-rewrite && ./bzbuild.py -c build-sdl3-bgfx-physx-imgui-sdl3audio
- cd m-rewrite && ./bzbuild.py -c build-sdl3-diligent-physx-imgui-sdl3audio
- cd m-rewrite && timeout 20s ./build-sdl3-bgfx-physx-imgui-sdl3audio/bz3 --backend-render bgfx --backend-ui imgui
- cd m-rewrite && timeout 20s ./build-sdl3-diligent-physx-imgui-sdl3audio/bz3 --backend-render diligent --backend-ui imgui

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
- Should dedicated renderer assertions for R1 semantics groups be landed inside R2 or delegated to `engine-backend-testing.md` immediately after R2?
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
- [ ] VQ3 visible directional shadowing improvements completed with BGFX/Diligent parity.
- [ ] VQ4 deterministic visual regression guardrails + wrapper/docs updates completed.
- [x] Post-R3 deferrals are explicitly documented.
