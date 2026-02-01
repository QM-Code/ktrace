# TODO (Owner’s Wishlist)

This replaces the old list. We are early-stage and can break APIs freely.
Focus is on long-term clarity, engine/game separation, and developer velocity.

## 1) Engine/Game Split (Highest Priority)
- Make `src/engine` fully standalone (no game headers, no game data dependencies).
- Split build into two phases: build Karma as libs → build BZ3 against them.
- Install/export public headers when Karma is split into its own repo.
- Create a Karma build script (parallel to `bzbuild.py`) once it is a separate repo.

## 2) Rendering & Scene Architecture
- Create a real render graph / pass scheduler (main scene, radar, UI overlay, post).
- Formalize a “scene” API: camera, layers, lights, renderables, skybox.
- Decouple game renderer from backend implementation details.
- Add a minimal debug draw layer (lines, boxes, text) for engine/game diagnostics.

## 3) Asset & World Pipeline
- Implement a proper asset registry (IDs, refcounts, hot reload hints).
- Add a world/content abstraction that supports FS + zipped + remote sources.
- Add a “world package” manifest format for server-delivered content.

## 4) Input System (Usability + Consistency)
- Add input **contexts** (gameplay vs UI vs roaming vs console).
- Make bindings fully data-driven, including context precedence.
- Add explicit controller support (dead zones, axis bindings, mappings).

## 5) UI System (Parity + Tooling)
- Keep ImGui/RmlUi feature parity as a hard rule. (done: validator + smoke harness in place)
- Add a single shared UI state validator to catch frontend divergence. (done: `ui.Validate`)
- Create a minimal UI smoke harness for regression checks. (done: `--ui-smoke`)
- VSync toggle: make runtime toggle actually work (engine-level work needed):
  - SDL window backend `setVsync()` is currently a no-op.
  - BGFX backend forces `BGFX_RESET_VSYNC` on init + resize.
  - Diligent backend presents with default interval (no toggle).
  - Wire `graphics.VSync` into these backends so UI toggle applies immediately.

## 6) Networking & Protocol
- Add explicit protocol versioning and compatibility handling.
- Introduce a client/server capability handshake for optional features.
- Create a replay/record format for debugging net sync.

## 7) Physics & Simulation
- Define a clean physics API contract (including query/raycast utilities).
- Add deterministic or lockstep options where feasible.
- Standardize player controller semantics across backends.

## 8) Dev/Editor Mode
- Integrate the roaming/dev camera as a proper “spectator” mode with no body.
- Add a minimal “world explorer” overlay for inspecting entities, materials,
  and network state.

## 9) Configuration & Data
- Formalize config schema docs (engine defaults vs game defaults vs user).
- Add a config validation pass on startup (strict mode).
- Add runtime config reload hooks where safe.

## 10) Tooling & Testing
- Add a lightweight test harness for:
  - input mapping
  - config parsing
  - protocol encode/decode
  - UI model/controller logic
- Add CI builds for bgfx+imgui and bgfx+rmlui at minimum.

## 11) Documentation
- Keep cascading docs up-to-date as code moves. (done: noted in `AGENTS.md`)
- Add a “How to add a new subsystem” guide. (done: `HOW-TO-ADD-SUBSYSTEM.md`)

## 12) Camera view toggle (first-person vs third-person)
- Add a settings toggle for camera mode (first-person vs third-person), plus config key.
- Minimal implementation: fixed offset behind/above the tank (no collision) — ~1–2 hours.
- Polished implementation: collision/occlusion raycast + smoothing + optional shoulder swap — ~1–2 days.
- Likely touch points:
  - Camera positioning logic in `src/game/renderer/`
  - Settings UI (ImGui + RmlUi) in `src/game/ui/frontends/*/console/panels/panel_settings.*`
  - Config defaults in `data/client/config.json`
  - UI config accessors in `src/game/ui/config/`

---

Notes:
- We can break APIs freely during this stage—prefer clean design over compatibility.
- When unsure about scope, prefer engine-level generalization and push game-specific
  behavior into `src/game`.
