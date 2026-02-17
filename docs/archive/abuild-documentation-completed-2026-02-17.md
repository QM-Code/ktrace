# ABuild Documentation (Archived)

Archived from `docs/projects/abuild-documentation.md` on `2026-02-17`.

## Project Snapshot
- Current owner: `codex`
- Status: `complete (canonical abuild usage synchronized; default-first policy locked)`
- Immediate next task: maintenance only: monitor new/edited docs for `abuild.py` command-shape drift and update examples when policy/CLI behavior changes.
- Validation gate: `./docs/scripts/lint-project-docs.sh`

## Mission
Get `abuild.py` documentation up to date so agents know the correct usage.

## Foundation References
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/governance/overseer-playbook.md`
- `AGENTS.md`

## Why This Is Separate
`abuild.py` usage is now a cross-project execution dependency (agent identity, slot lock ownership, backend selection rules). Keeping this in a dedicated track prevents policy drift across project docs and specialist prompts.

## Owned Paths
- `docs/archive/abuild-documentation-completed-2026-02-17.md`
- `docs/projects/ASSIGNMENTS.md`
- `docs/foundation/policy/execution-policy.md`
- `docs/foundation/governance/overseer-playbook.md`
- `AGENTS.md`
- Any project doc under `docs/projects/*.md` that still documents outdated `abuild.py` usage.

## Interface Boundaries
- Inputs consumed:
  - current `abuild.py` CLI behavior and lock model.
  - current delegation/build rules from foundation policy docs.
- Outputs/contracts exposed:
  - one canonical, copyable usage reference for specialists.
  - synchronized usage wording across policy/governance/project docs.
- Coordinate before changing:
  - `docs/foundation/policy/execution-policy.md`
  - `docs/foundation/governance/overseer-playbook.md`
  - `docs/projects/ASSIGNMENTS.md`

## Non-Goals
- Do not redesign `abuild.py` behavior in this track.
- Do not change backend implementation/runtime contracts in this track.
- Do not expand into unrelated renderer/gameplay/physics feature slices.

## Verified `abuild.py` Behavior (Source: `abuild.py`, 2026-02-17)
- Lock lifecycle:
  - claim: `./abuild.py --claim-lock -d <build-dir>`
  - release: `./abuild.py --release-lock -d <build-dir>`
  - status: `./abuild.py --lock-status -d <build-dir>`
- Agent identity:
  - from `ABUILD_AGENT_NAME` or `--agent <name>`.
- Build targeting:
  - use `-d <build-dir>` to target an assigned slot; build-dir names must start with `build-`.
- Backend selection:
  - `-b` accepts comma-separated backend tokens across categories.
  - omitted categories use defaults.
  - one token in a category selects that backend.
  - multiple tokens in a category create a runtime-selectable set in one binary.
- Examples confirmed:
  - `./abuild.py -c -d <build-dir> -b bgfx,diligent` builds one binary with runtime-selectable BGFX/Diligent renderer backends.
  - `./abuild.py -c -d <build-dir> -b imgui,rmlui` builds one binary with runtime-selectable ImGui/RmlUi UI backends.
  - `./abuild.py -c -d <build-dir> -b jolt,physx` builds one binary with runtime-selectable Jolt/PhysX physics backends.

## Specialist Build Contract (Canonical)
- Specialists use assigned, pre-provisioned `build-a*` slots only.
- Specialists do not create new build dirs and do not spend slice context on local tooling bootstrap.
- If local `./vcpkg` is missing/unbootstrapped, stop and escalate to overseer/human.
- Default-first execution: use `./abuild.py -c -d <build-dir>` unless the slice specifically requires backend variability.
- If backend selection is needed, include only the category/categories under test for that slice.

## Canonical Usage (Current)
```bash
# show default backend configuration
./abuild.py -D
Build defaults: sdl3, bgfx, jolt, rmlui, sdl3audio

# set specialist identity for lock ownership
export ABUILD_AGENT_NAME=<specialist-name>

# claim assigned slot before first build in the session
./abuild.py --claim-lock -d <build-dir>

# configure/build in assigned slot with defaults for unspecified categories
./abuild.py -c -d <build-dir>

# configure/build in assigned slot with runtime-selectable renderer set
./abuild.py -c -d <build-dir> -b bgfx,diligent

# configure/build in assigned slot with runtime-selectable ui set
./abuild.py -c -d <build-dir> -b imgui,rmlui

# configure/build in assigned slot with runtime-selectable physics set
./abuild.py -c -d <build-dir> -b jolt,physx

# optional: inspect lock metadata
./abuild.py --lock-status -d <build-dir>

# release slot lock when retiring/transferring ownership
./abuild.py --release-lock -d <build-dir>
```

## Backend-Category Selection Rule
If an option from a backend category (`platform`, `renderer`, `physics`, `ui`, `audio`) is not specified, `abuild.py` uses the category default. If one is specified, that backend is selected for the category. If more than one from a category is specified, `abuild.py` builds that category as runtime-selectable in one binary.

## Validation
From `m-rewrite/`:

```bash
./docs/scripts/lint-project-docs.sh
```

## Handoff Checklist
- [x] Canonical usage text is synchronized in all active docs that mention `abuild.py`.
- [x] Outdated command examples are removed/updated.
- [x] Validation command run and results recorded.
- [x] `docs/projects/ASSIGNMENTS.md` row is current.

## Status/Handoff Notes
- `2026-02-17`: verified lock/agent/backend-selection behavior directly from `abuild.py` and updated canonical specialist command flow.
- `2026-02-17`: documented explicit blocker posture: specialists escalate missing local `./vcpkg` and do not spend coding-slice context on environment bootstrap.
- `2026-02-17`: synchronized remaining governance/project docs from legacy `abuild.py -d ...` forms to canonical `./abuild.py -c -d <build-dir>` default-first policy; docs lint passed.
