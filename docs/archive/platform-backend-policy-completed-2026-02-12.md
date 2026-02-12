# Platform Backend Policy

## Project Snapshot
- Current owner: `specialist-platform-backend-policy`
- Status: `completed` (archived closeout snapshot; Slice 1-4 complete with admission contract active)
- Immediate next task: `none` (reference-only; reopen via a new active project doc if a concrete second-backend blocker is accepted).
- Strategic alignment track: `shared unblocker (architecture hygiene)`
- Validation gate: docs lint must pass for every slice; code-touching slices must also pass assigned `bzbuild.py` and wrapper gates.

## Mission
Keep an engine-owned platform seam while standardizing on SDL3 as the single active platform backend.

## Priority Directive (2026-02-11)
- This project is medium priority and should run behind active P0 renderer/network slices.
- The project must remove dormant platform backend complexity without collapsing the engine/platform seam.
- SDL3 remains the only active backend unless a concrete blocker justifies re-expansion.

## Primary Specs
- `AGENTS.md`
- `docs/AGENTS.md`
- `docs/projects/core-engine-infrastructure.md`
- `docs/projects/testing-ci-docs.md`
- `docs/projects/engine-backend-testing.md`

## Why This Is Separate
This is policy/boundary hardening work that can proceed without changing gameplay rules, network protocol semantics, or renderer feature slices.

## Owned Paths
- `docs/projects/platform-backend-policy.md`
- `docs/projects/core-engine-infrastructure.md` (platform/lifecycle policy references only)
- `m-rewrite/src/engine/platform/*`
- `m-rewrite/include/karma/platform/*`
- `m-rewrite/CMakeLists.txt` and `m-rewrite/src/engine/CMakeLists.txt` (platform backend wiring only)

## Interface Boundaries
- Engine/game code outside platform adapters must not consume SDL types directly.
- Keep one engine-facing platform contract; implementation is SDL3-only until a concrete second-backend requirement is approved.
- Coordinate before changing:
  - `docs/projects/core-engine-infrastructure.md`
  - `docs/projects/testing-ci-docs.md`
  - `docs/projects/engine-backend-testing.md`

## Non-Goals
- Do not implement GLFW support.
- Do not reintroduce SDL2 compatibility paths.
- Do not add speculative runtime backend switching/registry machinery.
- Do not move platform details into `src/game/*`.
- Do not widen scope into renderer/network/gameplay tracks.

## Validation
Docs-only slices (from repository root):

```bash
./docs/scripts/lint-project-docs.sh
```

Code-touching slices (from `m-rewrite/` with assigned build dir):

```bash
./scripts/check-platform-seam.sh
./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-sdl3audio
./scripts/test-engine-backends.sh build-sdl3-bgfx-jolt-rmlui-sdl3audio
```

## Trace Channels
- `engine.app`
- `engine.platform`

## Build/Run Commands
From `m-rewrite/`:

```bash
./scripts/check-platform-seam.sh
./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-sdl3audio
./scripts/test-engine-backends.sh build-sdl3-bgfx-jolt-rmlui-sdl3audio
```

## First Session Checklist
1. Read `AGENTS.md`, then `docs/AGENTS.md`, then this file.
2. Confirm SDL3-only policy and thin-abstraction constraint.
3. Implement exactly one slice from the queue below.
4. Validate with required commands.
5. Update this file and `docs/projects/ASSIGNMENTS.md` in the same handoff.

## Current Status
- `2026-02-11`: Project created and queued at medium priority.
- `2026-02-11`: Policy direction confirmed:
  - keep thin engine/platform abstraction seam,
  - keep SDL3 as only active backend,
  - remove dormant GLFW/SDL2 implementation and documentation references,
  - retain `bzbuild.py`/CMake scaffolding for future adapter reintroduction only if justified by concrete requirements.
