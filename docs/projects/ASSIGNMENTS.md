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
| `content-mount.md` | `codex` | `in progress` | Add stronger chunk-transfer integrity controls and one targeted regression test. | `2026-02-12` |
| `core-engine-infrastructure.md` | `overseer` | `in progress` | Keep implementation sequencing aligned to `docs/foundation/architecture/core-engine-contracts.md` as active tracks land. | `2026-02-12` |
| `gameplay-netcode.md` | `unassigned` | `queued` | Prepare next predicted-shot reconciliation slice (`local_shot_id`) behind current P0 renderer priorities. | `2026-02-12` |
| `physics-backend.md` | `codex` | `in progress` | Decide and implement runtime lock mutation/query contract or explicitly lock constraints to creation-time only with parity assertions. | `2026-02-12` |
| `renderer-parity.md` | `specialist-renderer-parity` | `priority/in progress` | Keep parity ledger synced with accepted `renderer-shadow-hardening` outcomes and leave VQ4 queued until VQ3 acceptance. | `2026-02-12` |
| `renderer-shadow-hardening.md` | `codex` | `priority/in progress` | Capture deterministic baseline evidence (sandbox + bz3), then execute alignment/blockiness hardening slices with parity gates. | `2026-02-12` |
| `ui-integration.md` | `codex` | `in progress` | Execute one bounded console focus-release parity follow-up slice without backend leakage. | `2026-02-12` |
| `webserver-unit-tests.md` | `unassigned` | `paused` | Resume only when webserver handler internals change; next slice is users/user_profile/server_edit mutation-flow tests. | `2026-02-13` |

## Active Specialist Roster
- `specialist-renderer-parity` -> `docs/projects/renderer-parity.md`
  - Build dirs:
    - `build-sdl3-bgfx-physx-imgui-sdl3audio`
    - `build-sdl3-diligent-physx-imgui-sdl3audio`
- `open-specialist-slot` -> `unassigned`

## Build Policy Lock
- Use `./bzbuild.py <build-dir>` for configure/build/test workflows.
- Do not use raw `cmake -S/-B` for delegated execution.
- Wrapper closeouts in parallel must pass explicit build dirs:
  - `./scripts/test-engine-backends.sh <build-dir>`
  - `./scripts/test-server-net.sh <build-dir>`
