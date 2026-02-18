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
| `gameplay-migration.md` | `overseer` | `in progress (D1 hardening landed; netcode lane consolidated)` | Execute D2 movement replication slice: wire client `PlayerLocation` intent path to rewrite server authority for tank drive state. | `2026-02-18` |
| `physics-refactor.md` | `specialist-physics-refactor` | `in progress (Phase 4t validated in build-a3; physics.system runtime-command traces now emit deterministic stage+operation+outcome+failure-cause tags)` | `KARMA intake`: execute a bounded Phase 4 follow-up to deepen runtime-command observability coverage while preserving current API/behavior semantics. | `2026-02-18` |
| `karma-lighting-shadow-parity.md` | `unassigned` | `priority/on hold (close-out snapshot captured; awaiting external revisions)` | `KARMA intake`: after external revisions, rerun canonical BGFX/Diligent baseline, resolve/characterize shared seam artifact, then resume bounded P0-S3 re-intake. | `2026-02-17` |
| `radar.md` | `overseer` | `in progress (research baseline complete)` | `shared unblocker + KARMA intake`: execute R1 engine substrate slice for generic offscreen render-target + multi-camera pass scaffolding. | `2026-02-18` |
| `ui-game.md` | `overseer` | `in progress (staged import landed; no build/runtime wiring yet)` | `m-dev parity + shared unblocker`: execute G0 classification pass for `src/game/ui/*` and record file-level dependency blockers. | `2026-02-18` |
| `ui-engine.md` | `codex` | `in progress` | Execute one bounded console focus-release parity follow-up slice without backend leakage. | `2026-02-12` |
| `backend-refactor.md` | `overseer` | `in progress (project created; consistency-first standards locked in docs)` | `shared unblocker`: execute BR0 standards-freeze slice and produce exact rename/move + include-impact matrix before code movement. | `2026-02-18` |
| `repo-prep.md` | `overseer` | `in progress (planning; no code movement started)` | `shared unblocker`: execute RP0 decision-gate lock (vcpkg + KGDK->BZ3 artifact contract + sibling branch/worktree model + KGDK public-header boundary) before any filesystem migration. | `2026-02-18` |

## Active Specialist Roster

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
