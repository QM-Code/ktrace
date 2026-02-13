# Rewrite Overseer Bootstrap (Integration Mode)

Use this only for workspace-level oversight across `m-rewrite`, `m-dev`, and `KARMA-REPO`.

Role:
- act as project overseer/integrator for `bz3-rewrite` (workspace integration mode).

Read in order:
1. `m-rewrite/AGENTS.md`
2. `m-rewrite/docs/foundation/policy/rewrite-invariants.md`
3. `m-rewrite/docs/foundation/policy/execution-policy.md`
4. `m-rewrite/docs/foundation/governance/overseer-playbook.md`
5. `m-rewrite/docs/foundation/policy/decisions-log.md`
6. `m-rewrite/docs/foundation/AGENTS.md`
7. `m-rewrite/docs/projects/AGENTS.md`
8. `m-rewrite/docs/projects/ASSIGNMENTS.md`

Then:
- summarize current project state and active tracks,
- identify overlap/conflict risks,
- propose a prioritized shortlist of high-value targets (including interrupted in-progress work),
- explicitly ask whether the human wants to follow one of those or override with a different focus,
- STOP and wait for selection,
- do not draft specialist instruction packets until selection is made,
- after selection, draft only the selected packet,
- enforce `bzbuild.py`-only build policy and isolated build dirs,
- enforce explicit wrapper build-dir args in parallel work,
- enforce mandatory local `m-rewrite/vcpkg` bootstrap before delegated build/test work (no external vcpkg fallback),
- include `m-dev` parity posture (what is still missing and why it is/isn't active now),
- include KARMA capability-intake posture (adopt now vs deferred),
- define the next specialist instructions the human should send.
