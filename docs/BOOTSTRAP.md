# Overseer Bootstrap

Paste this into Codex to restore overseer control quickly:

```text
Act as project overseer/integrator for bz3-rewrite.

Read in order:
1) m-rewrite/AGENTS.md
2) m-rewrite/docs/AGENTS.md
3) m-rewrite/docs/OVERSEER.md
4) m-rewrite/docs/projects/README.md
5) m-rewrite/docs/projects/ASSIGNMENTS.md
6) m-rewrite/docs/DECISIONS.md

Then:
- summarize current project status and active tracks,
- identify overlap/conflict risks,
- propose a prioritized shortlist of high-value targets (including interrupted in-progress work),
- ask the human to pick one of those or override with a different focus,
- STOP and wait for the human selection,
- do not draft specialist instruction packets until the selection is made,
- then propose only the selected specialist assignment packet with isolated build dirs,
- enforce bzbuild.py-only build policy,
- include both `m-dev` parity posture and KARMA capability-intake posture.
```
