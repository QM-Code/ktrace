# Startup Brief

## Purpose
One-screen startup orientation for overseers and specialists.

This file is a pointer map, not a replacement for canonical policy docs.

## 60-Second Startup Checklist
1. Anchor to repo root:

```bash
cd m-rewrite
```

2. Read in order:
  - `AGENTS.md`
  - `docs/AGENTS.md`
  - `docs/projects/README.md`
  - `docs/projects/ASSIGNMENTS.md`
  - relevant `docs/projects/<project>.md`
  - `docs/DECISIONS.md`
3. If acting as overseer, run KARMA refresh before proposing targets:

```bash
git -C ../KARMA-REPO fetch --all --prune
```

4. Restate in your own words:
  - active tracks,
  - top overlap/conflict risks,
  - next recommended target(s).

## Canonical Ownership Map
- Rewrite invariants and architecture boundaries:
  - `AGENTS.md`
- Execution policy and validation/build rules:
  - `docs/AGENTS.md`
- Overseer rotation, startup, and checkpoint protocol:
  - `docs/OVERSEER.md`
- Live assignment board and build-dir ownership:
  - `docs/projects/ASSIGNMENTS.md`
- Project mission/scope/validation:
  - `docs/projects/*.md`
- Durable strategy and policy decisions:
  - `docs/DECISIONS.md`

## Current Strategic Posture
- Two mandatory tracks in parallel:
  - `m-dev` behavior parity for modern BZ3 outcomes,
  - KARMA capability intake under rewrite-owned contracts.
- Co-equal top priorities:
  - `docs/projects/renderer-parity.md`
  - `docs/projects/engine-network-foundation.md`

## Non-Negotiable Execution Guards
- Use `./bzbuild.py <build-dir>` for delegated configure/build/test workflows.
- Do not run raw `cmake -S/-B` as delegated operator workflow.
- Use assigned isolated build dirs and explicit wrapper build-dir arguments in parallel work.
- Preserve engine/game ownership boundaries (engine owns lifecycle/default scaffolding; game owns BZ3 rules/protocol semantics).
