# Backend Refactor (Factory + Layout Standardization)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (project created; consistency-first standards locked in docs; implementation not started)`
- Immediate next task: execute `BR0` standards-freeze slice by producing an exact rename/move map and dependency impact matrix before any code movement.
- Validation gate: planning/doc slices run `./docs/scripts/lint-project-docs.sh`; code slices run `./abuild.py -c -d <build-dir>`, `./scripts/test-engine-backends.sh <build-dir>`, and `./scripts/test-server-net.sh <build-dir>`.

## Mission
Standardize backend architecture across `audio`, `renderer`, `physics`, `ui`, and `platform` so they follow one coherent pattern for:
- filesystem layout,
- factory responsibilities and behavior,
- constructor wiring and compile-time capability gating,
- optional stub strategy,
- namespace and header organization.

This track intentionally includes the follow-on namespace/header alignment needed to keep filesystem layout and symbol layout consistent.

## Requested Goals (Locked)
1. Remove redundant `backend_` filename prefix in audio and physics backend files/directories where it adds no information.
2. Add missing `backend_factory.cpp` ownership seams for UI and platform.
3. Standardize how `backend_factory.cpp` works across subsystems (selection model, parsing, compiled backend listing, creation/fallback semantics).
4. Normalize and document stub policy so differences are intentional and explained (not accidental drift).
5. Clarify and normalize renderer internal factory declarations (`src/engine/renderer/backends/internal/backend_factory.hpp`) relative to other subsystems.
6. After factory/layout standardization, execute full header/namespace refactor to match unified filesystem layout.

## Foundation References
- `docs/AGENTS.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/governance/overseer-playbook.md`
- `docs/foundation/policy/rewrite-invariants.md`
- `docs/foundation/architecture/core-engine-contracts.md`
- `docs/projects/ASSIGNMENTS.md`

## Why This Is Separate
- This is a cross-cutting substrate refactor that touches core engine architecture across multiple domains.
- It intersects active tracks (`physics-refactor`, `ui-engine`, `radar`, `karma-lighting-shadow-parity`) and needs explicit sequencing to avoid merge churn.
- The work is mostly structural and consistency-driven rather than feature-driven, so it benefits from one dedicated governance track.

## Current Baseline (2026-02-18)

| Subsystem | Current factory ownership | Current layout note | Gap against standardization goal |
|---|---|---|---|
| `audio` | `src/engine/audio/backend_factory.cpp` | backend impl files named `backend_*_stub.cpp` but contain real impl + stub fallback | redundant naming, mixed semantic meaning of `_stub` |
| `physics` | `src/engine/physics/backend_factory.cpp` | same pattern as audio | same naming/semantic drift as audio |
| `renderer` | `src/engine/renderer/backend_factory.cpp` | constructor declarations in `src/engine/renderer/backends/internal/backend_factory.hpp` | naming of internal factory header differs from audio/physics convention |
| `ui` | no dedicated `backend_factory.cpp`; selection logic in `src/engine/ui/system.cpp` | backend constructors declared in `src/engine/ui/backend.hpp` | missing explicit factory seam |
| `platform` | `src/engine/platform/window_factory.cpp` direct constructor path | no backend kind parse/list/create layer | no standardized backend factory behavior contract |

## Direction Lock (Non-Negotiable)
1. Standardization must be behavior-preserving by default; structural renames and seam extraction come before semantic backend behavior changes.
2. Factory APIs must remain deterministic:
   - parse backend name,
   - list compiled backends,
   - resolve preferred or auto,
   - report selected backend.
3. No backend API/type leakage into game paths while refactoring engine backend seams.
4. Namespace/header refactor must follow layout refactor, not precede it.
5. Transition slices must be bisectable: every accepted slice builds and passes required wrappers.
6. Do not combine this refactor with unrelated feature intake in renderer/UI/physics/gameplay tracks.
7. Platform naming consistency is now explicit for this track: migrate `src/engine/platform/window_factory.cpp` to `src/engine/platform/backend_factory.cpp` (with bounded compatibility shim only if required during transition).

