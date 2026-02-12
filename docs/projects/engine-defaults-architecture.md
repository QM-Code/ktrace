# Engine Defaults Architecture

## Project Snapshot
- Current owner: `specialist-engine-defaults-architecture`
- Status: `in progress` (layering contracts, stable-first sequencing, stage-1/stage-6 kickoff packets, and consolidated stages 1-6 handoff checklist are codified)
- Immediate next task: keep stage handoff checklist aligned as implementation slices land (update owner-role mapping and closeout evidence requirements when contracts/tests change).
- Validation gate: `./docs/scripts/lint-project-docs.sh`

## Mission
Codify rewrite-native engine structure and abstraction layers so future implementation work follows a clear `95% defaults / 5% overrides` model.

## Why This Is Separate
This is an architecture-documentation track that aligns multiple implementation projects without forcing immediate code churn.

## Owned Paths
- `docs/projects/engine-defaults-architecture.md`
- `docs/projects/core-engine-infrastructure.md`
- `docs/architecture/*`

## Interface Boundaries
- This project defines architecture guidance and contract intent.
- It should not prescribe backend-specific implementation details.
- Coordinate before changing:
  - `docs/projects/physics-backend.md`
  - `docs/archive/audio-backend-completed-2026-02-12.md` (reference-only closeout snapshot)
  - `docs/projects/core-engine-infrastructure.md`

## Non-Goals
- No direct feature parity implementation work in renderer/network/physics/audio.
- No dependency on KARMA file layout or KARMA path references in rewrite architecture docs.

## Validation
From repository root:

