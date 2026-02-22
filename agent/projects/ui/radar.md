# UI Radar System (Engine Substrate + Game Behavior)

## Project Snapshot
- Current owner: `overseer`
- Status: `in progress (research baseline complete; implementation not started)`
- Immediate next task: execute `R1` engine substrate slice: add generic offscreen render-target contract + multi-camera pass scheduling scaffold in `m-karma` renderer interfaces.
- Validation gate: docs updates: `cd m-overseer && ./agent/scripts/lint-projects.sh`; implementation slices: `cd m-karma && ./abuild.py -c -d <build-dir> -b bgfx,diligent,imgui,rmlui` with runtime smoke in `m-bz3`.

## Mission
Deliver a radar system where:
- engine provides generic technology for camera-to-texture picture-in-picture rendering,
- game defines the radar concept, tracked gameplay entities, orientation policy, and visual semantics,
- implementation preserves `m-karma`/`m-bz3` ownership boundaries while reusing proven behavior/flow from `q-karma` and `m-dev`.

## Foundation References
- `projects/ui.md`
- `projects/ui/karma.md`
- `projects/ui/bz3.md`
- `projects/gameplay.md`
- `projects/lighting.md`
- `../docs/building.md`
- `../docs/testing.md`

## Why This Is Separate
Radar is a cross-cutting engine/game/UI feature with high boundary risk:
- engine-side renderer and UI substrate work is required before game radar behavior can land,
- game-side semantics (shots/players/world interpretation) must stay out of engine contracts,
- parity scope spans both `m-dev` behavior and `q-karma` architecture, so explicit intake policy is required.

## External Research Findings

### `q-karma` (engine-first substrate, demo radar)
- `examples/main.cpp` demonstrates working radar as orthographic offscreen camera rendered into a dedicated render target, then displayed in UI.
- `include/karma/components/camera.h` exposes camera-level offscreen controls (`render_to_texture`, `render_target`, `render_target_key`) and optional shader override fields.
- `src/renderer/render_system.cpp` already schedules offscreen camera passes and keeps key-based render-target lifecycle in renderer system ownership.
- `src/renderer/backends/diligent/backend_mesh.cpp` and `backend_render.cpp` implement generic render-target creation and texture-handle export (`getRenderTargetTextureId`).
- `src/renderer/backends/diligent/backend_ui.cpp` consumes render-target texture handles in UI draw commands.
- `examples/assets/shaders/radar_override_*.hlsl` shows camera-override symbolic coloring (height ramp) as generic engine capability, not hardcoded radar logic.

### `m-dev` (game-owned radar semantics and richer behavior)
- `src/game/renderer/radar_renderer.cpp` and `.hpp` implement game-owned radar pipeline (render target, radar camera orientation policy, FOV beams, and render orchestration).
- `src/game/renderer/renderer.cpp` maps ECS gameplay tags to radar render objects (`RadarRenderable`, `RadarCircle`) and maintains radar-only entity sync.
- `src/game/client/world_session.cpp` marks world mesh as radar-renderable.
- `src/game/client/player.cpp`, `client.cpp`, `shot.cpp` attach `RadarCircle` behavior to players/shots and toggle visibility with gameplay state.
- `data/common/shaders/radar.vert` and `radar.frag` implement height-relative symbolic radar coloring (player height anchor, above/below color policy).
- `src/game/ui/frontends/imgui/hud/radar.cpp` and `src/game/ui/frontends/rmlui/hud/radar.cpp` show backend-specific HUD composition from radar texture.
- `data/client/ui/hud.rml` and `hud.rcss` define radar HUD panel; `transform: scale(1, -1)` normalizes texture orientation for display.

### `m-karma` + `m-bz3` current baseline
- Radar data assets already exist in `m-bz3`:
  - shaders: `data/client/shaders/radar.vert`, `data/client/shaders/radar.frag`
  - HUD panel markup/styles: `data/client/ui/hud.rml`, `data/client/ui/hud.rcss`
  - config knobs: `data/client/config.json` (`gui.radar`, `ui.hud.radar`, `assets.shaders.radar`)
- Engine renderer contracts currently do not expose full offscreen render-target lifecycle or camera-to-target pass APIs in `m-karma`:
  - `m-karma/include/karma/renderer/device.hpp`
  - `m-karma/include/karma/renderer/backend.hpp`
  - `m-karma/src/renderer/render_system.cpp`
- Scene components currently lack a fully settled engine-owned camera component contract:
  - `m-karma/include/karma/scene/components.hpp`
- Current UI path in `m-karma` still has external-texture parity gaps:
  - `m-karma/src/ui/backends/rmlui/cpu_renderer.cpp`
- Result: `m-bz3` has radar assets, but `m-karma` still needs substrate closure to reliably route radar textures into both HUD backends.

