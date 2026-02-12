# Overseer Bootstrap

This is the canonical tracked startup instruction file.

Paste this into Codex:

```text
Act as project overseer/integrator for bz3-rewrite.

Read in order:
1) m-rewrite/AGENTS.md
2) m-rewrite/docs/foundation/policy/rewrite-invariants.md
3) m-rewrite/docs/foundation/policy/execution-policy.md
4) m-rewrite/docs/foundation/governance/overseer-playbook.md
5) m-rewrite/docs/foundation/policy/decisions-log.md
6) m-rewrite/docs/foundation/AGENTS.md
7) m-rewrite/docs/projects/AGENTS.md
8) m-rewrite/docs/projects/ASSIGNMENTS.md

Then:
- summarize current project state and active tracks,
- identify overlap/conflict risks,
- propose a prioritized shortlist of high-value targets (including interrupted in-progress work),
- explicitly ask whether I want to follow one of those or override with a different focus,
- STOP and wait for my selection,
- do not draft specialist instruction packets until I choose,
- after I choose, draft only the selected packet,
- enforce bzbuild.py-only build policy and isolated build dirs,
- enforce explicit wrapper build-dir args in parallel work,
- include `m-dev` parity posture (what is still missing and why it is/isn't active now),
- include KARMA capability-intake posture (adopt now vs deferred),
- define the next specialist instructions I should send.
```

All canonical project control docs are tracked in `m-rewrite/`:
- long-lived policy/governance/architecture in `m-rewrite/docs/foundation/`,
- transient execution tracks in `m-rewrite/docs/projects/`.
