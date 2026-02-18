# Specialist Task Packet Template

Use this template when assigning work to a specialist agent.

Output requirement for overseer responses:
- When the human asks for a specialist prompt, return one single fenced `text` block that is directly copy-pastable.
- Fill concrete values for the selected slice; do not return placeholder-only/template skeleton output.

Bootstrap cadence policy:
- Use a `bootstrap packet` when any of these are true:
  - first packet for a specialist in the current session,
  - ownership moved to a different specialist,
  - human explicitly says `refresh bootstrap`,
  - specialist reports context compaction/summarization/reset.
- Otherwise use a `delta packet` for follow-up slices in the same specialist session.
- Operator recommendation to include in each new specialist bootstrap packet:
  - "If this specialist later reports context compaction/summarization, run `refresh bootstrap` before the next coding slice to restore full project-policy alignment."

## Bootstrap Packet (Full Read)

```text
Execution root:
- Standalone mode: if you are already in `m-rewrite` repo root, use unprefixed paths below.
- Integration mode: if you start at workspace root (`bz3-rewrite/`), run `cd m-rewrite` first.
- If you stay at integration workspace root, prefix every path below with `m-rewrite/`.

Read in order:
1) docs/AGENTS.md
2) docs/foundation/policy/execution-policy.md
3) docs/projects/AGENTS.md
4) docs/projects/ASSIGNMENTS.md
5) docs/projects/<project>.md

Take ownership of: docs/projects/<project>.md

Goal:
- <single concrete slice>

Scope:
- <exact behavior/API/test slice>

Strategic alignment (required):
- Track: <m-dev parity | q-karma capability intake | shared unblocker>
- Explain how this slice advances the selected track without violating rewrite ownership boundaries.

Constraints:
- Stay within owned paths and interface boundaries in docs/projects/<project>.md.
- No unrelated subsystem changes.
- Preserve engine/game and backend exposure boundaries from docs/AGENTS.md.
- Treat `q-karma` as capability reference only (never structure/layout template).
- Use abuild.py only. Do not run raw cmake -S/-B directly.
- Treat missing/unbootstrapped local `./vcpkg` as a hard blocker; report it to overseer/human and stop build/test work for that slice.
- Use explicit specialist identity and slot ownership:
  - Set `ABUILD_AGENT_NAME=<specialist-name>` (or pass `--agent <specialist-name>`).
  - Claim assigned slot(s) before first build: `./abuild.py --claim-lock -d <dir>`.
  - Do not use unassigned slots or mismatched owner locks.
- Use only assigned build dirs:
  - <dir A>
  - <dir B>
- Use explicit build-dir args on every build command:
  - default-first pattern: `./abuild.py -c -d <dir>`
  - backend-selective pattern (only when needed): `./abuild.py -c -d <dir> -b <token_csv>`
  - renderer dual-backend example: `./abuild.py -c -d <dir> -b bgfx,diligent`
  - ui dual-backend example: `./abuild.py -c -d <dir> -b imgui,rmlui`
  - physics dual-backend example: `./abuild.py -c -d <dir> -b jolt,physx`
  - never request broad multi-category backend lists unless the slice explicitly requires each category.

Validation (required):
- <exact command 1>
- <exact command 2>

Docs updates (required):
- Update docs/projects/<project>.md Project Snapshot + status/handoff checklist.
- Update docs/projects/ASSIGNMENTS.md row (owner/status/next-task/last-update).

Handoff must include:
- files changed
- exact commands run + results
- remaining risks/open questions
```

## Delta Packet (No Re-bootstrap)

```text
Use standing bootstrap context from this specialist session.
Do not re-read foundation/bootstrap docs unless the human says: refresh bootstrap.

Read only:
1) docs/projects/ASSIGNMENTS.md
2) docs/projects/<project>.md

Take ownership of: docs/projects/<project>.md

Goal:
- <single concrete slice>

Scope:
- <exact behavior/API/test slice>

Strategic alignment (required):
- Track: <m-dev parity | q-karma capability intake | shared unblocker>
- Keep scope aligned to standing bootstrap boundaries.

Constraints:
- Stay within owned paths and interface boundaries in docs/projects/<project>.md.
- No unrelated subsystem changes.
- Preserve engine/game and backend exposure boundaries from standing bootstrap context.
- Use abuild.py only. Do not run raw cmake -S/-B directly.
- Treat missing/unbootstrapped local `./vcpkg` as a hard blocker; report it to overseer/human and stop build/test work for that slice.
- Keep standing slot ownership in force:
  - reuse the same `ABUILD_AGENT_NAME`,
  - keep work in the same claimed slot(s) unless overseer explicitly reassigns.
- Use only assigned build dirs listed in docs/projects/ASSIGNMENTS.md.
- Use explicit build-dir args on every build command:
  - default-first pattern: `./abuild.py -c -d <dir>`
  - backend-selective pattern (only when needed): `./abuild.py -c -d <dir> -b <token_csv>`
  - renderer dual-backend example: `./abuild.py -c -d <dir> -b bgfx,diligent`
  - ui dual-backend example: `./abuild.py -c -d <dir> -b imgui,rmlui`
  - physics dual-backend example: `./abuild.py -c -d <dir> -b jolt,physx`
  - keep backend lists scoped to the exact category/categories touched by the slice.

Validation (required):
- <exact command 1>
- <exact command 2>

Docs updates (required):
- Update docs/projects/<project>.md Project Snapshot + status/handoff checklist.
- Update docs/projects/ASSIGNMENTS.md row (owner/status/next-task/last-update) when changed by this slice.

Handoff must include:
- files changed
- exact commands run + results
- remaining risks/open questions
```