## Direction Lock (Non-Negotiable)
1. Radar remains game-owned as a concept and behavior (`what radar means`, `what appears`, `how it behaves`).
2. Engine owns only reusable technology (`offscreen camera passes`, `render-target lifecycle`, `UI texture plumbing`).
3. Intake preference:
   - adopt `q-karma` architecture/flow for generic substrate,
   - adopt `m-dev` gameplay behavior where it is radar-specific parity.
4. No backend API/type leakage into `src/game/*`.
5. No engine-level `Radar*` gameplay concepts in `m-karma/src/*`.

## End Goal (Meet-In-The-Middle)
Radar is exposed to `m-bz3` gameplay/UI code as a normal HUD/console item, while all reusable rendering technology (offscreen pass scheduling, render-target lifecycle, and UI texture plumbing) remains engine-owned in `m-karma` and shared with non-radar overlays.

## Owned Paths
- `m-overseer/agent/projects/ui/radar.md`
- `m-overseer/agent/projects/ASSIGNMENTS.md`
- `m-karma/include/karma/renderer/*`
- `m-karma/src/renderer/*`
- `m-karma/include/karma/scene/components.hpp` and related scene/camera ownership surfaces
- `m-karma/include/karma/ui/*`
- `m-karma/src/ui/*`
- `m-bz3/src/game/client/*` (new radar runtime + integration slices)
- `m-bz3/data/client/shaders/radar.*`
- `m-bz3/data/client/ui/hud.*`
- `m-bz3/data/client/config*.json` (radar config wiring only)

## Interface Boundaries
- Inputs consumed:
  - `q-karma` generic offscreen camera/render-target/UI handle flow
  - `m-dev` game radar behavior (orientation policy, tracked entity classes, shot/player markers)
- Outputs exposed:
  - engine-generic offscreen pass and UI external-texture contracts
  - game-owned radar runtime and HUD bindings
- Coordinate before changing:
  - `m-karma/CMakeLists.txt`
  - `m-bz3/CMakeLists.txt`
  - renderer backend contract files in `m-karma/src/renderer/backends/*`
  - UI backend contract files in `m-karma/src/ui/backends/*`
  - `projects/ui.md`
  - `projects/ui/karma.md`
  - `projects/ui/bz3.md`

## Comparative Decision Matrix
| Concern | `q-karma` approach | `m-dev` approach | Rewrite decision |
|---|---|---|---|
| Offscreen camera substrate | Engine camera component + render target key routing | Game-local renderer orchestration | Use q-karma-style engine substrate. |
| Radar semantics ownership | Demo-specific in app example | Fully game-owned radar module | Keep radar semantics game-owned. |
| Shader customization | Camera override shader path + user params | Dedicated radar material shaders | Support both generic override path and game-owned radar material policy. |
| Radar entity selection | Implicit scene render to target | Explicit game ECS tags (`RadarRenderable`, `RadarCircle`) | Keep explicit game-owned inclusion tags. |
| Orientation behavior | Top-down follow camera in demo | Player-forward-up policy (world rotates in radar) | Preserve m-dev behavior as game policy. |
| UI presentation | ImGui draws target texture | ImGui + RmlUi HUD integration | Require both UI backends in rewrite validation. |

## Target Architecture in `m-karma` + `m-bz3`

### Engine Substrate (generic, no radar semantics)
1. Render-target lifecycle API in renderer contracts:
   - create/destroy target,
   - query UI-bindable texture handle,
   - backend parity for BGFX and Diligent.
2. Multi-camera pass scheduling:
   - primary camera pass to swapchain,
   - optional offscreen camera passes to named/explicit targets,
   - deterministic ordering and cleanup.
3. Camera pass data model:
   - perspective/orthographic controls,
   - primary/offscreen role flags,
   - optional shader override + user params.
4. UI external-texture path:
   - backend-neutral handle representation for UI draw code,
   - ImGui and RmlUi support for drawing engine render-target textures.

### Game Radar Runtime (game-owned behavior)
1. Game radar module under `src/game/client/*` (new radar domain paths).
2. Game-owned radar tags/components for tracked classes:
   - world/static environment presence,
   - player blips,
   - shot blips.
3. Orientation and camera policy:
   - player-forward-up behavior (world rotates relative to radar),
   - top-down orthographic capture centered on tracked player.
4. Symbolization policy:
   - baseline from existing radar shaders/config,
   - game-owned control over FOV wedges, marker radii, and filtering.
5. HUD integration:
   - wire radar texture to both ImGui and RmlUi HUD implementations,
   - honor `ui.hud.radar` visibility policy.

## Execution Plan

### R0: Research Lock (completed in this document)
- Consolidate source behavior from `q-karma` and `m-dev`.
- Record rewrite gap map and direction lock.
- Acceptance:
  - this document accepted as authoritative radar integration plan.

### R1: Engine Render-Target Contract Slice (`q-karma intake`)
- Add renderer contract APIs for offscreen target lifecycle and texture-handle export.
- Implement BGFX + Diligent backend parity for these APIs.
- Acceptance:
  - both backends can allocate/destroy offscreen targets and return valid texture handles.

