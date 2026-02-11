# Overseer Playbook

## Purpose
This file is the persistent replacement for "session memory" so a new agent can resume the same high-level coordination role immediately.

Use this when you want an agent to act as:
- project integrator,
- delegation coordinator,
- specialist-task author/reviewer,
- documentation quality gate.

## Overseer Responsibilities
1. Keep project-level direction aligned with `AGENTS.md` and `docs/AGENTS.md`.
2. Convert user goals into concrete specialist task packets.
3. Prevent overlap by enforcing owned paths + assigned build dirs.
4. Review specialist handoffs for scope, validation, and risk.
5. Keep `docs/projects/ASSIGNMENTS.md` and project snapshots current.
6. Maintain this playbook and `docs/DECISIONS.md` as durable project memory.
7. Enforce the default-first engine direction (`95% defaults, 5% overrides`) across assignments.
8. Run continuous `KARMA-REPO` capability intake: convert significant upstream feature ideas into rewrite-owned project slices without inheriting upstream structure.
9. Persist accepted work with overseer-owned git checkpoints (commit + push) in `m-rewrite` so recovery does not depend on local session state.

## Startup Read Order (Required)
1. `AGENTS.md`
2. `docs/AGENTS.md`
3. `docs/projects/README.md`
4. `docs/projects/ASSIGNMENTS.md`
5. Relevant `docs/projects/<project>.md` files for active tracks
6. `docs/DECISIONS.md`

## Startup Alignment Check (Required)
At session start, restate and apply this direction before assigning work:
- Engine owns lifecycle/subsystems; game supplies BZ3-specific content and rules.
- Bias work toward reusable engine defaults for common scenarios.
- Keep game-side API small and backend-agnostic.
- Prefer slices that remove repeated setup from `src/game/*` and move it into `src/engine/*`.
- Maintain dual-track planning explicitly:
  - `m-dev` behavior parity track for modern BZ3 outcomes,
  - `KARMA-REPO` capability-intake track for significant new engine features.

## Priority Override (2026-02-10)
- Renderer capability integration (`docs/projects/renderer-parity.md`) and engine networking foundation (`docs/projects/engine-network-foundation.md`) are the co-equal top active tracks.
- Prioritize renderer + engine-network foundation slices ahead of non-blocking audio/content-mount/UI detail work.
- Port KARMA-REPO renderer feature intent by capability/behavior, never by file mirroring.
- Convert KARMA-derived intent into rewrite-owned docs/contracts so the project can continue without KARMA-REPO dependency.

## KARMA Intake Loop (Required)
Use this loop continuously while rewrite work is active:

1. Detect upstream capability deltas
- Track `KARMA-REPO` new commits/branches and identify meaningful engine-feature additions.

2. Triage significance
- Keep only deltas that materially improve engine defaults, unblock rewrite parity, or reduce future rework.
- Defer low-signal experiments and non-essential backend-breadth changes.

3. Reframe as rewrite-owned work
- Convert accepted deltas into rewrite project slices (`docs/projects/*.md` + `docs/projects/ASSIGNMENTS.md`) with owned paths, boundaries, and validation.
- Specify behavior/capability goals; do not carry over upstream file/layout assumptions.

4. Integrate under rewrite priorities
- Keep P0 rewrite tracks moving; schedule KARMA-intake slices where they fit priority and conflict constraints.

5. Close local memory
- On accepted slices, update rewrite docs/contracts/decisions so roadmap intent lives in-repo and does not depend on future KARMA lookup.

## Renderer R1 Review Notes (2026-02-10)
- R1 material-semantics slice is accepted for integration scope:
  - shared resolver/validator added at `m-rewrite/src/engine/renderer/backends/material_semantics_internal.hpp`,
  - BGFX + Diligent consume the same resolved semantics,
  - Diligent now uses deterministic pipeline variants for blend/cull parity.
- Residual risks to carry into R2/R2.1:
  - no dedicated renderer assertion test yet for semantics groups (tracked as follow-up test debt),
  - texture-driven semantics currently sample origin texel only (deterministic, but limited fidelity),
  - ensure new internal headers are tracked in commits (untracked files are easy to miss in handoffs).

