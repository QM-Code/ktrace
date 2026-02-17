# Core Engine Infrastructure (Execution Track)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress`
- Immediate next task: keep active project slices aligned to the staged contract rollout in `docs/foundation/architecture/core-engine-contracts.md`.
- Validation gate: run touched-project gates; for cross-scope slices run both `./scripts/test-engine-backends.sh <build-dir>` and `./scripts/test-server-net.sh <build-dir>`.

## Mission
Coordinate rollout of core engine contract implementation across active projects without duplicating or redefining canonical architecture contracts.

## Canonical Contract Source
- `docs/foundation/architecture/core-engine-contracts.md` (authoritative)
- `docs/foundation/architecture/engine-defaults-model.md`
- `docs/foundation/architecture/engine-defaults-program.md`

This project file is intentionally transient and should only track execution sequencing and integration state.

## Integration Scope
- Stage sequencing across scheduler/component/physics/audio contracts.
- Cross-project boundary checks for renderer/network/content/UI/physics work.
- Assignment coordination and dependency ordering.

## Non-Goals
- Do not restate full architecture contracts here.
- Do not duplicate policy/governance text from `docs/foundation/policy/*` or `docs/foundation/governance/*`.

## Active Sequencing Focus
1. Keep renderer P0 tracks unblocked while preserving core contract boundaries.
2. Continue physics/audio contract hardening under the staged model.
3. Keep content-mount and netcode changes aligned to engine-owned contract boundaries.
4. Surface cross-project risks early in `docs/projects/ASSIGNMENTS.md`.

## Coordination Points
- `docs/projects/physics-backend.md`
- `docs/projects/karma-lighting-shadow-parity.md`
- `docs/projects/content-mount.md`
- `docs/projects/gameplay-netcode.md`

## Validation
From `m-rewrite/`:

```bash
./docs/scripts/lint-project-docs.sh
```

For code-touch slices, use project-specific gates with explicit build-dir args.

## Open Questions
- Do scheduler/component implementation slices need a dedicated active project doc, or should they remain governed through this integration tracker plus per-subsystem project docs?
- Should staged closeout evidence move into a dedicated governance ledger, or stay in project handoffs only?

## Handoff Checklist
- [ ] Assignment dependencies updated.
- [ ] Cross-project boundary risks called out.
- [ ] Canonical contracts unchanged (or changed only in foundation docs).
- [ ] Validation commands and results recorded.
