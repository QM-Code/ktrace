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
| `engine-game-boundary-hygiene.md` | `specialist-engine-boundary-e1` | `in progress (E0.6/E1 landed on build-a1; compatibility wrappers + shared primitives extraction validated)` | `shared unblocker`: execute E2 manifest/cache extraction into `common/content` and keep game wrappers behavior-identical. | `2026-02-17` |
| `gameplay-migration.md` | `overseer` | `in progress (D1 hardening landed)` | Execute D2 movement replication slice: wire client `PlayerLocation` intent path to rewrite server authority for tank drive state. | `2026-02-14` |
| `gameplay-netcode.md` | `unassigned` | `queued` | Prepare next predicted-shot reconciliation slice (`local_shot_id`) behind current P0 renderer priorities. | `2026-02-12` |
| `physics-refactor.md` | `codex` | `in progress (new KARMA-alignment foundation track)` | `KARMA intake`: execute Phase 0/1 contract reset and scaffold KARMA-style world/controller/collider API layers. | `2026-02-17` |
| `renderer-backend-file-split.md` | `specialist-renderer-file-split` | `in progress (Phase 3 pipeline extraction landed and validated on build-a6)` | `shared unblocker`: execute Phase 4 move-only shadow extraction by splitting shadow resource/cache/update/pass blocks into `shadow.cpp` for BGFX and Diligent. | `2026-02-17` |
| `karma-lighting-shadow-parity.md` | `unassigned` | `priority/on hold (close-out snapshot captured; awaiting external revisions)` | `KARMA intake`: after external revisions, rerun canonical BGFX/Diligent baseline, resolve/characterize shared seam artifact, then resume bounded P0-S3 re-intake. | `2026-02-17` |
| `ui-integration.md` | `codex` | `in progress` | Execute one bounded console focus-release parity follow-up slice without backend leakage. | `2026-02-12` |
| `webserver-unit-tests.md` | `unassigned` | `paused` | Resume only when webserver handler internals change; next slice is users/user_profile/server_edit mutation-flow tests. | `2026-02-13` |

## Active Specialist Roster
- `specialist-renderer-file-split` -> `build-a6` (`renderer-backend-file-split.md`)

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
