# Project Assignments (m-overseer)

Use this board for active overseer-only delegation and orchestration tracking.

Update rules:
- One row per active project file in `projects/` (exclude `AGENTS.md` and `ASSIGNMENTS.md`).
- Update `Owner`, `Status`, `Next Task`, and `Last Update` in every handoff.
- Keep each `Next Task` as one concrete action.

| Project | Owner | Status | Next Task | Last Update |
|---|---|---|---|---|
| `lighting.md` | `unassigned` | `priority/on hold (close-out snapshot captured; awaiting external revisions)` | `q-karma intake`: after external revisions, rerun canonical BGFX/Diligent baseline, resolve/characterize shared seam artifact, then resume bounded P0-S3 re-intake. | `2026-02-17` |
| `radar.md` | `overseer` | `in progress (research baseline complete)` | `shared unblocker + q-karma intake`: execute R1 engine substrate slice for generic offscreen render-target + multi-camera pass scaffolding. | `2026-02-18` |
| `ui-engine.md` | `codex` | `in progress` | Execute one bounded console focus-release parity follow-up slice without backend leakage. | `2026-02-12` |
| `ui-game.md` | `overseer` | `in progress (staged import landed; no build/runtime wiring yet)` | `m-dev parity + shared unblocker`: execute G0 classification pass for `src/game/ui/*` and record file-level dependency blockers. | `2026-02-18` |
| `gameplay.md` | `overseer` | `in progress (new localhost playable-loop bring-up track)` | Dispatch `GP-S1` specialist packet (join/play defaults + remove non-parity actor tick drift). | `2026-02-21` |
| `cleanup.md` | `overseer` | `in progress (analysis baseline complete; ui-deferred path accepted)` | Execute `CLN-S2`: remove non-parity server actor tick drift/health decay and add session->actor indexing to eliminate repeated entity scans. | `2026-02-21` |
| `fallbacks.md` | `overseer` | `in progress (stb/enet/bgfx cleanup landed; full wrapper suites pending)` | Run `test-engine-backends.sh` (`m-karma`) and `test-server-net.sh` (`m-bz3`), then decide whether to remove remaining safe fallback blocks (`glm`, `assimp`, `spdlog`, `miniz`, `nlohmann_json`). | `2026-02-21` |
