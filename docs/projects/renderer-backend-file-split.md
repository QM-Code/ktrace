# Renderer Backend File Split

## Project Snapshot
- Current owner: `unassigned`
- Status: `blocked (deferred while active karma-lighting-shadow-parity work owns the same backend hotspot paths)`
- Immediate next task: wait for overseer confirmation that active `karma-lighting-shadow-parity` specialist work is retired from backend monolith paths, then start Phase 0 scaffolding.
- Validation gate: `./abuild.py -c -d <build-dir> -b bgfx,diligent` and `./scripts/test-engine-backends.sh <build-dir>`.

## Mission
Split the monolithic renderer backend implementation units into conceptually isolated, agent-friendly files for both BGFX and Diligent at the same time, with matching naming and equivalent boundaries where feasible.

Primary objective:
- remove context-overwhelming single-file ownership (`backend_bgfx.cpp`, `backend_diligent.cpp`) without changing runtime behavior.

## Foundation References
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/policy/rewrite-invariants.md`
- `docs/foundation/architecture/core-engine-contracts.md`
- `docs/projects/karma-lighting-shadow-parity.md`

## Why This Is Separate
- This is a structural maintainability track and a shared unblocker for renderer work.
- It reduces merge pressure and context overload for active renderer/shadow slices without forcing algorithm changes.
- It can proceed as mechanical extraction with strict no-behavior-change acceptance criteria.
- It intentionally defers while active P0 lighting/shadow parity slices are mutating the same backend implementation hotspots.

## Owned Paths
- `src/engine/renderer/backends/bgfx/*`
- `src/engine/renderer/backends/diligent/*`
- `src/engine/renderer/backends/backend_factory_internal.hpp` (if signatures/decls require alignment)
- `src/engine/CMakeLists.txt`
- `docs/projects/renderer-backend-file-split.md`
- `docs/projects/ASSIGNMENTS.md`

## Interface Boundaries
- Inputs consumed:
  - existing renderer backend interface contract from `include/karma/renderer/backend.hpp`
  - existing backend-internal helper headers under `src/engine/renderer/backends/*_internal.hpp`
- Outputs exposed:
  - same `CreateBgfxBackend(...)` / `CreateDiligentBackend(...)` behavior and same runtime-visible renderer behavior.
- Coordinate before changing:
  - `docs/projects/karma-lighting-shadow-parity.md` owned paths:
    - `src/engine/renderer/backends/bgfx/backend_bgfx.cpp`
    - `src/engine/renderer/backends/diligent/backend_diligent.cpp`
    - shadow-related shared internals and sandbox tests.

## Non-Goals
- Do not change renderer algorithms, shader math, or parity targets as part of splitting.
- Do not redesign public renderer interfaces in `include/karma/renderer/backend.hpp`.
- Do not fold unrelated gameplay/network/UI work into this project.
- Do not mix large behavior changes with extraction commits.

## Target Layout (Both Backends)
Canonical per-backend file set:
- `core.cpp`
- `assets.cpp`
- `textures.cpp`
- `pipeline.cpp`
- `shadow.cpp`
- `render.cpp`
- `contracts.cpp`
- `internal.hpp` (private class declaration and shared private structs/helpers)

Naming rule:
- remove redundant backend name prefixes from filenames inside backend directories.
- examples:
  - `src/engine/renderer/backends/diligent/backend_diligent.cpp` -> `src/engine/renderer/backends/diligent/core.cpp`
  - `src/engine/renderer/backends/bgfx/backend_bgfx.cpp` -> `src/engine/renderer/backends/bgfx/core.cpp`

Backend-specific allowances:
- If a concept is backend-unique (for example BGFX shader integrity manifest/trust-chain parsing), keep it isolated in the same-named backend file (`contracts.cpp`) rather than re-expanding `core.cpp`.

## Concept Mapping (Mechanical Extraction Targets)
Shared conceptual mapping for both backends:
1. `core.cpp`
- constructor/destructor, frame begin/end, factory function, camera/light/environment setters, validity checks.

2. `assets.cpp`
- mesh/material creation/destruction, submit queues, backend-owned asset registry wiring.

3. `textures.cpp`
- texture upload/create helpers, mip handling, white/default texture helpers.

4. `pipeline.cpp`
- shader/program/pipeline creation, uniform/sampler registration, startup-time pipeline readiness checks.

5. `shadow.cpp`
- directional/point shadow map resource lifecycle, cache invalidation, update scheduling, upload/render shadow passes.

6. `render.cpp`
- `renderFrame`/`renderLayer` orchestration and draw submission loops (including debug-line draw path).

7. `contracts.cpp`
- direct-sampler/contract observability checks and backend-specific validation plumbing.

## Execution Plan (Simultaneous + Equivalent)
1. Phase 0: Scaffolding + Prefix Removal
- Add `internal.hpp` in both backend directories.
- Rename monolith entrypoint files to `core.cpp` for both backends, keep full behavior unchanged.
- Update `src/engine/CMakeLists.txt` file references.

2. Phase 1: Extract `contracts.cpp` in both backends
- Move contract/observability and backend-specific integrity code out of `core.cpp`.
- Keep call sites unchanged; extraction-only.

3. Phase 2: Extract `textures.cpp` + `assets.cpp` in both backends
- Move texture helper stack first, then mesh/material lifecycle functions.
- Keep public backend method signatures unchanged.

4. Phase 3: Extract `pipeline.cpp` in both backends
- Move shader source/program/pipeline/uniform setup code.
- Preserve initialization order and failure behavior.

5. Phase 4: Extract `shadow.cpp` in both backends
- Move shadow resource creation/update/cache helpers and shadow-pass draw logic.
- Preserve current shadow trace/invariant outputs.

6. Phase 5: Extract `render.cpp` in both backends
- Leave top-level orchestration in `core.cpp`; move heavy layer render loops into `render.cpp`.
- Keep identical draw order and layer behavior.

7. Phase 6: Final convergence cleanup
- Ensure both backend directories expose the same file naming pattern.
- Remove leftover monolith-style helpers from `core.cpp`.

## Agent-Friendly Guardrails
- No extraction phase should touch more than two conceptual files per backend.
- Keep each backend file under soft cap `~900` lines; hard cap `1200` lines.
- If a moved function still requires broad state access, prefer moving that state cluster and helper methods together rather than splitting one function across files.
- Each extraction commit must be "move-only" plus includes/CMake adjustments unless a compile fix is unavoidable.

## Validation
From `m-rewrite/`:

```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent
./scripts/test-engine-backends.sh <build-dir>
./docs/scripts/lint-project-docs.sh
```

Recommended renderer smoke after each major phase:

```bash
./<build-dir>/src/engine/renderer_shadow_sandbox --backend-render bgfx --duration-sec 10
./<build-dir>/src/engine/renderer_shadow_sandbox --backend-render diligent --duration-sec 10
```

## Trace Channels
- `render.system`
- `render.bgfx`
- `render.diligent`
- `render.mesh`

## Build/Run Commands
```bash
./abuild.py -c -d <build-dir> -b bgfx,diligent
./scripts/test-engine-backends.sh <build-dir>
```

## First Session Checklist
1. Read `AGENTS.md`, `docs/foundation/policy/execution-policy.md`, then this file.
2. Confirm no active conflicting slice is mutating the same backend monolith paths.
3. Execute Phase 0 scaffolding in both backends together.
4. Run validation commands with assigned build dir.
5. Update this file and `docs/projects/ASSIGNMENTS.md` in the same handoff.

## Current Status
- `2026-02-17`: project created to formalize simultaneous BGFX/Diligent monolith split strategy.
- `2026-02-17`: naming direction locked to directory-scoped files (`core.cpp`, not `backend_<name>_core.cpp`).
- `2026-02-17`: KARMA split approach accepted as reference for conceptual partitioning, with rewrite-specific guardrails to keep files agent-friendly and avoid one oversized `render.cpp`.
- `2026-02-17`: execution deferred to avoid overlap with active `karma-lighting-shadow-parity` ownership on backend monolith files and adjacent shadow/render internals.

## Open Questions
- Should `render.cpp` be further split into `render_world.cpp` and `render_debug.cpp` if either backend exceeds the hard line cap after Phase 5?
- Should shared internal helper headers (for example shadow/material semantics internals) get a follow-up split project, or stay out of scope for this track?

## Handoff Checklist
- [ ] Both backend directories use matching split filenames where conceptually equivalent.
- [ ] No `backend_bgfx.cpp` / `backend_diligent.cpp` monolith files remain.
- [ ] Build and wrapper validation are green.
- [ ] No renderer behavior drift introduced by extraction.
- [ ] This file and `docs/projects/ASSIGNMENTS.md` are updated.