## Consistency Decisions (Locked 2026-02-18)
1. Namespace model aligns to filesystem hierarchy. Target backend namespace form is:
   - `karma::audio::backend`
   - `karma::physics::backend`
   - `karma::renderer::backend`
   - `karma::ui::backend`
   - `karma::platform::backend`
2. Existing underscore namespaces (`karma::audio_backend`, `karma::physics_backend`, `karma::renderer_backend`) are transitional only during BR6 and must be removed by BR7.
3. UI and platform adopt the same backend-factory API surface shape as other subsystems:
   - `BackendKindName`
   - `ParseBackendKind`
   - `CompiledBackends`
   - `CreateBackend`
4. Platform standardization includes formal platform backend-kind enumeration and parse/list/create behavior, even if only one backend is compiled in current builds.
5. Stub strategy is standardized to explicit per-backend stub units (`stub.cpp`) for all subsystems. Real backend implementation units must not also carry stub fallback classes.
6. Renderer constructor-declaration header is standardized to common naming:
   - target path: `src/engine/renderer/backends/factory_internal.hpp`.
7. Audio/physics redundant `backend_` prefix removal applies to files, internal headers, and backend subdirectory names (not filenames only).
8. Compatibility shims are allowed only as short-lived migration aids and must be removed by BR7.
9. Execution granularity is one subsystem per slice for churn control and merge safety.
10. Validation policy remains uniform:
   - every code slice runs baseline build + wrappers,
   - renderer/ui/platform-touch slices also run runtime backend matrix.

## Proposed Canonical Pattern

### A. Per-subsystem backend seams
- Public backend contract header remains in `include/karma/<subsystem>/...` (existing ownership preserved).
- Runtime factory entrypoint lives in `src/engine/<subsystem>/backend_factory.cpp`.
- Backend constructor declarations live in one subsystem-local internal header:
  - preferred name: `src/engine/<subsystem>/backends/factory_internal.hpp`.
- Concrete backend implementations live under:
  - `src/engine/<subsystem>/backends/<backend-name>/...`
  - or bounded equivalent when a backend is a single source file.

### B. Factory behavior standard
- All backend factories expose these semantics:
  - `BackendKindName(kind)`
  - `ParseBackendKind(name)`
  - `CompiledBackends()`
  - `CreateBackend(preferred, out_selected)`
- `preferred == Auto` must iterate compiled backends in deterministic priority order.
- Explicit backend requests must not silently switch to a different backend.
- `out_selected` is set to `Auto` before resolution, then to concrete selected backend on success.

### C. Stub policy standard
- Stubs are optional implementation strategy, required behavior contract.
- Required behavior when backend is unavailable (not compiled or failed init policy path):
  - deterministic failure surface,
  - trace-visible reason,
  - no crash/no undefined behavior.
- Locked style for this project: explicit per-backend `stub.cpp` units across all subsystems.

### D. Renderer internal factory header clarification
- `src/engine/renderer/backends/internal/backend_factory.hpp` currently serves the same purpose as audio/physics internal factory headers: constructor declarations used by the top-level factory.
- In this track it will be renamed to the standardized path:
  - `src/engine/renderer/backends/factory_internal.hpp`.

## Owned Paths
- `docs/projects/backend-refactor.md`
- `docs/projects/ASSIGNMENTS.md`
- `src/engine/audio/backend_factory.cpp`
- `src/engine/audio/backends/*`
- `src/engine/renderer/backend_factory.cpp`
- `src/engine/renderer/backends/*`
- `src/engine/physics/backend_factory.cpp`
- `src/engine/physics/backends/*`
- `src/engine/ui/backend.hpp`
- `src/engine/ui/backend_factory.cpp` (new)
- `src/engine/ui/backends/*`
- `src/engine/ui/system.cpp`
- `src/engine/platform/backend_factory.cpp` (new or standardized replacement path)
- `src/engine/platform/window_factory.cpp`
- `src/engine/platform/backends/*`
- `include/karma/audio/*`
- `include/karma/physics/*`
- `include/karma/renderer/*`
- `include/karma/ui/*`
- `include/karma/platform/*`
- `src/engine/app/*/backend_resolution.cpp`
- `include/karma/app/*/backend_resolution.hpp`
- `src/engine/cli/client/backend_options.cpp`
- `src/engine/CMakeLists.txt`