### R2: Engine Multi-Camera Offscreen Pass Slice (`q-karma intake`)
- Introduce rewrite-owned camera pass model and render-system scheduling for offscreen + primary passes.
- Support named target-key reuse/cleanup behavior.
- Acceptance:
  - deterministic offscreen pass render updates are visible through target texture handles.

### R3: UI External-Texture Bridge Slice (`shared unblocker`)
- Add UI substrate for rendering external renderer textures in both ImGui and RmlUi paths.
- Remove current blocker where RmlUi external textures are unimplemented.
- Acceptance:
  - both UI backends can draw a renderer-produced texture in a HUD element.

### R4: Game Radar Domain Slice (`m-dev parity`)
- Introduce game-owned radar system/components and player/world/shot tagging flow.
- Preserve game-owned semantics and avoid engine gameplay leakage.
- Acceptance:
  - radar captures world + player/shot symbols from game-owned data.

### R5: HUD Radar Integration Slice (`m-dev parity`)
- Wire radar texture into rewrite HUD flows (ImGui + RmlUi) with visibility toggles.
- Ensure panel orientation and sizing parity behavior.
- Acceptance:
  - radar appears in both HUD backends with consistent orientation and toggle behavior.

### R6: Behavior Hardening + Closeout (`shared unblocker`)
- Validate advanced behavior: world-rotation policy, FOV overlay, shot marker updates, and runtime stability.
- Add bounded tests and trace diagnostics for maintenance.
- Acceptance:
  - runtime behavior is stable and docs/tests cover critical radar flows.

## Non-Goals
- Do not move radar gameplay meaning into engine-core contracts.
- Do not port `m-dev` file layout or renderer structure directly.
- Do not make radar dependent on a single UI backend.
- Do not tie radar implementation to one renderer backend.
- Do not widen scope into unrelated gameplay migration or non-radar UI refactors.

## Validation
From repo roots as appropriate:

```bash
# Build with renderer + UI backend coverage for radar slices
cd m-karma
./abuild.py -c -d <build-dir> -b bgfx,diligent,imgui,rmlui

# Renderer contract regression (required when renderer/backend files are touched)
./scripts/test-engine-backends.sh <build-dir>

# Runtime smoke (radar/HUD visibility path in both renderer and UI backends)
cd m-bz3
timeout 20s ./<build-dir>/bz3 --backend-render bgfx --backend-ui imgui --data-dir ./data --user-config data/client/config.json
timeout 20s ./<build-dir>/bz3 --backend-render diligent --backend-ui imgui --data-dir ./data --user-config data/client/config.json
timeout 20s ./<build-dir>/bz3 --backend-render bgfx --backend-ui rmlui --data-dir ./data --user-config data/client/config.json
timeout 20s ./<build-dir>/bz3 --backend-render diligent --backend-ui rmlui --data-dir ./data --user-config data/client/config.json

# Docs structure gate
cd m-overseer
./agent/scripts/lint-projects.sh
```

## Trace Channels
- Existing channels to use immediately:
  - `render.system`
  - `render.bgfx`
  - `render.diligent`
  - `ui.system`
  - `ui.system.imgui`
  - `ui.system.rmlui`
- Add during implementation:
  - `game.radar` (game-owned radar flow)
  - `ui.system.radar` (UI radar texture bind/composition diagnostics)

## Current Status
- `2026-02-18`: research baseline completed from:
  - `q-karma` engine-first offscreen camera/render-target/UI handle model,
  - `m-dev` game-owned radar behavior and richer gameplay semantics,
  - `m-karma` + `m-bz3` current renderer/UI substrate gaps and existing radar assets.
- `2026-02-18`: direction lock established:
  - engine owns generic PiP substrate,
  - game owns radar semantics and behavior.
- `2026-02-18`: implementation readiness:
  - `m-bz3` already contains radar shaders/config/HUD panel assets,
  - missing engine contracts are now explicitly scoped as R1-R3.
- `2026-02-22`: moved from `ui-radar.md` to `ui/radar.md` and aligned under the `ui.md` superproject.

## Open Questions
- Should camera pass ownership live in `scene::CameraComponent` ECS data, a renderer-pass registry, or a hybrid model?
- How should render-target descriptors be configured (fixed defaults vs explicit per-pass config in game data)?
- Should radar marker representation start as world-space mesh/circle entities (m-dev style) or switch early to symbolic/icon pass primitives?
- What is the minimal stable external-texture API that keeps ImGui and RmlUi parity without backend leakage?
- Should radar orientation policy expose optional modes (`north-up` vs `player-up`) in game config from first slice, or lock to player-up first for parity?

## Handoff Checklist
- [ ] Engine substrate slice and game radar slice remain separated by boundary rules in this doc.
- [ ] Renderer/UI backend parity validations are recorded for every implementation handoff.
- [ ] `m-overseer/agent/projects/ASSIGNMENTS.md` row is updated in the same handoff.
- [ ] Risks and unresolved questions are carried forward explicitly.
