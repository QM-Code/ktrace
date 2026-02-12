# Engine Defaults Model

Status: active architecture guidance for the `engine-defaults-architecture.md` project.

## Goal
Define rewrite-native engine defaults that keep the common path fast to adopt while preserving explicit, bounded override hooks for advanced cases.

## Scope
This document defines architecture contracts for:
- system scheduling/orchestration,
- engine component catalog strategy,
- physics API layering (core plus optional facade),
- audio API layering (core plus optional facade),
- explicit `95% defaults / 5% overrides` policy.

This document does not define backend-specific implementation details.

## 95% Default / 5% Override Policy

### Default path (target: ~95% of use)
- Engine owns lifecycle, ordering, and subsystem initialization.
- Game configures behavior through data/config and stable engine contracts.
- Engine provides high-value convenience facades for common workflows.
- Defaults are deterministic and testable across supported backend combinations.

### Override path (target: ~5% of use)
- Overrides are explicit opt-in APIs, not implicit side effects.
- Overrides stay backend-neutral at the game-facing boundary.
- Overrides must declare ownership, ordering, and failure behavior.
- Override APIs are smaller than default APIs and require focused tests.

### Promotion rule
When a custom path appears in multiple game contexts, promote it into engine defaults only after contract semantics are clear and reusable.

## 1) System Scheduling Model

### Ownership boundaries
- Engine owns the scheduler, phase graph, and system lifetime.
- Game contributes system logic through registration contracts only.
- Backends are not directly visible to game-registered systems.

### Phase model
Use a fixed engine phase order with deterministic execution:
1. `bootstrap` (one-time startup wiring)
2. `pre_fixed`
3. `fixed`
4. `post_fixed`
5. `pre_frame`
6. `frame`
7. `late_frame`
8. `render_prepare`
9. `render_submit`
10. `post_frame`
11. `shutdown` (one-time teardown)

### Dependency expression model
Each system registration should declare:
- `system_id` (stable unique identifier),
- `phase`,
- `runs_when_paused` (default `false`),
- `depends_on` (hard order dependencies),
- `before` / `after` hints (soft ordering),
- `threading_mode` (default `main_thread`).

### Update ordering guarantees
- Phase order is fixed and engine-owned.
- Within a phase, execute by topological sort over `depends_on`.
- Ties are resolved by deterministic `system_id` ordering.
- Cycles are startup errors (fail-fast with actionable diagnostics).

### Default path (95%)
- Engine registers and runs a built-in baseline system set.
- Game adds content/rules to existing systems without changing schedule ownership.
- Most games only toggle system behavior through config and content.

### Override path (5%)
- Game may register additional systems in existing phases.
- Game may replace a default system only through explicit override registration.
- New custom phases are disallowed by default; allow only with explicit engine-level approval plus contract docs.

### Validation expectations
- Add deterministic scheduler-order tests for dependency and tie-break behavior.
- Add one failure-path test for cycle detection diagnostics.

## 2) Engine Component Catalog Strategy

### Catalog objectives
- Provide reusable, engine-owned component building blocks.
- Keep component ownership and serialization rules explicit.
- Avoid gameplay rule leakage into engine component definitions.

### Core catalog families
1. Lifecycle and identity:
   - entity id, enabled state, lifetime tags.
2. Spatial:
   - transform, hierarchy/parenting, bounds metadata.
3. Runtime presentation:
   - render proxy references, visibility flags, debug draw tags.
4. Simulation integration:
   - physics body binding, motion-state cache, collision-event inbox.
5. Audio integration:
   - emitter state, event trigger queue, listener marker.
6. Networking hooks:
   - replication tags, interpolation state, authority mode tag.

### Ownership and lifecycle rules
- Engine owns storage and lifetime for engine catalog components.
- Systems own mutation windows for their components inside scheduled phases.
- Game writes through contracts; direct mutation of backend state is not allowed.

### Serialization and config rules
- Each engine component declares:
  - serialization mode (`runtime-only`, `saveable`, or `replicated`),
  - default initialization behavior,
  - versioning expectations for schema evolution.