## Interface Boundaries
- Inputs consumed:
  - existing backend contracts in `include/karma/*`.
  - existing backend-resolution and CLI backend option surfaces.
- Outputs exposed:
  - standardized backend factory surfaces across all subsystems.
  - unified namespace/header organization aligned with filesystem.
- Coordinate before changing:
  - `src/engine/CMakeLists.txt` (hotspot)
  - `src/game/CMakeLists.txt` (if transitive include/export impacts appear)
  - `docs/foundation/architecture/core-engine-contracts.md` (if external contracts change)
  - `docs/projects/physics-refactor.md`, `docs/projects/ui-engine.md`, `docs/projects/radar.md`, `docs/projects/karma-lighting-shadow-parity.md` when overlap is active.

## Non-Goals
- Do not redesign backend feature semantics (audio mixing, physics dynamics, renderer shading, UI content behavior, platform event behavior) in this track.
- Do not add new runtime backend capabilities beyond what is required for seam standardization.
- Do not widen into gameplay/UI feature migration.
- Do not force a second platform backend unless separately approved by platform expansion policy.

## Execution Plan

### BR0: Standards Freeze + Impact Matrix (`shared unblocker`)
Scope:
- Freeze canonical naming, factory API contract, and stub policy decisions in this doc.
- Produce exact rename/move map and include-impact matrix.
- Record required compatibility aliases (if any) for staged migration.

Acceptance:
- exact rename list approved before any file moves.
- factory behavior contract accepted for all five subsystems.
- overlap/conflict sequencing plan recorded.

### BR1: Mechanical Rename Pass (`shared unblocker`)
Scope:
- Remove redundant `backend_` filename prefixes in audio/physics backend files and internal headers.
- Update includes and build lists only.
- No behavior changes.

Acceptance:
- no behavior diffs.
- build graph updated and green.
- include paths compile cleanly.

### BR2: UI Factory Extraction (`shared unblocker`)
Scope:
- Add `src/engine/ui/backend_factory.cpp`.
- Move backend selection/creation policy out of `src/engine/ui/system.cpp`.
- Keep `UiSystem` lifecycle semantics unchanged.

Acceptance:
- UI backend selection logic lives only in UI factory seam.
- `UiSystem` consumes standardized creation surface.

### BR3: Platform Factory Standardization (`shared unblocker`)
Scope:
- Rename `src/engine/platform/window_factory.cpp` to `src/engine/platform/backend_factory.cpp` for naming consistency with other subsystems.
- Keep a short-lived forwarding shim only if needed to preserve a bisectable migration.
- Introduce standardized platform backend factory surface at the new canonical path.
- Add parse/list/create semantics where platform backend choices are surfaced.
- Keep single-backend behavior stable for current builds.

Acceptance:
- platform follows same factory contract shape as other subsystems.
- no runtime regression in current SDL3 path.

### BR4: Cross-Subsystem Factory Behavior Normalization (`shared unblocker`)
Scope:
- Align audio/physics/renderer/ui/platform factories to one deterministic algorithm and naming contract.
- Normalize tracing/error phrasing and `out_selected` behavior.

Acceptance:
- each subsystem exposes equivalent backend-factory behavior semantics.
- backend-resolution call sites remain clean and predictable.

### BR5: Stub Policy Harmonization (`shared unblocker`)
Scope:
- Apply chosen stub strategy consistently.
- Ensure unavailable backend behavior is deterministic and traceable everywhere.
- Document justified exceptions if any subsystem must diverge.

Acceptance:
- no accidental stub-policy drift remains.
- rationale for any exception is documented in this file.

