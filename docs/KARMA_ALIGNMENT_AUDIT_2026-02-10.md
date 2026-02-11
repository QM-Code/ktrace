# KARMA Alignment Audit (2026-02-10)

## Purpose
Persist the current cross-codebase feature-gap analysis so future overseers/specialists do not need to repeat this discovery pass.

This audit focuses on capabilities present in `KARMA-REPO` that are not yet represented in `m-rewrite/src/engine/*`, excluding the already-active renderer backend deep-dive track.

## Alignment Principle (Locked)
- Port by capability and behavior, not by file/directory mirroring.
- `m-rewrite` architecture ownership remains authoritative (`engine` owns lifecycle/subsystems; `game` owns BZ3-specific rules/content).
- `KARMA-REPO` is a capability reference and exploratory signal, not a structural template.

## High-Confidence Gaps Outside Renderer Backend Internals

### 1) Engine-level network transport abstraction missing in rewrite engine
- KARMA has generic transport interfaces and ENet factories:
  - `KARMA-REPO/include/karma/network/transport.h:33`
  - `KARMA-REPO/include/karma/network/enet_transport.h:9`
- Rewrite currently keeps networking under game runtime paths:
  - `m-rewrite/src/game/net`
  - `m-rewrite/src/game/server/net`
  - `m-rewrite/src/game/client/net`
- Impact:
  - Harder to reuse transport capability as engine-owned subsystem across projects.

### 2) System graph scheduling layer absent in rewrite engine
- KARMA has ordered/dependency-based system execution:
  - `KARMA-REPO/include/karma/systems/system_graph.h:17`
  - `KARMA-REPO/include/karma/systems/system.h:9`
- Rewrite engine app currently orchestrates subsystems directly:
  - `m-rewrite/src/engine/app/engine_app.cpp:222`
  - `m-rewrite/src/engine/app/engine_app.cpp:252`
- Impact:
  - Less declarative scheduling/composability for future subsystem growth.

### 3) Engine component catalog is thinner in rewrite
- KARMA exposes reusable engine-side components (examples):
  - environment: `KARMA-REPO/include/karma/components/environment.h:9`
  - light: `KARMA-REPO/include/karma/components/light.h:8`
  - player controller intent: `KARMA-REPO/include/karma/components/player_controller.h:12`
- Rewrite scene components are currently minimal:
  - `m-rewrite/include/karma/scene/components.hpp:13`
- Impact:
  - Some engine-agnostic data contracts remain implicit or game-side.

### 4) Engine debug/editor overlay layer missing
- KARMA debug overlay layer:
  - `KARMA-REPO/include/karma/debug/debug_overlay.h:22`
  - `KARMA-REPO/src/debug/debug_overlay.cpp:8`
- No equivalent debug overlay module exists under `m-rewrite/src/engine`.
- Impact:
  - Lower observability/edit-time introspection in-engine.

### 5) Physics abstraction model divergence (capability parity still open)
- KARMA exposes object wrappers and world facade:
  - `KARMA-REPO/include/karma/physics/physics_world.hpp:18`
  - `KARMA-REPO/include/karma/physics/rigid_body.hpp:11`
  - `KARMA-REPO/include/karma/physics/player_controller.hpp:10`
- Rewrite uses BodyId/backend-contract-oriented system API:
  - `m-rewrite/include/karma/physics/backend.hpp:50`
  - `m-rewrite/include/karma/physics/physics_system.hpp:10`
- Impact:
  - Different ergonomics and extension points; parity requires explicit product/API decision, not direct port.

### 6) Audio model divergence (clip/ECS semantics vs request/voice API)
- KARMA clip-centric API + ECS audio system:
  - `KARMA-REPO/include/karma/audio/audio.h:13`
  - `KARMA-REPO/include/karma/audio/audio_system.h:15`
  - backend clip interfaces:
    - `KARMA-REPO/include/karma/audio/backends/sdl/clip.hpp:10`
    - `KARMA-REPO/include/karma/audio/backends/miniaudio/clip.hpp:10`
- Rewrite uses backend-neutral request/voice model:
  - `m-rewrite/include/karma/audio/backend.hpp:36`
  - `m-rewrite/include/karma/audio/audio_system.hpp:10`
- Impact:
  - Different user-facing runtime semantics; parity should be judged by behavior outcomes, not class shape.

### 7) Platform backend breadth differs
- KARMA ships real GLFW + SDL implementations:
  - `KARMA-REPO/src/platform/backends/window_glfw.cpp:1`
  - `KARMA-REPO/src/platform/backends/window_sdl.cpp:1`
- Rewrite enforces an SDL3-only active platform backend path:
  - `m-rewrite/src/engine/platform/backends/window_sdl3.cpp:1`
  - `m-rewrite/src/engine/platform/window_factory.cpp:1`
- Impact:
  - Cross-platform backend breadth is intentionally narrower in rewrite at present to preserve a thin, deterministic SDL3-first seam.

### 8) Renderer front-end API surface is currently narrower in rewrite
- KARMA device-level API includes render targets, line draw, environment/shadow controls, UI draw:
  - `KARMA-REPO/include/karma/renderer/device.h:28`
- Rewrite device-level API currently exposes a leaner subset:
  - `m-rewrite/include/karma/renderer/device.hpp:16`
- Note:
  - Renderer parity is already the active top-priority track.

## Important Non-Gap/Intentional Differences
- Rewrite already has features not central in KARMA’s old app shape (e.g., server app split, content archive/resolver infrastructure, game/server runtime boundaries).
- Some KARMA-era APIs are replaced in rewrite by newer contracts; these are not automatically regressions.

## Draft Ownership Framework (KARMA Intent vs Rewrite Authority)
Use this before deciding whether to port, adapt, or reject a KARMA capability.

### Prefer KARMA intent when:
- The capability is engine-agnostic and currently absent in rewrite engine.
- It improves default-path productivity (95% case) without backend leakage.
- Rewrite has no equivalent behavior outcome yet.

### Prefer rewrite authority when:
- Rewrite already provides equivalent or better behavior via a different contract.
- KARMA approach would violate rewrite boundaries (`engine` vs `game`) or introduce backend API exposure.
- KARMA implementation is exploratory debt that conflicts with current architecture quality bar.

### Require explicit decision note when:
- Matching KARMA capability would force rewrite contract expansion.
- Two backends cannot achieve equivalent behavior without a policy choice.
- A candidate port moves ownership from engine to game or vice versa.

## Decision Seeds (For Next Alignment Session)
These are intentionally not finalized in this document.

1. Network ownership boundary:
- Keep transport as game-domain only, or promote generic transport interfaces into engine?

2. Systems scheduling:
- Introduce a rewrite-native system graph, or keep explicit app orchestration?

3. Component catalog strategy:
- Expand engine-level components (light/environment/controller intent), or keep current minimal scene model and expose via other contracts?

4. Physics API direction:
- Retain BodyId-first API only, or add higher-level wrappers for common control flows?

5. Audio API direction:
- Keep request/voice model as primary, or add clip-style facade for ergonomics while preserving backend abstraction?

## Suggested Use
- Treat this as the baseline reference during parity roadmap updates.
- Update this file only when a capability changes status or an ownership decision is locked.
