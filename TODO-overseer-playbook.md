# Overseer Playbook

## Purpose
This file defines overseer-only workflow:
- startup alignment and triage,
- specialist assignment/rotation protocol,
- accepted-slice checkpoint gate.

Execution command policy (for all specialists) is canonical in `docs/foundation/policy/execution-policy.md`.

## Overseer Responsibilities
1. Keep direction aligned with `docs/AGENTS.md`, `docs/foundation/policy/execution-policy.md`, and `docs/foundation/policy/decisions-log.md`.
2. Convert user goals into bounded specialist packets.
3. Prevent overlap by enforcing owned paths and assigned build dirs from `docs/projects/ASSIGNMENTS.md`.
4. Review specialist handoffs for scope, validation, risks, and boundary integrity.
5. Keep `docs/projects/ASSIGNMENTS.md` plus project snapshot fields current.
6. Maintain durable memory in `docs/foundation/policy/decisions-log.md`.
7. Enforce default-first direction (`95% defaults / 5% overrides`).
8. In integration mode, run continuous `q-karma` capability intake without structure mirroring.
9. Persist accepted work via overseer checkpoint commits/pushes.

## Startup Protocol (Required)
1. Anchor to repo root:

```bash
cd m-rewrite
```

2. Read in order:
  - `docs/BOOTSTRAP.md`
  - then execute the read order defined in `docs/BOOTSTRAP.md`
3. Restate startup alignment:
  - engine owns lifecycle/subsystems; game owns BZ3 rules/protocol semantics,
  - default-first assignment posture.
4. Integration mode only: run q-karma refresh gate before proposing targets:

```bash
git -C ../q-karma fetch --all --prune
```

5. Integration mode only: summarize q-karma freshness in startup output:
  - new remote branches (if any),
  - notable upstream head commits,
  - intake candidates (adopt now / defer).
  - If fetch fails, mark q-karma state stale explicitly.

## Current Priority Override
- Renderer capability parity and q-karma lighting/shadow parity are co-equal P0 priorities.
- Active execution is consolidated under `docs/projects/karma-lighting-parity.md`; prioritize it ahead of non-blocking audio/content-mount/UI and queued backend follow-up.
- Convert q-karma feature intent into rewrite-owned contracts/docs.

## q-karma Intake Loop (Integration Mode)
1. Detect upstream capability deltas.
2. Triage significance (default-path leverage, parity unblock, rework reduction).
3. Reframe accepted deltas into rewrite project slices with owned paths and validation.
4. Schedule under current rewrite priorities and conflict constraints.
5. Close local memory by updating rewrite docs/decisions so direction is q-karma-independent.

## Assignment Protocol
When issuing a specialist packet:
- use `docs/foundation/governance/handoff-template.md`,
- include explicit owned paths, non-goals, and conflict hotspots,
- include explicit specialist agent identity (stable name for lock ownership),
- include assigned isolated build dirs from `docs/projects/ASSIGNMENTS.md`,
- enforce `./abuild.py -c -d <build-dir>`-first operator flows (omit `-c` only for intentional reuse of configured dirs),
- require `ABUILD_AGENT_NAME` (or `abuild.py --agent`) plus slot lock lifecycle (`abuild.py --claim-lock` at start, `abuild.py --release-lock` on retire/transfer),
- keep specialist packets focused on assigned pre-provisioned build slots; avoid setup/bootstrap chores in specialist slices,
- if local `./vcpkg` or slot readiness is missing, escalate to overseer/human as a blocker instead of spending specialist context on environment setup,
- require test/demo data paths under `demo/` (`demo/communities`, `demo/users`, `demo/worlds`) unless explicitly justified otherwise,
- require project-specific validation and wrapper gates with explicit build-dir args.
- use full specialist bootstrap packet once per specialist session, then use delta packets for follow-up slices.
- provide the packet as a single fully copy-pastable fenced `text` block with concrete instructions (no placeholders/template skeleton).

## Specialist Bootstrap Refresh Policy
- Full specialist bootstrap packet is required when any of these are true:
  - first packet for a specialist in the current session,
  - ownership changes to a different specialist,
  - human explicitly says `refresh bootstrap`,
  - specialist reports context compaction/summarization/reset.
- Otherwise issue a delta packet that references standing bootstrap context and only project-level reads.
- On each new specialist bootstrap packet, include this operator recommendation:
  - "If this specialist later reports context compaction/summarization, run `refresh bootstrap` before the next coding slice to restore policy/project alignment."

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
  - record major cross-track decisions in `docs/foundation/policy/decisions-log.md`.
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
Act as project overseer/integrator for m-rewrite.

Read in order:
1) docs/BOOTSTRAP.md

Then:
- execute all startup/read-order and output requirements defined in that file.
```

Integration-mode summon template is in:
- `docs/overseer/BOOTSTRAP.md`

### Retire Specialist
```text
This slice is accepted and complete. Stop coding on this track.

Do only:
1) final handoff summary (files/tests/risks),
2) ensure project snapshot + ASSIGNMENTS row are current,
3) release assigned slot locks (`./abuild.py --release-lock -d <build-dir>` with matching `ABUILD_AGENT_NAME`).
```

### Start Specialist (Bootstrap)
Use `docs/foundation/governance/handoff-template.md` bootstrap packet and fill only one selected track packet.

### Continue Specialist (Delta)
Use `docs/foundation/governance/handoff-template.md` delta packet for follow-up slices in the same specialist session.