- `2026-02-11`: Slice 1 (docs+inventory) completed:
  - scoped inventory executed across `docs/`, `CMakeLists.txt`, `src/engine/CMakeLists.txt`, `include/karma/platform/*`, and `src/engine/platform/*`,
  - keep/remove decisions captured below with rationale,
  - Slice 2 execution checklist derived from findings is ready.
- `2026-02-11`: Slice 2 (docs/build cleanup) completed:
  - top-level build policy wiring now enforces SDL3-only window backend,
  - engine platform source wiring and factory branches for dormant GLFW/SDL2 stubs were removed,
  - stale docs references to active rewrite GLFW/SDL2 stubs were updated.
- `2026-02-11`: Slice 3 (seam guardrails) completed:
  - added deterministic guardrail script `scripts/check-platform-seam.sh`,
  - guardrail fails on SDL header include and `SDL_*` symbol usage outside allowlisted backend implementation files,
  - tightened platform seam by removing SDL header/type exposure from `src/engine/platform/backends/window_sdl3.hpp`.
- `2026-02-11`: Slice 4 (future reintroduction contract) completed:
  - codified formal second-backend admission contract, conformance gate matrix, stage-based promotion checklist, and speculative-work rejection criteria,
  - kept SDL3 as only active backend and did not add dormant backend implementation code.
- `2026-02-12`: Project closed and archived:
  - active delegation ownership moved out of `docs/projects/`,
  - archive snapshot path set to `docs/archive/platform-backend-policy-completed-2026-02-12.md`,
  - reopen rule locked to explicit new project doc creation when admission entry criteria are met.

## Slice 1 Inventory (GLFW/SDL2 References)
| Path / Reference | Decision | Rationale |
|---|---|---|
| `CMakeLists.txt` (`KARMA_WINDOW_BACKEND` cache string/options + `KARMA_WINDOW_BACKEND_SDL2` / `KARMA_WINDOW_BACKEND_GLFW` compile defs) | `Removed in Slice 2 (2026-02-11)` | SDL3-only active backend policy: multi-backend option surface was dormant complexity. |
| `src/engine/CMakeLists.txt` (`window_sdl2_stub.cpp`, `window_glfw_stub.cpp` in `ENGINE_CLIENT_SOURCES`) | `Removed in Slice 2 (2026-02-11)` | Build graph now includes only active SDL3 window backend source. |
| `src/engine/platform/window_factory.cpp` (`#elif KARMA_WINDOW_BACKEND_SDL2/GLFW` includes + stub returns) | `Removed in Slice 2 (2026-02-11)` | Thin seam preserved while dead backend branches were removed. |
| `src/engine/platform/backends/window_sdl2_stub.hpp` | `Removed in Slice 2 (2026-02-11)` | Dormant SDL2 stub backend removed under SDL3-only policy. |
| `src/engine/platform/backends/window_sdl2_stub.cpp` | `Removed in Slice 2 (2026-02-11)` | Implementation unit removed with dormant SDL2 backend. |
| `src/engine/platform/backends/window_glfw_stub.hpp` | `Removed in Slice 2 (2026-02-11)` | Dormant GLFW stub backend removed under SDL3-only policy. |
| `src/engine/platform/backends/window_glfw_stub.cpp` | `Removed in Slice 2 (2026-02-11)` | Implementation unit removed with dormant GLFW backend. |
| `include/karma/platform/*` | `Keep` | No GLFW/SDL2 references found; this is the seam to preserve. |
| `docs/DECISIONS.md` (platform breadth deferred decision) | `Keep` | Historical decision log remains accurate as architecture context. |
| `docs/archive/KARMA_ALIGNMENT_AUDIT_2026-02-10.md` (stale rewrite GLFW/SDL2 stub statement) | `Updated in Slice 2 (2026-02-11)` | Archived historical audit; content was updated to reflect SDL3-only active backend state for rewrite before archival. |
| `docs/projects/platform-backend-policy.md` (policy guardrails mentioning GLFW/SDL2) | `Keep (policy lines only)` | Explicitly disallowing GLFW/SDL2 implementation is an intentional constraint, not dormant backend wiring. |
| `docs/projects/ASSIGNMENTS.md` (platform row task text) | `Updated in Slice 2 handoff` | Assignment now advances to Slice 3 seam guardrails. |