- Config defaults are authoritative for optional tuning values.

### Default path (95%)
- Game composes entities from engine catalog components and preset archetype bundles.
- Most gameplay uses existing component types plus content/config values.

### Override path (5%)
- Game may register custom components with explicit metadata:
  - ownership system,
  - serialization mode,
  - compatibility/version notes.
- Custom components must not redefine semantics of existing engine catalog components.

### Validation expectations
- Add catalog registration validation for duplicate ids and missing ownership.
- Add round-trip coverage for each serializable engine component family.

## 3) Physics API Layering

### Core backend-neutral contract (mandatory)
The core physics API should remain small and stable:
- world lifecycle (`init`, `shutdown`, `step`),
- body lifecycle (`create`, `destroy`, validity checks),
- state access (`transform`, `linear/angular velocity`, gravity/lock toggles),
- base queries (`raycast closest`, overlap primitives),
- deterministic error behavior for invalid inputs.

### Optional default facade (95%)
Provide a convenience layer for common gameplay needs:
- spawn helpers for static/dynamic bodies with engine defaults,
- common query helpers (ground probe, forward hit probe),
- constraint presets for common lock/use cases,
- config-driven default materials/damping.

The facade should call into the same core contract and never bypass it.

### Explicit advanced path (5%)
- Expose advanced operations through a separate `PhysicsAdvanced` contract surface.
- Keep advanced APIs backend-neutral and capability-gated.
- Use explicit data structs for advanced options rather than backend handles.

### Non-goals
- No game-facing exposure of backend SDK types or pointers.
- No backend-specific behavior switches in gameplay code.

### Validation expectations
- Core parity tests remain authoritative across compiled physics backends.
- Any new facade helper requires at least one deterministic behavior assertion.

## 4) Audio API Layering

### Core backend-neutral contract (mandatory)
The core audio API should remain stable and minimal:
- lifecycle (`init`, `shutdown`, per-frame update),
- listener state updates,
- one-shot and controllable voice playback,
- stop/pause/resume and gain/pitch/loop controls,
- deterministic handling of invalid/non-finite requests.

### Optional default facade (95%)
Provide a convenience layer for common use:
- event-based sound triggers (`play_event`, `stop_event_group`),
- default bus routing (`ui`, `world`, `music`, `sfx`),
- positional defaults (attenuation/pan presets),
- voice budgeting with engine-managed priorities.

The facade should map to core primitives and stay backend-neutral.

### Explicit advanced path (5%)
- Expose advanced routing/streaming controls through separate optional contracts.
- Keep explicit capability checks for optional advanced features.
- Ensure all advanced features degrade predictably when unsupported.

### Non-goals
- No game-facing backend device/voice objects.
- No backend-specific command branching in game logic.

### Validation expectations
- Existing backend smoke tests remain required.
- Add deterministic assertions for every newly introduced facade rule.

## 5) Implementation Sequence (Bounded)
1. Lock scheduler contract shape and deterministic ordering guarantees.
2. Define initial engine component catalog metadata schema and validation rules.
3. Stabilize physics core contract semantics before adding new facade helpers.
4. Stabilize audio core contract semantics before expanding event-level facade APIs.
5. Add explicit advanced-path interfaces only after default-path behavior is tested and documented.

## 6) Required Cross-Doc Sync
When this model changes, update:
- `docs/projects/engine-defaults-architecture.md` (status, open questions, next tasks),
- `docs/projects/core-engine-infrastructure.md` (implementation sequencing impact),
- `docs/projects/physics-backend.md` and `docs/archive/audio-backend-completed-2026-02-12.md` (contract implications; audio snapshot is reference-only unless reopened),
- `docs/projects/ASSIGNMENTS.md` (owner/status/next task).

## Open Questions
- Should component catalog metadata live as code-adjacent manifests, docs-first tables, or both?
- Which advanced physics/audio capability gates should be standardized first after core stabilization?
