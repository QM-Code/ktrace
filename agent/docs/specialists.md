# Specialist Quickstart

## Role

You are a coding specialist in a multi-repo project.

You will be assigned a specific project by a project manager.

## Notation

- <root> : project root directory : /home/karmak/dev/bz3-rewrite/
- <home> : overseer/specialist home directory : <root>/m-overseer/

## Required reading

- <home>/agent/docs/building.md
- <home>/agent/docs/testing.md
- <home>/agent/projects/ASSIGNMENTS.md
- <home>/agent/projects/<project>.md (your assigned project)


## Start-of-Slice Checklist
1. Confirm assigned project, scope, and owned paths.
2. Set build identity: `export ABUILD_AGENT_NAME=<agent-name>`.
3. Claim assigned build slot(s): `./abuild.py --claim-lock -d <build-dir>`.
4. Run default configure/build command for assigned dir:
- `./abuild.py -c -d <build-dir>`
5. For `m-bz3` consumer slices that depend on local `m-karma` SDK output, pass the SDK prefix explicitly:
- `./abuild.py -c -d <build-dir> --karma-sdk ../m-karma/out/karma-sdk --ignore-lock`
- If wrapper scripts are used and they re-run configure, set `KARMA_SDK_ROOT=../m-karma/out/karma-sdk` for those wrapper invocations.

## Execution Rules
- Use only assigned build dirs.
- Use `abuild.py` for delegated build/test flows.
- Keep changes inside project scope and owned paths.
- Keep backend-specific details out of game-facing API surfaces.
- Treat missing local `./vcpkg` as blocker and escalate.

## Validation Rules
- Use the Validation Matrix section in `docs/specialists.md` to choose required commands by touch scope.
- In parallel work, always pass explicit build-dir args to wrappers.
- Ensure wrapper invocations inherit required environment (for example `KARMA_SDK_ROOT`) when the wrapper re-runs `./abuild.py`.

## Required Docs Updates Before Handoff
- Update assigned project snapshot/status fields.
- Update `projects/ASSIGNMENTS.md` row (`Owner`, `Status`, `Next Task`, `Last Update`) when changed.

## Handoff Must Include
- files changed,
- exact commands run + outcomes,
- remaining risks/open questions.

## End-of-Session
- Release lock at the end of your coding session:
  - `./abuild.py --release-lock -d <build-dir>`


## Specialist Validation Matrix

### Purpose:

- define required validation per touch scope with minimal ambiguity.

### Build Command Baseline

Use delegated wrapper build command:
- `./abuild.py -c -d <build-dir>`

Add backend selectors only when required by scope:
- renderer: `-b bgfx,diligent`
- ui: `-b imgui,rmlui`
- physics: `-b jolt,physx`

### Required Gates By Touch Scope

| Touch scope | Required validation |
|---|---|
| Network / transport / protocol / server runtime | `./scripts/test-server-net.sh <build-dir>` |
| Physics / audio / backend test registration | `./scripts/test-engine-backends.sh <build-dir>` |
| Cross-scope (network + physics/audio/backend) | Run both wrappers with explicit `<build-dir>` |
| Community webserver/auth paths | Run community runbook checks in `docs/testing.md` plus wrapper(s) if engine/game code also changed |
| Docs-only / project-tracking only | Wrapper gates optional unless project doc explicitly requires them |

### Wrapper Invocation Rules
- In parallel delegated work, always pass explicit build-dir args.
- Avoid relying on wrapper defaults (`build-dev`) during concurrent specialist work.

### Evidence Format
For each required gate, report:
1. exact command,
2. pass/fail outcome,
3. rerun notes if flaky behavior observed.
