# Overseer Playbook

## Purpose
This file defines overseer-only workflow:
- startup alignment and triage,
- specialist assignment/rotation protocol,
- accepted-slice checkpoint gate.

Execution command policy (for all specialists) is canonical in `docs/AGENTS.md`.

## Overseer Responsibilities
1. Keep direction aligned with `AGENTS.md`, `docs/AGENTS.md`, and `docs/DECISIONS.md`.
2. Convert user goals into bounded specialist packets.
3. Prevent overlap by enforcing owned paths and assigned build dirs from `docs/projects/ASSIGNMENTS.md`.
4. Review specialist handoffs for scope, validation, risks, and boundary integrity.
5. Keep `docs/projects/ASSIGNMENTS.md` plus project snapshot fields current.
6. Maintain durable memory in `docs/DECISIONS.md`.
7. Enforce default-first direction (`95% defaults / 5% overrides`) and dual-track framing.
8. Run continuous `KARMA-REPO` capability intake without structure mirroring.
9. Persist accepted work via overseer checkpoint commits/pushes.

## Startup Protocol (Required)
1. Anchor to repo root:

```bash
cd m-rewrite
```

2. Read in order:
  - `docs/STARTUP_BRIEF.md`
  - `AGENTS.md`
  - `docs/AGENTS.md`
  - `docs/projects/README.md`
  - `docs/projects/ASSIGNMENTS.md`
  - active `docs/projects/<project>.md` files
  - `docs/DECISIONS.md`
3. Restate startup alignment:
  - engine owns lifecycle/subsystems; game owns BZ3 rules/protocol semantics,
  - default-first assignment posture,
  - dual-track posture (`m-dev` parity + KARMA capability intake).
4. Run KARMA refresh gate before proposing targets:

```bash
git -C ../KARMA-REPO fetch --all --prune
```

5. Summarize KARMA freshness in startup output:
  - new remote branches (if any),
  - notable upstream head commits,
  - intake candidates (adopt now / defer).
  - If fetch fails, mark KARMA state stale explicitly.

## Current Priority Override
- Renderer parity and renderer-shadow-hardening are co-equal P0 tracks.
- Prioritize these ahead of non-blocking audio/content-mount/UI and queued backend follow-up.
- Convert KARMA feature intent into rewrite-owned contracts/docs.

## KARMA Intake Loop (Required)
1. Detect upstream capability deltas.
2. Triage significance (default-path leverage, parity unblock, rework reduction).
3. Reframe accepted deltas into rewrite project slices with owned paths and validation.
4. Schedule under current rewrite priorities and conflict constraints.
5. Close local memory by updating rewrite docs/decisions so direction is KARMA-independent.

## Assignment Protocol
When issuing a specialist packet:
- use `docs/HANDOFF_TEMPLATE.md`,
- include explicit owned paths, non-goals, and conflict hotspots,
- include assigned isolated build dirs from `docs/projects/ASSIGNMENTS.md`,
- enforce `./bzbuild.py <build-dir>`-only operator flows,
- require project-specific validation and wrapper gates with explicit build-dir args.

## Review and Rotation Protocol
1. Close current slice with handoff evidence:
  - files changed,
  - exact commands + outcomes,
  - open risks/questions.
2. Review gate:
  - scope boundaries respected,
  - required tests/wrappers passed,
  - docs/snapshot/assignment updates complete,
  - default-first direction preserved.
3. Update durable state:
  - update `docs/projects/ASSIGNMENTS.md`,
  - update project `Project Snapshot` and status section,
  - record major cross-track decisions in `docs/DECISIONS.md`.
4. Decide retire vs continue:
  - retire complete slice,
  - continue only narrow adjacent follow-up.

## Checkpoint Gate (Hard Requirement)
After accepted slice batches, run from `m-rewrite/`:

```bash
./scripts/overseer-checkpoint.sh -m "<slice batch summary>" --all-accepted
```

Rules:
- verify successful exit before issuing any new specialist assignment,
- do not include unreviewed/out-of-scope dirty files,
- checkpoint frequently to preserve outage recovery.

## Message Templates

### Summon Overseer
```text
Act as project overseer/integrator for bz3-rewrite.

Read in order:
1) m-rewrite/docs/STARTUP_BRIEF.md
2) m-rewrite/AGENTS.md
3) m-rewrite/docs/AGENTS.md
4) m-rewrite/docs/OVERSEER.md
5) m-rewrite/docs/projects/README.md
6) m-rewrite/docs/projects/ASSIGNMENTS.md
7) m-rewrite/docs/DECISIONS.md

Then:
- summarize current project state and active tracks,
- identify overlap/conflict risks,
- propose a prioritized shortlist of high-value targets (including interrupted in-progress work),
- ask the human to pick one of those or override with a different focus,
- STOP and wait for the human selection,
- do not draft specialist instruction packets until the selection is made,
- then draft only the selected specialist packet with isolated build dirs,
- enforce bzbuild.py-only build policy and explicit wrapper build-dir args,
- include `m-dev` parity posture and KARMA intake posture (adopt now vs defer).
```

### Retire Specialist
```text
This slice is accepted and complete. Stop coding on this track.

Do only:
1) final handoff summary (files/tests/risks),
2) ensure project snapshot + ASSIGNMENTS row are current.
```

### Start Specialist
Use `docs/HANDOFF_TEMPLATE.md` and fill only one selected track packet.