## Slice Queue
1. Slice 1 (docs+inventory, no engine-code behavior changes):
   - Inventory all GLFW/SDL2 references across docs, CMake, and platform-related source.
   - Classify each reference as remove/retain-with-rationale.
   - Add a concrete removal plan and acceptance gate to this file.
   - Status: `Completed (2026-02-11; docs-only, no runtime behavior changes)`.
2. Slice 2 (docs/build cleanup):
   - Remove stale GLFW/SDL2 references from docs and build wiring where no longer used.
   - Keep SDL3-only wiring explicit and deterministic.
   - Status: `Completed (2026-02-11; runtime behavior preserved)`.
3. Slice 3 (seam guardrails):
   - Add/strengthen checks to prevent SDL type leakage outside platform/backend boundaries.
   - Keep checks deterministic and fast for CI use.
   - Status: `Completed (2026-02-11; guardrail + seam hardening landed)`.
4. Slice 4 (future reintroduction contract):
   - Codify requirements for adding a future second backend (e.g., SDL4/competitor) through contract + conformance tests, not speculative dormant code paths.
   - Status: `Completed (2026-02-11; docs-only contract codification)`.

## Second Backend Admission Contract
### Entry Criteria
1. A concrete blocker is documented that cannot be resolved within SDL3-only policy and directly impacts active rewrite delivery.
2. A named owner is assigned with isolated build directories and explicit validation responsibility.
3. A proposal packet is attached with:
   - exact seam changes and owned paths,
   - deterministic validation commands,
   - rollback plan that preserves SDL3-only behavior if the candidate fails.
4. The proposal explicitly states zero scope expansion into renderer/network/gameplay semantics.
5. Overseer approval is recorded before any backend-specific implementation begins.

### Required Seam Invariants
1. Engine-facing platform contract remains centered on `karma::platform::Window` and backend details stay engine-internal.
2. `./scripts/check-platform-seam.sh` remains mandatory and passing; SDL headers/types must stay in allowlisted backend implementation files only.
3. No backend-specific headers/types are introduced into `src/game/*` or public engine/game-facing platform contracts.
4. SDL3 remains the default active backend until a separate explicit default-switch acceptance is approved.
5. No dormant or stub-only backend trees are added; candidate work must be gated by concrete admission and conformance evidence.

### Explicit Non-Goals
1. No speculative backend registry/switching machinery without an approved concrete blocker.
2. No parity-by-file-mirroring from external repos.
3. No placeholder/stub backend implementations intended only to “prepare” for future work.
4. No contract drift that weakens seam guardrails or broadens backend leakage.

## Conformance Gate Matrix
| Gate | Command / Evidence | Pass Criteria | Failure / Rollback Policy |
|---|---|---|---|
| `Seam Guardrail` | `./scripts/check-platform-seam.sh` | No SDL header/type violations outside allowlist. | Candidate admission is blocked; revert offending seam leakage before any further review. |
| `Baseline Build` | `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-sdl3audio` | Clean configure/build with SDL3 default path intact. | Candidate is rejected for regression risk; rollback candidate changes and restore baseline pass. |
| `Baseline Backend Wrapper` | `./scripts/test-engine-backends.sh build-sdl3-bgfx-jolt-rmlui-sdl3audio` | Required backend smoke/parity tests pass on baseline profile. | Candidate is rejected; no merge until wrapper gate returns to green. |
| `Candidate Build Profile` | `./bzbuild.py -c build-<candidate>-bgfx-jolt-rmlui-sdl3audio` (or explicitly approved equivalent) | Candidate profile config/build passes without touching renderer/network/gameplay semantics. | Candidate stays in prototype state; rollback or isolate failing changes. |
| `Candidate Backend Wrapper` | `./scripts/test-engine-backends.sh build-<candidate>-bgfx-jolt-rmlui-sdl3audio` (or explicitly approved equivalent) | Candidate backend passes same backend wrapper expectations as baseline. | Candidate admission fails; rollback and keep SDL3-only active backend policy. |
| `Docs Integrity` | `./docs/scripts/lint-project-docs.sh` | Project/assignment docs stay synchronized with actual gate outcomes. | Candidate cannot be promoted; docs + status must be corrected in same handoff. |