### BR6: Header + Namespace Refactor (`shared unblocker`)
Scope:
- Align namespaces and header/include layout with standardized filesystem structure.
- Update app/CLI/backend-resolution includes and references.
- Add bounded compatibility aliases only if required to keep migration bisectable.

Acceptance:
- namespaces and filesystem layout are coherent across all backend subsystems.
- no stale include roots or orphaned aliases remain.

### BR7: Closeout + Contract Docs Sync (`shared unblocker`)
Scope:
- Remove temporary migration shims.
- Sync foundation architecture docs if public contracts changed.
- finalize maintenance guidance.

Acceptance:
- project doc and assignment row reflect closed or next actionable state.
- no temporary compatibility scaffolding left unintentionally.

## Validation
From `m-rewrite/`:

```bash
# planning/doc slices
./docs/scripts/lint-project-docs.sh

# code slices (minimum gate)
./abuild.py -c -d <build-dir>
./scripts/test-engine-backends.sh <build-dir>
./scripts/test-server-net.sh <build-dir>
```

Extended validation when renderer/ui/platform selections are touched:

```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent,imgui,rmlui
timeout 20s ./<build-dir>/bz3 --backend-render bgfx --backend-ui imgui
timeout 20s ./<build-dir>/bz3 --backend-render diligent --backend-ui imgui
timeout 20s ./<build-dir>/bz3 --backend-render bgfx --backend-ui rmlui
timeout 20s ./<build-dir>/bz3 --backend-render diligent --backend-ui rmlui
```

## Trace Channels
- `engine.app`
- `audio.system`
- `audio.sdl3audio`
- `audio.miniaudio`
- `physics.system`
- `physics.jolt`
- `physics.physx`
- `render.system`
- `render.bgfx`
- `render.diligent`
- `ui.system`
- `ui.system.imgui`
- `ui.system.rmlui`

## Build/Run Commands
```bash
./abuild.py -c -d <build-dir>
./scripts/test-engine-backends.sh <build-dir>
./scripts/test-server-net.sh <build-dir>
```

## First Session Checklist
1. Read `docs/AGENTS.md`, then `docs/foundation/policy/execution-policy.md`, then this file.
2. Confirm active overlap windows with `physics-refactor`, `ui-engine`, `radar`, and renderer parity tracks.
3. Execute `BR0` only: produce exact rename/move map and impact matrix.
4. Run required validation for any touched scope.
5. Update this file and `docs/projects/ASSIGNMENTS.md` in the same handoff.

## Risks and Mitigations
1. Risk: widespread include/path churn breaks unrelated tracks.
   - Mitigation: mechanical rename slices are isolated and behavior-free; validate each slice before proceeding.
2. Risk: merge conflicts in `src/engine/CMakeLists.txt`.
   - Mitigation: keep build-list edits narrowly scoped and sequence with active owners.
3. Risk: inconsistent fallback behavior after factory normalization.
   - Mitigation: enforce explicit unavailable-backend semantics and trace evidence in all subsystems.
4. Risk: namespace refactor breaks downstream includes.
   - Mitigation: staged compatibility aliases with explicit removal phase (`BR7`).

## Open Questions
- None blocking for implementation start. Consistency decisions are locked in this file.

## Current Status
- `2026-02-18`: project created with locked goals, baseline inventory, and phased implementation plan.
- `2026-02-18`: documentation decision lock added: platform factory naming target is explicitly `window_factory.cpp` -> `backend_factory.cpp`.
- `2026-02-18`: consistency-first ambiguity resolution locked (namespace model, stub model, UI/platform factory parity, renderer internal header naming, and migration constraints).
- `2026-02-18`: no source refactor has started yet; first executable slice is `BR0` standards freeze + exact rename/move map.

## Handoff Checklist
- [ ] Scope stayed within this project boundaries.
- [ ] Validation commands and results are recorded.
- [ ] `docs/projects/backend-refactor.md` snapshot/status updated.
- [ ] `docs/projects/ASSIGNMENTS.md` row updated in the same handoff.
- [ ] Risks/open questions carried forward.