```bash
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `n/a` (docs project)

## Build/Run Commands
```bash
# Docs-only project; no build commands required.
```

## First Session Checklist
1. Read `AGENTS.md`, then `docs/AGENTS.md`, then this file.
2. Write explicit ownership boundaries for:
   - system scheduling/orchestration,
   - component catalog strategy,
   - physics API layers (core + optional facade),
   - audio API layers (core + optional facade).
3. Define candidate "95% default path" APIs and likely extension points.
4. Record unknowns as open questions, not implicit assumptions.
5. Run docs lint and update assignment row.

## Current Status
- `2026-02-10`: Completed first architecture codification slice in `docs/architecture/ENGINE_DEFAULTS_MODEL.md`, including:
  - engine-owned scheduling model with deterministic ordering + explicit override hooks,
  - component catalog strategy with ownership/serialization rules,
  - physics and audio layered API guidance (core contract + optional facade + advanced path),
  - explicit `95% defaults / 5% overrides` policy and bounded implementation sequence.
- `2026-02-10`: Converted scheduler guidance into implementation-ready spec in `docs/projects/core-engine-infrastructure.md`, including:
  - authoritative phase enum,
  - system registration schema,
  - dependency validation/order rules,
  - mandatory cycle-fail behavior,
  - explicit override boundaries,
  - first-slice acceptance criteria and required tests.
- `2026-02-10`: Converted component catalog guidance into implementation-ready spec in `docs/projects/core-engine-infrastructure.md`, including:
  - canonical descriptor metadata schema,
  - ownership and runtime source-of-truth rules,
  - serialization mode and schema-version compatibility policy,
  - startup/runtime validation and deterministic failure rules,
  - explicit extension/replacement boundaries,
  - first-slice acceptance criteria and required tests.
- `2026-02-10`: Converted physics API layering guidance into implementation-ready spec in `docs/projects/core-engine-infrastructure.md`, including:
  - core `BodyId` and `PhysicsSystem` boundary rules,
  - optional 95%-path facade boundaries,
  - explicit override/extension limits,
  - startup/runtime validation and deterministic failure rules,
  - first-slice acceptance criteria and required tests.
- `2026-02-10`: Converted audio API layering guidance into implementation-ready spec in `docs/projects/core-engine-infrastructure.md`, including:
  - core deterministic request/voice boundary rules,
  - optional 95%-path facade boundaries,
  - explicit override/extension limits,
  - startup/runtime validation and deterministic failure rules,
  - explicit acceptance criteria and required unit/parity/integration tests.
- `2026-02-10`: Synced `docs/projects/physics-backend.md` and `docs/projects/audio-backend.md` to explicitly reflect:
  - core contract boundaries,
  - optional 95%-path facade boundaries,
  - override/extension limits,
  - validation/failure expectations,
  - required tests and wrapper-gate expectations aligned to `core-engine-infrastructure.md`.
- `2026-02-12`: `audio-backend` track archived at `docs/archive/audio-backend-completed-2026-02-12.md` after non-finite `gain`/`pitch` deterministic rejection parity closeout; this architecture track now treats that snapshot as reference-only unless a new active audio project doc is created.
- `2026-02-10`: Validation run: `./docs/scripts/lint-project-docs.sh` passed (`[lint] OK`).
- `2026-02-11`: Codified stable-first implementation sequencing in `docs/projects/core-engine-infrastructure.md` with staged entry/implementation/exit criteria across scheduler/component/physics/audio contracts, explicit wrapper-gate closeout rules, and parallelization constraints.
- `2026-02-11`: Added stage-1 scheduler implementation kickoff packet in `docs/projects/core-engine-infrastructure.md` with:
  - entry checklist,
  - concrete required tests to run first,
  - exit-criteria checklist,
  - wrapper closeout reminders for `./scripts/test-engine-backends.sh <build-dir>` when applicable,
  - explicit boundaries preventing physics/audio scope creep before scheduler core exits.
- `2026-02-11`: Added stage-2 component-catalog implementation kickoff packet in `docs/projects/core-engine-infrastructure.md` with:
  - entry checklist,
  - concrete required tests to run first,
  - exit-criteria checklist,
  - wrapper closeout reminders for `./scripts/test-engine-backends.sh <build-dir>` when applicable,
  - explicit boundaries preventing scheduler/physics/audio scope creep before component-catalog stage exits.
- `2026-02-11`: Added stage-3 physics-core implementation kickoff packet in `docs/projects/core-engine-infrastructure.md` with:
  - entry checklist,
  - concrete required tests to run first,
  - exit-criteria checklist,
  - wrapper closeout reminders for `./scripts/test-engine-backends.sh <build-dir>` when applicable,
  - explicit boundaries preventing scheduler/component/audio creep before physics-core stage exits.
- `2026-02-11`: Added stage-4 physics-facade/override implementation kickoff packet in `docs/projects/core-engine-infrastructure.md` with:
  - entry checklist,
  - concrete required tests to run first,
  - exit-criteria checklist,
  - wrapper closeout reminders for `./scripts/test-engine-backends.sh <build-dir>` when applicable,
  - explicit boundaries preventing scheduler/component/audio/core-physics creep before physics-facade/override stage exits.
- `2026-02-11`: Added stage-5 audio-core implementation kickoff packet in `docs/projects/core-engine-infrastructure.md` with:
  - entry checklist,
  - concrete required tests to run first,
  - exit-criteria checklist,
  - wrapper closeout reminders for `./scripts/test-engine-backends.sh <build-dir>` when applicable,
  - explicit boundaries preventing scheduler/component/physics (including Stage-3/Stage-4 physics) creep before audio-core stage exits.
- `2026-02-11`: Added stage-6 audio-facade/override implementation kickoff packet in `docs/projects/core-engine-infrastructure.md` with:
  - entry checklist,
  - concrete required tests to run first,
  - exit-criteria checklist,
  - wrapper closeout reminders for `./scripts/test-engine-backends.sh <build-dir>` when applicable,
  - explicit boundaries preventing scheduler/component/physics (including Stage-3/Stage-4 physics) and Stage-5 audio-core creep before audio-facade/override stage exits.
- `2026-02-11`: Added consolidated stages 1-6 implementation handoff checklist in `docs/projects/core-engine-infrastructure.md`, mapping each stage to:
  - execution owner role,
  - required closeout evidence (tests + wrapper closeout expectations),
  - standardized handoff evidence format requirements.

## Open Questions
- Should component catalog metadata be maintained as docs-only tables, code-adjacent manifests, or both?
- Which advanced capability gates should be standardized first after core physics/audio semantics are locked?

## Handoff Checklist
- [x] Docs updated
- [x] Lint run and summarized
- [x] Risks/open questions listed