### Required Build/Test Evidence Format
1. Exact command list in execution order.
2. Exit code for each command.
3. Build directory used for each command.
4. Explicit pass/fail summary for wrapper tests (include test counts where available).
5. File list touched by the candidate backend slice.

### Failure / Rollback Policy
1. Any failed gate immediately blocks promotion to the next stage.
2. Failed candidate slices are rolled back or isolated; no partial backend breadth is left active.
3. SDL3-only default behavior must remain green before new backend work resumes.
4. Re-attempt requires a new handoff with corrected scope and fresh full evidence.

## Promotion Checklist
| Stage | Required Acceptance Conditions | Promotion Decision |
|---|---|---|
| `Proposal` | Entry criteria met, blocker documented, owner assigned, proposal packet + rollback plan approved. | Move to `Prototype` only after overseer approval. |
| `Prototype` | Candidate compiles in isolated candidate build profile, seam guardrail passes, no gameplay/network/renderer scope drift. | Move to `Parity` only when baseline remains fully green. |
| `Parity` | Baseline + candidate conformance gate matrix both pass with deterministic evidence in one handoff. | Move to `Default-Candidate` only with explicit risk review sign-off. |
| `Default-Candidate` | Demonstrated parity stability, rollback rehearsal documented, and explicit separate approval to consider default change. | Default switch remains a separate acceptance event; without approval, SDL3 stays default. |

## Rejection Criteria (Speculative Backend Work)
1. Missing concrete blocker or missing overseer-approved proposal packet.
2. Adds dormant/stub backend code without conformance evidence.
3. Introduces SDL/backend type leakage outside allowlisted backend implementation files.
4. Requires renderer/network/gameplay changes to “make backend work.”
5. Omits deterministic gate evidence or fails any gate in the conformance matrix.
6. Attempts to change default backend without completing promotion stages and explicit default-switch approval.

## Slice 3 Guardrail Enforcement (Completed 2026-02-11)
- Command:
  - `./scripts/check-platform-seam.sh`
- Allowlisted backend implementation files:
  - `src/engine/platform/backends/window_sdl3.cpp`
  - `src/engine/audio/backends/backend_sdl3audio_stub.cpp`
- Blocked patterns outside allowlist:
  - SDL header includes: `^[[:space:]]*#include[[:space:]]*[<"]SDL`
  - SDL symbol usage: `\bSDL_[A-Za-z0-9_]+\b`
- Enforcement result:
  - SDL headers/types are constrained to allowlisted backend implementation files; non-backend files fail fast.

## Slice 2 Execution Checklist (Derived from Slice 1 Inventory, Completed 2026-02-11)
1. [x] Updated top-level window backend policy wiring in `CMakeLists.txt`:
   - removed `sdl2|glfw` cache options and compile-definition branches,
   - retained deterministic SDL3-only define wiring.
2. [x] Updated engine source wiring in `src/engine/CMakeLists.txt`:
   - removed GLFW/SDL2 stub source entries from `ENGINE_CLIENT_SOURCES`.
3. [x] Simplified platform factory in `src/engine/platform/window_factory.cpp`:
   - removed SDL2/GLFW include + factory branches,
   - retained SDL3 path and seam contract (`CreateWindow(const WindowConfig&) -> std::unique_ptr<Window>`).
