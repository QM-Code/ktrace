# Project Assignments

Use this board for active delegation only.

Update rules:
- One row per active project file in `docs/projects/` (exclude `README.md`, `AGENTS.md`, `PROJECT_TEMPLATE.md`, `ASSIGNMENTS.md`).
- Update `Owner`, `Status`, `Next Task`, and `Last Update` in every handoff.
- Keep each `Next Task` as one concrete action.

Strategic tracking:
- Each accepted slice should be labeled as `m-dev parity`, `KARMA intake`, or `shared unblocker`.
- Keep work inside assigned isolated build dirs and explicit wrapper build-dir args.

| Project | Owner | Status | Next Task | Last Update |
|---|---|---|---|---|
| `content-mount.md` | `unassigned` | `queued (handoff-ready; shared unblocker slices landed)` | `shared unblocker`: harden delta-selection policy with trace-backed tuning and one bounded regression. | `2026-02-16` |
| `core-engine-infrastructure.md` | `overseer` | `in progress` | Keep implementation sequencing aligned to `docs/foundation/architecture/core-engine-contracts.md` as active tracks land. | `2026-02-12` |
| `gameplay-migration.md` | `overseer` | `in progress (D1 hardening landed)` | Execute D2 movement replication slice: wire client `PlayerLocation` intent path to rewrite server authority for tank drive state. | `2026-02-14` |
| `gameplay-netcode.md` | `unassigned` | `queued` | Prepare next predicted-shot reconciliation slice (`local_shot_id`) behind current P0 renderer priorities. | `2026-02-12` |
| `physics-backend.md` | `codex` | `in progress` | Decide and implement runtime lock mutation/query contract or explicitly lock constraints to creation-time only with parity assertions. | `2026-02-12` |
| `renderer-backend-file-split.md` | `unassigned` | `blocked (deferred to avoid overlap with active karma-lighting-shadow-parity backend ownership)` | `shared unblocker`: resume only after active renderer parity specialist retires from backend monolith hotspot paths, then execute Phase 0 scaffolding. | `2026-02-17` |
| `karma-lighting-shadow-parity.md` | `specialist-renderer-csm-p0s1` | `priority/in progress (P0-S1 directional CSM + P0-S2 compare-sampler intake complete)` | `KARMA intake`: execute `P0-S3` point-shadow GPU generation path in both backends while preserving bounded fallback during stabilization. | `2026-02-17` |
| `ui-integration.md` | `codex` | `in progress` | Execute one bounded console focus-release parity follow-up slice without backend leakage. | `2026-02-12` |
| `webserver-unit-tests.md` | `unassigned` | `paused` | Resume only when webserver handler internals change; next slice is users/user_profile/server_edit mutation-flow tests. | `2026-02-13` |

## Active Specialist Roster
- `specialist-renderer-csm-p0s1` -> `karma-lighting-shadow-parity.md` (`build-a5`)

## Build Policy Lock
- Use `./abuild.py -c -d <build-dir>` for configure/build/test workflows (`-d` required; omit `-c` only when intentionally reusing an already configured dir).
- Default-first: most slices should use `./abuild.py -c -d <build-dir>` with no `-b`.
- Do not use raw `cmake -S/-B` for delegated execution.
- Specialist packets must provide explicit agent identity and assigned `build-a*` slot(s).
- Specialists must claim/release assigned slots through `abuild.py` lock commands:
  - claim: `./abuild.py --claim-lock -d <build-dir>`
  - release: `./abuild.py --release-lock -d <build-dir>`
- Renderer dual-backend runtime-select builds use one command/list in a single binary:
  - `./abuild.py -c -d <build-dir> -b bgfx,diligent`
- UI dual-backend runtime-select builds use one command/list in a single binary:
  - `./abuild.py -c -d <build-dir> -b imgui,rmlui`
- Physics dual-backend runtime-select builds use one command/list in a single binary:
  - `./abuild.py -c -d <build-dir> -b jolt,physx`
- When `-b` is used, include only the backend category/categories directly touched by that slice.
- Local `./vcpkg` is mandatory; specialists treat missing/unbootstrapped setup as a blocker and escalate to overseer/human.
- Wrapper closeouts in parallel must pass explicit build dirs:
  - `./scripts/test-engine-backends.sh <build-dir>`
  - `./scripts/test-server-net.sh <build-dir>`
