# Specialist Task Packet Template

Use this template when assigning work to a specialist agent.
Replace placeholders with concrete values before sending.

Output requirement for overseer responses:
- When the human asks for a specialist prompt, return one single fenced `text` block that is directly copy-pastable.
- Fill concrete values for the selected slice; do not return placeholder-only/template skeleton output.

Placeholder legend:
- `<repo-root>`: specialist execution repository root (for example `m-karma` or `m-bz3`).
- `<read-doc-N>`: required read-order docs for that repository.
- `<project-doc>`: active project tracking document for this slice.
- `<assignments-doc>`: assignment/ownership board path for this repository.
- `<build-wrapper>`: approved build wrapper command for this repository.
- `<track-label>`: strategic track label used by your project.

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
- If you are already in `<repo-root>`, use unprefixed paths below.
- If you start at the multi-repo parent root, run `cd <repo-root>` first.
- If you remain at parent root, consistently prefix all repo-relative paths with `<repo-root>/`.

Read in order:
1) <read-doc-1>
2) <read-doc-2>
3) <read-doc-3>
4) <assignments-doc>
5) <project-doc>

Take ownership of: <project-doc>

Goal:
- <single concrete slice>

Scope:
- <exact behavior/API/test slice>

Strategic alignment (required):
- Track: <track-label>
- Explain how this slice advances the selected track without violating repository boundaries.

Constraints:
- Stay within owned paths and interface boundaries in <project-doc>.
- No unrelated subsystem changes.
- Preserve the architecture and boundary rules from the required read docs.
- If an external reference repo exists, treat it as capability reference only, not a structure/layout template.
- Use only approved wrapper commands for this repository (for example `<build-wrapper>`). Do not run raw toolchain commands unless explicitly allowed by repo policy.
- If required local setup is missing (for example local `./vcpkg`), report it as a blocker to overseer/human and stop build/test work for this slice.
- Use explicit specialist identity and slot ownership:
  - Set the required specialist identity env/flag for this repository.
  - Claim assigned slot(s) before first build when lock tooling exists.
  - Do not use unassigned slots or mismatched owner locks.
- Use only assigned build dirs/slots from <assignments-doc>.
- Use explicit build-dir args on every build command:
  - default-first pattern: `<build-wrapper> -c -d <dir>`
  - selector pattern (only when needed): `<build-wrapper> -c -d <dir> -b <token_csv>`
  - keep selector lists scoped to the categories touched by this slice.

Validation (required):
- <exact command 1>
- <exact command 2>

Docs updates (required):
- Update <project-doc> Project Snapshot + status/handoff checklist.
- Update <assignments-doc> row (owner/status/next-task/last-update).

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
1) <assignments-doc>
2) <project-doc>

Take ownership of: <project-doc>

Goal:
- <single concrete slice>

Scope:
- <exact behavior/API/test slice>

Strategic alignment (required):
- Track: <track-label>
- Keep scope aligned to standing bootstrap boundaries.

Constraints:
- Stay within owned paths and interface boundaries in <project-doc>.
- No unrelated subsystem changes.
- Preserve the architecture and boundary rules from standing bootstrap context.
- Use only approved wrapper commands for this repository (for example `<build-wrapper>`). Do not run raw toolchain commands unless explicitly allowed by repo policy.
- Treat missing required local setup as a hard blocker; report it to overseer/human and stop build/test work for this slice.
- Keep standing slot ownership in force:
  - reuse the same specialist identity mechanism,
  - keep work in the same claimed slot(s) unless overseer explicitly reassigns.
- Use only assigned build dirs listed in <assignments-doc>.
- Use explicit build-dir args on every build command:
  - default-first pattern: `<build-wrapper> -c -d <dir>`
  - selector pattern (only when needed): `<build-wrapper> -c -d <dir> -b <token_csv>`
  - keep backend lists scoped to the exact category/categories touched by the slice.

Validation (required):
- <exact command 1>
- <exact command 2>

Docs updates (required):
- Update <project-doc> Project Snapshot + status/handoff checklist.
- Update <assignments-doc> row (owner/status/next-task/last-update) when changed by this slice.

Handoff must include:
- files changed
- exact commands run + results
- remaining risks/open questions
```
