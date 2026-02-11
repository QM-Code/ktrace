# Specialist Task Packet Template

Use this template when assigning work to a specialist agent.

```text
Execution root:
- If you start at workspace root (`bz3-rewrite/`), run `cd m-rewrite` first.
- If you stay at workspace root, prefix every path below with `m-rewrite/`.

Read in order:
1) AGENTS.md
2) docs/AGENTS.md
3) docs/projects/README.md
4) docs/projects/ASSIGNMENTS.md
5) docs/projects/<project>.md

Take ownership of: docs/projects/<project>.md

Goal:
- <single concrete slice>

Scope:
- <exact behavior/API/test slice>

Strategic alignment (required):
- Track: <m-dev parity | KARMA capability intake | shared unblocker>
- Explain how this slice advances the selected track without violating rewrite ownership boundaries.

Constraints:
- Stay within owned paths and interface boundaries in docs/projects/<project>.md.
- No unrelated subsystem changes.
- Preserve engine/game and backend exposure boundaries from AGENTS.md.
- Treat `KARMA-REPO` as capability reference only (never structure/layout template).
- Use bzbuild.py only. Do not run raw cmake -S/-B directly.
- Use only assigned build dirs:
  - <dir A>
  - <dir B>

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
