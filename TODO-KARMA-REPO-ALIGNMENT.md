# TODO: KARMA-REPO Alignment

This file captures the remaining work to bring `KARMA-REPO/` and `src/engine/`
into full alignment. We are allowed to modify either tree; the goal is a single,
best overall engine API and implementation.

## Remaining alignment work

1) **Public API surface parity**
   - Ensure `include/karma/*` headers in both trees expose the same classes,
     names, and signatures:
     - `EngineApp`, `GameInterface`, `EngineConfig`
     - `ecs::World/Entity/ComponentStorage`
     - `systems::SystemGraph`
     - input mapping types
     - renderer/graphics types (see item 2)

2) **Renderer/graphics/scene alignment**
   - KARMA-REPO has `renderer/` + `scene/` as a core module; `src/engine/` uses
     `graphics/` + `renderer/` + `RendererCore`.
   - Decide whether to:
     - move KARMA-REPO to the **graphics/device + renderer core** model, or
     - fold `src/engine/graphics/*` into KARMA-REPO’s renderer model.
   - This is the largest remaining semantic gap.

3) **ECS + systems alignment**
   - Our ECS lives in `src/engine/ecs/*` + `ecs/systems/*`.
   - KARMA-REPO has `include/karma/ecs` + `include/karma/systems`, but the
     system set and naming still need to match 1:1.
   - Ensure component names/fields match (`Transform`, `Mesh`, `Camera`,
     `Light`, `Collider`, `Rigidbody`, `Audio*`, etc.).

4) **App + loop lifecycle**
   - EngineApp start/tick lifecycle should be identical in both repos.
   - Confirm KARMA-REPO’s `examples/main.cpp` usage matches BZ3 EngineApp
     usage now.

5) **Input mapping**
   - KARMA-REPO input mapping headers should match the `input/mapping` API and
     keybinding config flow used in `src/engine/`.

6) **Build/export parity**
   - CMake target names, export sets, and install layout should match
     (`karma`, `karma_server`, `find_package(karma)`).
   - Backend selection options should be consistent across both repos.

7) **Docs**
   - Update KARMA-REPO docs to reflect the glm-based API and the engine-owned
     loop model.

8) **Phase 5 decision (renderer ownership)**
   - KARMA-REPO philosophy implies render orchestration is engine-owned.
   - Decide whether to:
     - Move `src/game/renderer/` responsibilities into engine systems/extension points
       and remove the game renderer, **or**
     - Keep `src/game/renderer/` and update Phase 5’s goal to match reality.

## Suggested next step

Produce a header-by-header mismatch list between:

- `KARMA-REPO/include/karma`
- `include/karma`

and use that as the canonical task list for alignment.
