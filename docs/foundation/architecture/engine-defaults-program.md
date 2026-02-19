# Engine Defaults Program

This document tracks long-lived architecture rollout posture for defaults-model adoption.

Canonical contracts:
- `docs/foundation/architecture/engine-defaults-model.md`
- `docs/foundation/architecture/core-engine-contracts.md`

Execution tracking surfaces:
- `docs/projects/ASSIGNMENTS.md` (active owner/status/next-task board)
- subsystem project docs in `docs/projects/`

## Program Objectives
1. Keep `95% defaults / 5% overrides` policy enforceable in code and tests.
2. Preserve engine-owned boundaries while expanding capability parity.
3. Ensure staged implementation order remains explicit and reviewable.

## Canonical Adoption Sequence
1. Scheduler core contract.
2. Component catalog core contract.
3. Physics core contract, then physics facade/override boundaries.
4. Audio core contract, then audio facade/override boundaries.

Authoritative stage definitions, tests, and handoff evidence are maintained in:
- `docs/foundation/architecture/core-engine-contracts.md`

## Coordination Scope
- Stage sequencing across scheduler/component/physics/audio contracts.
- Cross-project boundary checks for renderer/network/content/UI/physics work.
- Dependency ordering across active project slices and assignment packets.

## Current Program Posture
- Scheduler/component/physics/audio staged contract definitions are codified.
- Ongoing execution status is tracked in `docs/projects/ASSIGNMENTS.md` and project docs, not as per-slice logs in this foundation file.
- Any change to staged contract semantics must be made in foundation docs first, then reflected in active project tasks.

## Active Sequencing Focus
1. Keep renderer parity tracks unblocked while preserving core contract boundaries.
2. Continue physics/audio contract hardening under the staged model.
3. Keep content-mount and gameplay-netcode changes aligned to engine-owned contract boundaries.
4. Surface cross-project risks early in `docs/projects/ASSIGNMENTS.md`.

## Validation Coordination
From `m-rewrite/`:

```bash
./docs/scripts/lint-project-docs.sh
```

For cross-scope code slices, run both wrapper gates with explicit build-dir args:
- `./scripts/test-engine-backends.sh <build-dir>`
- `./scripts/test-server-net.sh <build-dir>`

## Change Control Rules
- Do not put per-slice owner/status/handoff logs in this file.
- Do not duplicate full contract text from `core-engine-contracts.md`.
- Do not duplicate policy/governance text from `docs/foundation/policy/*` or `docs/foundation/governance/*`.
- Do not reference archived project paths as active guidance.

## Open Questions
- Should scheduler/component implementation slices use dedicated project docs, or remain coordinated through subsystem docs plus assignment ordering?
- Should staged closeout evidence live in a dedicated governance ledger, or remain in project handoffs?
- Should component metadata live docs-first, code-adjacent, or both?
- Which advanced capability gates should be standardized first after core closure?
