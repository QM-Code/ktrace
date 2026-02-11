# Testing + CI + Docs Governance

## Project Snapshot
- Current owner: `codex`
- Status: `in progress (docs-governance sync slice 1 completed 2026-02-11)`
- Immediate next task: keep this track validation matrix synchronized with future accepted guard/wrapper changes (next expected sync points: renderer VQ3/VQ4 closeout + any new server/net or platform seam guard integrations).
- Validation gate: docs-only slices run `./docs/scripts/lint-project-docs.sh`; script/CI slices must run the track-specific flows in `Validation Source of Truth`.

## Mission
Own quality gates, test wrappers, CI workflows, and cross-track documentation consistency.

## Primary Specs
- `docs/projects/server-network.md`
- `docs/projects/engine-backend-testing.md`
- `docs/projects/core-engine-infrastructure.md` (references + delegation readiness)

## Why This Is Separate
This project is support infrastructure and can run in parallel with implementation projects.

## Owned Paths
- `m-rewrite/scripts/test-*.sh`
- `m-rewrite/.github/workflows/*`
- `docs/projects/server-network.md`
- `docs/projects/engine-backend-testing.md`
- `docs/projects/*`

## Interface Boundaries
- Inputs: new tests or run requirements from implementation projects.
- Outputs: enforced and documented validation procedures.
- Coordinate before changing:
  - `AGENTS.md`
  - `docs/projects/core-engine-infrastructure.md`

## Non-Goals
- Implementing subsystem behavior changes unless explicitly assigned.

## Validation Source of Truth
All commands below are run from `m-rewrite/`.

### Renderer Track (`docs/projects/renderer-parity.md`)
| Context | Required local/CI flow | Guard/wrapper posture |
|---|---|---|
| Local delegated closeout | `./bzbuild.py -c build-sdl3-bgfx-physx-imgui-sdl3audio` then `./bzbuild.py -c build-sdl3-diligent-physx-imgui-sdl3audio` then `./scripts/run-renderer-vq2-evidence.sh` | `run-renderer-vq2-evidence.sh` is a standalone evidence runner (logs + timeout exit code + child-process cleanup reporting); not integrated into other wrappers. |
| CI baseline (current) | `.github/workflows/core-test-suite.yml` wrapper gates (`./scripts/test-engine-backends.sh`, `./scripts/test-server-net.sh`) | No dedicated renderer VQ evidence CI wrapper yet; renderer evidence remains an explicit specialist closeout artifact. |

### Server/Net Track (`docs/projects/engine-network-foundation.md`, `docs/projects/server-network.md`)
| Context | Required local/CI flow | Guard/wrapper posture |
|---|---|---|
| Local delegated closeout | `./bzbuild.py -c <assigned-server-net-build-dir>` then `./scripts/test-server-net.sh <assigned-server-net-build-dir>` | `./scripts/test-server-net.sh` runs `./scripts/check-network-backend-encapsulation.sh` as a pre-check before configure/build/test. |
| Optional standalone guard invocation | `./scripts/check-network-backend-encapsulation.sh` | Supported for fast isolation/debug; functionally redundant if `test-server-net.sh` already ran in the same tree state. |
| CI baseline (current) | `.github/workflows/core-test-suite.yml` runs `./scripts/test-server-net.sh` (default `build-dev`) | Guard pre-check is enforced in CI via wrapper integration. |

### Platform Seam Track (`docs/projects/platform-backend-policy.md`)
| Context | Required local/CI flow | Guard/wrapper posture |
|---|---|---|
| Local delegated closeout | `./scripts/check-platform-seam.sh` then `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-sdl3audio` | `check-platform-seam.sh` is currently standalone (not integrated into `test-engine-backends.sh`). |
| CI baseline (current) | `.github/workflows/core-test-suite.yml` wrapper gates still run (`./scripts/test-engine-backends.sh`, `./scripts/test-server-net.sh`) | No dedicated platform seam guard CI step yet; seam guard remains mandatory in local slice acceptance until workflow integration is explicitly added. |

## Guard Command Policy
- `./scripts/check-network-backend-encapsulation.sh` is both:
  - a standalone guard command, and
  - a wrapper-integrated pre-check inside `./scripts/test-server-net.sh`.
- `./scripts/check-platform-seam.sh` is standalone-only today; run it explicitly for platform seam slices.
- `./scripts/run-renderer-vq2-evidence.sh` is standalone-only today; run it explicitly for renderer visual-quality evidence slices.

## Trace Channels
- Not subsystem-specific; this project validates process and docs.

## Build Isolation and `bzbuild.py` Policy (Required)
- Operator-facing configure/build/test flows must use `./bzbuild.py <build-dir>` only.
- Do not run raw `cmake -S/-B` directly for delegated slice execution.
- Use isolated build dirs for delegated parallel work, and pass explicit build-dir arguments to wrappers:
  - `./scripts/test-engine-backends.sh <build-dir>`
  - `./scripts/test-server-net.sh <build-dir>`
- `build-dev` default wrapper runs remain valid for serialized local checks and CI baseline behavior.
- Wrapper internals currently invoke `cmake`/`ctest`; this does not change the operator policy above.

## First Session Checklist
1. Confirm all CTest targets map to one required flow in `Validation Source of Truth`.
2. Confirm guard command posture is accurate (standalone-only vs wrapper-integrated).
3. Confirm CI wrapper usage still matches documented baseline behavior.
4. Confirm isolated build-dir and `bzbuild.py` policy wording remains unchanged.

## Drift Checklist (Future Accepted Slices)
- If a guard script gains wrapper integration, update both the track matrix and `Guard Command Policy`.
- If CI starts or stops invoking a guard/wrapper directly, update CI baseline rows the same day.
- If a wrapper default/build-dir contract changes, update `Build Isolation and bzbuild.py Policy` wording immediately.
- If a track adds a required evidence runner, add it under `Validation Source of Truth` before accepting the slice.

## Current Status
- `2026-02-11`: Completed this docs-governance sync slice.
- Source-of-truth validation flows now explicitly cover renderer evidence, server/net wrapper + guard integration, and platform seam standalone guard usage.
- Guard command posture is now explicit and drift-checkable (standalone vs wrapper-integrated).
- Build isolation and `bzbuild.py` policy wording is now centralized in this file.

## Open Questions
- Should `check-platform-seam.sh` be integrated into an existing wrapper and CI path, or kept as explicit standalone enforcement?
- Should renderer VQ evidence capture eventually receive a dedicated CI workflow step, or remain specialist closeout evidence only?

## Handoff Checklist
- [ ] New tests documented.
- [ ] Wrapper scripts updated if needed.
- [ ] CI workflow updated if needed.
- [ ] `AGENTS.md`, `docs/AGENTS.md`, and `docs/projects/*.md` aligned.
- [x] Standalone guard commands vs wrapper-integrated guards documented and current.
- [x] Isolated build-dir + `bzbuild.py` policy wording documented and current.