4. [x] Removed dormant platform stub files:
   - `src/engine/platform/backends/window_sdl2_stub.hpp`
   - `src/engine/platform/backends/window_sdl2_stub.cpp`
   - `src/engine/platform/backends/window_glfw_stub.hpp`
   - `src/engine/platform/backends/window_glfw_stub.cpp`
5. [x] Cleaned stale docs references describing active GLFW/SDL2 rewrite stubs:
   - updated `docs/archive/KARMA_ALIGNMENT_AUDIT_2026-02-10.md`,
   - retained intentional policy/decision-history references.
6. [x] Re-scanned and validated post-cleanup:
   - `rg -n -i "glfw|sdl2|sdl 2|sdl-2" docs CMakeLists.txt src/engine/CMakeLists.txt include/karma/platform src/engine/platform`
   - outcome: no stale build/platform stub wiring remains; retained references are policy/history/inventory records.
7. [x] Ran closeout validation gates for Slice 2:
   - `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-sdl3audio`,
   - `./scripts/test-engine-backends.sh build-sdl3-bgfx-jolt-rmlui-sdl3audio`,
   - `./docs/scripts/lint-project-docs.sh`.

## Completed Specialist Packet (Slice 2)
```text
Read in order:
1) AGENTS.md
2) docs/AGENTS.md
3) docs/projects/README.md
4) docs/projects/ASSIGNMENTS.md
5) docs/projects/platform-backend-policy.md

Take ownership of: docs/projects/platform-backend-policy.md

Goal:
- Execute Slice 2 cleanup from the approved Slice 1 inventory while preserving SDL3-only active backend policy and thin platform seam.

Scope:
- Apply Slice 2 checklist items captured in this project file:
  - build wiring cleanup (`CMakeLists.txt`, `src/engine/CMakeLists.txt`),
  - platform backend factory/stub cleanup under `src/engine/platform/*`,
  - stale-doc cleanup where references no longer match active SDL3-only state.

Constraints:
- Do not implement GLFW/SDL2 functionality.
- Do not remove SDL3 seam abstractions.
- Do not touch renderer/network/gameplay paths.
- Build policy remains `bzbuild.py`-only if build validation is run.

Validation (required):
- `./docs/scripts/lint-project-docs.sh`
- Because Slice 2 is code-touching, also run assigned `bzbuild.py` + wrapper gates.

Docs updates (required):
- Update docs/projects/platform-backend-policy.md snapshot/status/slice queue.
- Update docs/projects/ASSIGNMENTS.md row (status/next task/last-update).

Handoff must include:
- files changed
- exact commands run + results
- post-cleanup reference scan results vs. retained-reference allowlist
- risks/open questions
- explicit next slice task (Slice 3 guardrails)
```

## Slice 1 Acceptance Gate (Must Pass)
- Inventory covers docs + build + platform-owned paths listed in scope.
- Each identified GLFW/SDL2 reference has explicit keep/remove decision with rationale.
- No runtime behavior changes are introduced.
- `./docs/scripts/lint-project-docs.sh` passes.
- `docs/projects/platform-backend-policy.md` and `docs/projects/ASSIGNMENTS.md` are both updated in the same handoff.

## Handoff Review Rubric (Overseer)
- `Accept` when Slice 1 acceptance gate is fully satisfied with explicit evidence.
- `Revise` when inventory is partial, rationale is missing, scope is widened, or docs updates are incomplete.
- Required evidence format:
  - file list with paths,
  - exact commands and outcomes,
  - concise keep/remove decision table summary,
  - explicit statement of what was intentionally not changed.

## Open Questions
- What is the minimal platform contract surface we want to freeze before any backend swap is reconsidered?
- Which tests should become mandatory conformance gates for any future SDL4 or competitor adapter?

## Handoff Checklist
- [x] Slice scope completed
- [x] Docs updated
- [x] Validation run and summarized
- [x] Build policy constraints preserved (`bzbuild.py` only)
- [x] Risks/open questions listed
