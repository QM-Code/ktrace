# Project Assignments (m-overseer)

Use this board for active overseer-only delegation and orchestration tracking.

Update rules:
- One row per active project file in `projects/` (exclude `AGENTS.md` and `ASSIGNMENTS.md`).
- Update `Owner`, `Status`, `Next Task`, and `Last Update` in every handoff.
- Keep each `Next Task` as one concrete action.

| Project | Owner | Status | Next Task | Last Update |
|---|---|---|---|---|
| `diligent.md` | `overseer` | `not started` | Draft specialist packet for package-managed Diligent integration to make KARMA SDK client export relocatable. | `2026-02-19` |
| `gameplay.md` | `overseer` | `in progress (merged baseline: gameplay-migration + physics-refactor overlap normalized; Phase 5 physics foundation complete, behavior-parity migration remains)` | `m-dev parity + q-karma intake`: execute G0 re-baseline packet for D2 movement replication seams (`PlayerLocation` transport + authoritative server actor updates). | `2026-02-19` |
| `lighting.md` | `unassigned` | `priority/on hold (close-out snapshot captured; awaiting external revisions)` | `q-karma intake`: after external revisions, rerun canonical BGFX/Diligent baseline, resolve/characterize shared seam artifact, then resume bounded P0-S3 re-intake. | `2026-02-17` |
| `radar.md` | `overseer` | `in progress (research baseline complete)` | `shared unblocker + q-karma intake`: execute R1 engine substrate slice for generic offscreen render-target + multi-camera pass scaffolding. | `2026-02-18` |
| `ui-engine.md` | `codex` | `in progress` | Execute one bounded console focus-release parity follow-up slice without backend leakage. | `2026-02-12` |
| `ui-game.md` | `overseer` | `in progress (staged import landed; no build/runtime wiring yet)` | `m-dev parity + shared unblocker`: execute G0 classification pass for `src/game/ui/*` and record file-level dependency blockers. | `2026-02-18` |
| `headers.md` | `codex` | `complete (H0-H6 delivered: explicit SDK allowlist install + drift guard + compatibility-reviewed pruning + end-to-end validation passing)` | Maintenance only: when `include/karma/*` changes, update `cmake/KarmaSdkHeaders.cmake` classification and rerun SDK/BZ3 validation gates. | `2026-02-19` |
| `lone-cmake.md` | `overseer` | `not started` | Run M0 baseline map of `m-bz3/src/game/CMakeLists.txt` target ownership and no-op relocation plan. | `2026-02-19` |
