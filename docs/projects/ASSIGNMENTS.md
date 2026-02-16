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
| `karma-shadow-intake-p0.md` | `overseer` | `priority/in progress (KS0 + KS1 + KS2.1 directional GPU shadow hardening + KS2.2 local-light source contract/shading bridge landed)` | `KARMA intake`: continue `KS2` by implementing bounded point-shadow generation/sampling for selected local lights in BGFX + Diligent (with existing trace/fallback contracts). | `2026-02-16` |
| `physics-backend.md` | `codex` | `in progress` | Decide and implement runtime lock mutation/query contract or explicitly lock constraints to creation-time only with parity assertions. | `2026-02-12` |
| `renderer-parity.md` | `specialist-renderer-parity` | `priority/in progress (R26-A complete; R26-B slice 1 landed; R26-B slice 2 scaffolding landed; R26-B slice 3 BGFX GPU prototype landed; R26-B slice 4 Diligent GPU prototype landed; R26-B slice 5 lifecycle/fallback hardening landed; R26-B visual closeout checkpoint captured with Diligent screenshot tooling blocker documented; R26-B gpu_default no-shadow fix landed and operator-verified; R26-B depth-attachment GPU shadow slice landed; R26-C intake matrix landed; R26-D config policy slice landed; R26-D bias-model policy slice landed; R26-D bounded bias sweep/default-lock evidence landed)` | Execute operator visual closeout for locked bias defaults (`gpu_default`) and open one minimal follow-up adjustment only if contact-edge artifacts persist. | `2026-02-16` |
| `renderer-shadow-hardening.md` | `codex` | `priority/in progress (gpu_default parity path and bias controls landed; visual closeout pending)` | Run operator visual closeout for locked `gpu_default` defaults, then open one bounded contact-edge/aliasing follow-up only if needed. | `2026-02-16` |
| `ui-integration.md` | `codex` | `in progress` | Execute one bounded console focus-release parity follow-up slice without backend leakage. | `2026-02-12` |
| `webserver-unit-tests.md` | `unassigned` | `paused` | Resume only when webserver handler internals change; next slice is users/user_profile/server_edit mutation-flow tests. | `2026-02-13` |

## Active Specialist Roster
- `specialist-renderer-parity` -> `docs/projects/renderer-parity.md`
  - Build dirs:
    - `build-sdl3-bgfx-physx-imgui-sdl3audio`
    - `build-sdl3-diligent-physx-imgui-sdl3audio`

## Build Policy Lock
- Use `./bzbuild.py <build-dir>` for configure/build/test workflows.
- Do not use raw `cmake -S/-B` for delegated execution.
- Wrapper closeouts in parallel must pass explicit build dirs:
  - `./scripts/test-engine-backends.sh <build-dir>`
  - `./scripts/test-server-net.sh <build-dir>`
