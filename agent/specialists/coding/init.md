# Specialist Quickstart

## Role

You are a coding specialist in a multi-repo project.

## Identifiers

You will be assigned to a specific project by a project manager and provided the following identifiers:

- <agent> : your unique identification
- <project> : the project you will be working on
- <build-dir> : one or more build directories that you will be working with

## Notation

- <root> : project root directory : /home/karmak/dev/bz3-rewrite/
- <home> : overseer/specialist home directory : <root>/m-overseer/agent

## Required Reading

- <home>/docs/coding.md
- <home>/docs/building.md
- <home>/docs/testing.md
- <home>/projects/ASSIGNMENTS.md
- <home>/projects/<project>.md (your assigned project)

## Execution Rules
- Use only assigned build dirs.
- Use `abuild.py` for delegated build/test flows.
- Keep changes inside project scope and owned paths.
- Keep backend-specific details out of game-facing API surfaces.

## Validation Rules
- Use the Validation Matrix shown below to choose required commands by touch scope.
- In parallel work, always pass explicit build-dir args to wrappers.

## Required Docs Updates Before Handoff
- Update assigned project snapshot/status fields.
- Update `projects/ASSIGNMENTS.md` row (`Owner`, `Status`, `Next Task`, `Last Update`) when changed.

## Handoff Must Include
- files changed,
- exact commands run + outcomes,
- remaining risks/open questions.

## End-of-Session
- Release lock at the end of your coding session:
  - `./abuild.py --agent <agent> --directory <build-dir> --release-lock`

## Specialist Validation Matrix

### Purpose:

- define required validation per touch scope with minimal ambiguity.

### Required Gates By Touch Scope

| Touch scope | Required validation |
|---|---|
| Network / transport / protocol / server runtime | `./scripts/test-server-net.sh <build-dir>` |
| Physics / audio / backend test registration | `./scripts/test-engine-backends.sh <build-dir>` |
| SDK packaging/runtime (Linux) | `./scripts/test-sdk-runtime-linux.sh <sdk-prefix> [consumer-bin ...]` |
| SDK packaging/runtime (macOS) | `./scripts/test-sdk-runtime-macos.sh <sdk-prefix> [consumer-bin ...]` |
| SDK packaging/runtime (Windows) | `./scripts/test-sdk-runtime-windows.sh <sdk-prefix> [consumer-bin ...]` |
| SDK packaging/policy (mobile static contract) | `./scripts/test-sdk-mobile-static.sh <sdk-prefix>` |
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