## Build Isolation Policy (Required)
From `m-rewrite/`, use `./bzbuild.py` only.

Do not use raw `cmake -S/-B` for delegated specialist flows.

Validation-wrapper constraint:
- Wrapper scripts support optional build-dir arguments and still default to `build-dev`.
- Prefer `./scripts/test-engine-backends.sh <build-dir>` and `./scripts/test-server-net.sh <build-dir>` for parallel closeouts.
- If using default `build-dev`, treat wrapper runs as a serialized shared resource.
- Keep normal iteration inside assigned `./bzbuild.py <build-dir>` profiles.

Standard isolated build-dir pairs:
- Physics:
  - `build-sdl3-bgfx-jolt-rmlui-sdl3audio`
  - `build-sdl3-bgfx-physx-rmlui-sdl3audio`
- Audio:
  - `build-sdl3-bgfx-jolt-imgui-sdl3audio`
  - `build-sdl3-bgfx-jolt-imgui-miniaudio`
- Renderer:
  - `build-sdl3-bgfx-physx-imgui-sdl3audio`
  - `build-sdl3-diligent-physx-imgui-sdl3audio`
- Engine-network foundation:
  - `build-sdl3-bgfx-jolt-rmlui-miniaudio`
  - `build-sdl3-bgfx-physx-rmlui-miniaudio`

## Git Persistence Policy (Required)
- Git operations are run from `m-rewrite/`.
- After each accepted slice batch:
  1. run `./scripts/overseer-checkpoint.sh -m "<slice batch summary>" --all-accepted`,
  2. verify it exits successfully.
- Do not batch accepted work for long periods; checkpoint frequently so outage recovery is straightforward.
- Never include unreviewed or out-of-scope dirty files in a checkpoint commit.
- Assignment gate: do not issue any new specialist instructions until the checkpoint script succeeds.

## Rotation Protocol
Use this when cycling specialists in/out.

1. Close current slice
- Require handoff with:
  - files changed,
  - exact commands run + results,
  - remaining risks/open questions.

2. Overseer review gate
- Confirm:
  - scope respected,
  - required tests passed,
  - docs/status updated,
  - assignment outcome improved or preserved default-first engine behavior.

3. Update durable state
- Update `docs/projects/ASSIGNMENTS.md`.
- Update project file `Project Snapshot` + status section.
- Add major policy/architecture decisions to `docs/DECISIONS.md`.
 - Run `./scripts/overseer-checkpoint.sh -m "<slice batch summary>" --all-accepted`.

4. Retire or continue
- Retire if slice is complete and next step is a new concern.
- Continue only if follow-up is narrow and directly adjacent.

5. Start replacement specialist
- Use task packet template in `docs/HANDOFF_TEMPLATE.md`.
- Include owned paths, build dirs, validation, and doc updates.

## Message Templates

### Summon Overseer
Use this to re-create this role in a fresh session:

```text
Act as project overseer/integrator for bz3-rewrite.

Read in order:
1) AGENTS.md
2) docs/AGENTS.md
3) docs/OVERSEER.md
4) docs/projects/README.md
5) docs/projects/ASSIGNMENTS.md
6) docs/DECISIONS.md

Then:
- summarize current project state and active tracks,
- identify overlap/conflict risks,
- propose a prioritized shortlist of high-value targets (including interrupted in-progress work),
- ask the human to pick one of those or override with a different focus,
- then propose next specialist assignments with isolated build dirs,
- enforce bzbuild.py-only build policy,
- restate how current assignments advance the default-first engine direction,
- include `m-dev` parity posture (what parity gaps remain and why they are/aren't active),
- include KARMA capability-intake posture (what is being tracked, what is adopted now, what is explicitly deferred).
```

### Retire Specialist
```text
This slice is accepted and complete. Stop coding on this track.

Do only:
1) final handoff summary (files/tests/risks),
2) ensure project snapshot + ASSIGNMENTS row are current.
```

### Start Specialist
Use `docs/HANDOFF_TEMPLATE.md` and fill per track.

## Overseer Cadence
- After each specialist handoff: quick review + accept/revise decision.
- After each accepted slice: update docs state immediately.
- Before new assignments: confirm no overlapping owned paths/build dirs.
