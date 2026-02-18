# Physics Refactor (KARMA Alignment)

## Project Snapshot
- Current owner: `specialist-physics-refactor`
- Status: `in progress` (Phase 4a substrate slice landed: backend-neutral collider-shape + trigger/filter runtime property hooks are wired through PhysicsSystem and consumed by ECS sync)
- Supersedes: `docs/projects/physics-backend.md` (retired to `docs/archive/physics-backend-retired-2026-02-17.md`)
- Immediate next task: execute next Phase 4 follow-up to deepen backend-native runtime behavior parity (trigger/filter semantics + shape/material/static-mesh/controller ingestion depth) and reduce fallback-only paths.
- Validation gate: `./scripts/test-engine-backends.sh <build-dir>`

## Mission
Align `m-rewrite` physics architecture with the direction proven in `KARMA-REPO`: world-level physics API, collider/component-driven sync, controller-based character collision, static mesh collision intake, and backend-equivalent behavior across compiled physics backends.

The goal is behavioral and architectural alignment, not literal file-for-file copying.

## Foundation References
- `docs/foundation/architecture/core-engine-contracts.md` (physics sections)
- `docs/foundation/governance/engine-backend-testing.md`
- `docs/foundation/policy/decisions-log.md` (BodyId core + optional facade direction)
- `KARMA-REPO/include/karma/physics/*`
- `KARMA-REPO/src/physics/*`
- `KARMA-REPO/examples/main.cpp`

## Why This Is Separate
- Existing rewrite physics work is mostly backend-core parity (`BodyId`, gravity flag, locks, raycast).
- KARMA-aligned intake requires wider architectural changes (public API shape, ECS component model, sync system, backend capability depth, engine loop wiring).
- This is a major foundation realignment and should be tracked as a dedicated project.

## Owned Paths
- `m-rewrite/include/karma/physics/*`
- `m-rewrite/src/engine/physics/*`
- `m-rewrite/src/engine/physics/tests/*`
- `m-rewrite/include/karma/app/*` and `m-rewrite/src/engine/app/*` (physics-facing lifecycle hooks)
- `m-rewrite/include/karma/scene/*` and `m-rewrite/src/engine/scene/*` (if physics components/sync hooks land here)
- `m-rewrite/src/engine/CMakeLists.txt`
- `m-rewrite/docs/projects/physics-refactor.md`
- `m-rewrite/docs/projects/ASSIGNMENTS.md`

## Direction Lock
- We accept intentional breakage in currently migrated gameplay code while refactoring this foundation.
- Priority is physics architecture alignment first; game repair follows afterward.

## KARMA Intake Checklist (Definition of Alignment)
- [ ] Physics-backed world API supports dynamic rigid bodies, static mesh collision, player controller creation, gravity control, and raycast.
- [ ] ECS physics sync system owns collider-to-physics object lifecycle and transform/velocity write-back.
- [ ] Collider component model includes box/sphere/capsule/mesh and trigger semantics.
- [ ] Player controller component + backend path supports collision-driven movement and grounded behavior.
- [ ] Player controller shape/size updates are detected and reconciled (including controlled recreation when required).
- [ ] Static world geometry collision can be created from mesh assets through physics backend path.
- [ ] Collision enablement/filter intent is represented at component level and honored by sync path.
- [ ] Cleanup of stale physics objects (destroyed/changed entities) is deterministic.
- [ ] Both compiled physics backends expose equivalent contract behavior for these capabilities.
- [ ] A demo-level scenario equivalent to `KARMA-REPO/examples/main.cpp` physics flow is possible in rewrite without backend leakage.

## Target Architecture in `m-rewrite`

### 1) Public Gameplay Physics Surface
- Introduce/restore a KARMA-like public surface under `karma::physics`:
  - `World`
  - `RigidBody`
  - `StaticBody`
  - `PlayerController`
- Maintain overlap with KARMA interface shape where practical:
  - `createBoxBody(...)`
  - `createStaticMesh(...)`
  - `createPlayer(...)`
  - `playerController()`
  - `raycast(...)`
- Keep backend specifics hidden from game code.

### 2) Core Backend-Neutral Layer
- Preserve a backend-neutral core (existing BodyId-backed capabilities are useful and should be retained as internal substrate).
- Normalize layering so higher-level world/component flows map onto this substrate.
- Keep deterministic invalid-input and stale-handle behavior from current parity work.

### 3) ECS/Scene Physics Component Layer
- Add/normalize physics data components equivalent to KARMA intent:
  - rigidbody
  - colliders (box/sphere/capsule/mesh)
  - player controller intent
  - collision visibility/layer mask intent
- Decide final namespace ownership (`scene` vs dedicated `components`) early and keep consistent.

### 4) Physics Sync System
- Implement KARMA-style sync responsibilities:
  - create/update/destroy runtime rigid bodies from ECS components
  - create static mesh collision from mesh components
  - maintain player controller runtime object from controller + collider components
  - apply dirty transform pushes and pull simulated state back to ECS
  - enforce collision-enabled gating and stale cleanup

### 5) Engine Loop Integration
- Ensure game-facing fixed-step hooks can drive physics-intent updates before physics simulation.
- Ensure physics simulation and ECS sync order is explicit and deterministic.
- Keep backend selection/runtime ownership policy consistent with existing engine architecture.

## Carried-Forward Assets From Retired `physics-backend.md`
- Existing backend selection/lifecycle scaffolding is already working.
- Jolt + PhysX compiled backend parity harness exists and should remain authoritative.
- Implemented parity slices to preserve:
  - closest-hit raycast query path
  - dynamic gravity enable flag API
  - creation-time rotation lock behavior
  - creation-time translation lock behavior
- Existing explicit TODOs to carry forward into new contracts:
  - standardized start-inside ray semantics
  - filter/layer query contract design
  - optimize native-hit to logical-id mapping where needed

These are not discarded; they become the lower-layer regression base for the refactor.

## Execution Plan

### Phase 0: Contract Reset and Migration Scaffolding
1. Freeze direction and publish breaking-change intent in docs/trace notes.
2. Define target public physics API (KARMA-overlap) and compatibility stance for existing `PhysicsSystem` usage.
3. Decide whether current BodyId API remains internal-only or dual-exposed during transition.
4. Add migration TODO matrix mapping old interfaces/tests to new layers.

Exit criteria:
- `physics-refactor.md` accepted as authoritative plan.
- No ambiguity on public API target and transition path.

### Phase 1: Public World/Body/Controller API Introduction
1. Add `World`, `RigidBody`, `StaticBody`, `PlayerController` wrappers in rewrite.
2. Expose world-level calls overlapping KARMA behavior where sensible.
3. Keep backend abstraction hidden and backend-neutral.
4. Add compile-time and runtime guards for invalid lifecycle usage.

Exit criteria:
- New public physics API compiles and can be instantiated.
- No backend objects leak through public headers.

### Phase 2: Component Model Expansion
1. Introduce physics component set (rigidbody/collider/controller/visibility mask intent).
2. Ensure component validation constraints are explicit (for example controller requires compatible collider).
3. Define authoritative ownership for transform fields and dirty-state semantics.

Exit criteria:
- Components exist, are serializable where required, and have documented invariants.

### Phase 3: ECS Sync System (KARMA-Style Behavior)
1. Implement rigid-body sync from components.
2. Implement static mesh collider creation path.
3. Implement player-controller sync path, including shape/size-change handling.
4. Implement simulated-state write-back and deterministic stale cleanup.

Exit criteria:
- Entity/component-driven collision behavior is active.
- Controller-driven movement can collide against static mesh collision.

### Phase 4: Backend Capability Depth (Jolt + PhysX)
1. Ensure backend adapters support required world/controller/static-mesh capabilities.
2. Normalize grounded, trigger, kinematic, gravity semantics at contract level.
3. Ensure shape handling and recreation semantics are equivalent across backends.
4. Keep deterministic failure mapping and trace quality.

Exit criteria:
- Both backends pass the same capability-level contract suite for new features.

### Phase 5: Engine and Game-Facing Integration
1. Wire fixed-step game hooks and physics intent timing as needed for controller flow.
2. Replace ad-hoc gameplay collision approximations with physics-backed paths.
3. Accept temporary game breakage while foundation lands; fix-up can follow as separate slices.

Exit criteria:
- Engine supports KARMA-style controller/collider update flow end-to-end.

### Phase 6: Validation, Parity, and Stabilization
1. Keep existing backend parity tests green for preserved low-level contracts.
2. Add/refine higher-level tests for:
  - ECS sync lifecycle
  - controller collision/grounding
  - static mesh collision intake
  - collider mutation/recreation behavior
  - collision mask/filter semantics
3. Add scenario smoke equivalent to KARMA demo collision behavior.

Exit criteria:
- Lower-layer parity + higher-layer KARMA-aligned behavior tests pass in both backends.

## Required Validation Commands
From `m-rewrite/`:

```bash
./abuild.py -c --test-physics -d <build-dir> -b jolt,physx
./scripts/test-engine-backends.sh <build-dir>
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `engine.app`
- `engine.server`
- `engine.sim`
- `engine.sim.frames`
- `physics.system`
- `physics.jolt`
- `physics.physx`

## Open Decisions (Resolve Early)
- Keep existing `physics::PhysicsSystem` type name as backend substrate, or repurpose it as ECS sync system and rename substrate.
- Whether to preserve BodyId API as supported public advanced path or internal-only implementation detail.
- Component namespace location for rewrite physics components.
- Exact collision-layer/filter contract shape for first aligned slice.
- Runtime lock mutation/query policy in the new layered API.

## Phase 0/1 Contract Slice (2026-02-18)
Landed in this bounded slice:
- Public KARMA-aligned facade surface added under `include/karma/physics/*`:
  - `World`
  - `RigidBody`
  - `StaticBody`
  - `PlayerController`
  - `PhysicsMaterial` (`types.hpp`)
- Engine-side facade scaffolding added under `src/engine/physics/*`, mapping facade calls onto existing `PhysicsSystem` + `BodyId` substrate.
- Existing `PhysicsSystem` parity API remains intact and supported (additive compatibility model, no removal).
- Backend internals remain hidden from the new public facade headers.
- Parity harness now includes a bounded world-facade smoke path (`RunFacadeScaffoldChecks`) to prove compile/link/runtime surface viability per backend.

Intentionally deferred (explicit non-goals for this slice):
- Real static-mesh collision ingestion from mesh assets (`createStaticMesh` currently uses placeholder static-body scaffolding).
- Controller grounded/collision semantics and shape reconciliation policies.
- True half-extents/material propagation into backend shape creation.
- ECS component model and sync-system migration (Phase 2/3 work).

## Phase 2a Component Contract Slice (2026-02-18)
Landed in this bounded slice:
- Scene-owned physics component contracts introduced under `include/karma/scene/physics_components.hpp` and re-exported via `include/karma/scene/components.hpp`.
- Added backend-neutral component data contracts for:
  - rigidbody intent,
  - collider intent (shape + trigger + enabled + collision layer/filter intent),
  - player-controller intent,
  - collision mask intent.
- Added bounded validation helpers for component contracts (finite-value checks, positive-dimension checks, mesh-path non-empty checks, collision-mask checks).
- Added parity-test coverage for:
  - default validity,
  - invalid-input rejection,
  - ECS storage/view behavior for new scene physics components.
- Closed carry-over world-facade issues:
  - borrowed-system `World::shutdown()` no longer shuts down externally provided `PhysicsSystem`,
  - `World::raycast(...)` no longer fabricates normals; phase contract now returns trustworthy hit position and zeros the normal field.

Explicit deferrals for next phases:
- `Phase 2b`:
  - component-level transform ownership/dirty-state policy,
  - shape-specific mutation/reconciliation policy (controller+collider coupling rules),
  - collision filter/layer contract tightening beyond current mask baseline.
- `Phase 3`:
  - ECS physics sync system implementation (create/update/destroy runtime objects, write-back ordering, stale cleanup),
  - static mesh ingestion pipeline hookup,
  - controller grounded/collision-driven runtime behavior.

## Phase 2b Policy Hardening Slice (2026-02-18)
Landed in this bounded slice:
- Added explicit transform ownership contract utilities in `include/karma/scene/physics_components.hpp`:
  - authority model (`scene-authoritative` vs `physics-authoritative`),
  - coherent dirty/push/pull validation helper for ownership intent,
  - helper predicates for scene->physics push and physics->scene pull policy decisions.
- Added shape-specific collider reconciliation policy:
  - reconcile action classifier (`no-op`, `runtime property update`, `runtime shape rebuild`, `reject invalid`),
  - shape-parameter diff handling keyed by active collider shape contract.
- Added controller/collider compatibility classification policy:
  - compatibility states for missing/invalid/disabled/trigger/unsupported-shape cases,
  - helper to collapse classification to a bounded compatible/not-compatible decision.
- Added bounded parity coverage for all Phase 2b policies:
  - transform ownership validation + helper behavior,
  - collider reconcile-action classification,
  - controller/collider compatibility classification.

Explicit Phase 3 deferrals after Phase 2b closure:
- No ECS sync-system runtime wiring yet (no runtime create/update/destroy from ECS components).
- No runtime reconcile execution against backend objects yet (policy helpers are contract-level only).
- No gameplay/controller integration or grounded-runtime behavior changes yet.

## Phase 3 ECS Sync Slice 1 (2026-02-18)
Landed in this bounded slice:
- Added `src/engine/physics/ecs_sync_system.hpp/.cpp` with deterministic two-phase flow:
  - pre-sim: validate intents, reconcile runtime bodies (create/rebuild/update/teardown), apply scene->physics transform push by ownership policy.
  - post-sim: apply physics->scene transform pull by ownership policy.
- Consumes Phase 2 policy helpers from `include/karma/scene/physics_components.hpp`:
  - `ValidateRigidBodyIntent`, `ValidateColliderIntent`, `ValidateTransformOwnership`,
  - `ClassifyColliderReconcileAction`,
  - `ShouldPushSceneTransformToPhysics`, `ShouldPullPhysicsTransformToScene`,
  - `ClassifyControllerColliderCompatibility` / `IsControllerColliderCompatible`.
- Runtime support in this slice is explicitly Box-only for colliders; unsupported/invalid collider intent is deterministically rejected/teardown.
- Added deterministic stale cleanup for destroyed entities and entities that no longer satisfy required component sets.
- Added bounded sync-system parity checks in `physics_backend_parity_test.cpp` per backend for:
  - create on valid entity,
  - rebuild on shape-parameter mutation,
  - teardown on invalid intent,
  - scene-authoritative push behavior,
  - physics-authoritative pull behavior,
  - cleanup after entity destroy.
- Added minimal sync-system introspection helpers used by parity tests (binding existence/count, runtime body lookup, transform snapshot).

Explicit remaining deferrals after this Phase 3 slice:
- No static-mesh ingestion/runtime path yet.
- No controller runtime object lifecycle or grounded behavior implementation yet.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.
- No non-Box collider runtime ingestion (unsupported shapes remain deterministic reject/teardown in this slice).

## Phase 3 ECS Sync Slice 2 (2026-02-18)
Landed in this bounded slice:
- Extended `EcsSyncSystem` runtime lifecycle behavior to include static mesh placeholder support under policy constraints:
  - `ColliderShapeKind::Mesh` now participates in runtime lifecycle for static intent (`dynamic=false`),
  - runtime object creation remains placeholder/substrate-backed (no mesh-geometry ingestion yet),
  - invalid transitions (for example mesh + dynamic=true) deterministically teardown runtime state.
- Added bounded controller lifecycle metadata wiring in ECS sync:
  - compatibility transitions are classified each pre-sim tick using Phase 2 policy helpers,
  - compatible states create/update controller runtime metadata bound to the entity,
  - incompatible states deterministically teardown runtime body + controller metadata,
  - controller component removal clears controller metadata while preserving body lifecycle when otherwise valid.
- Closed the remaining `ColliderReconcileAction::UpdateRuntimeProperties` behavior gap for enabled-state transitions:
  - collider `enabled=false` now deterministically maps to teardown/no-runtime,
  - re-enable (`enabled=true`) deterministically recreates runtime body on the next valid pre-sim pass.
- Added sync introspection helpers for controller metadata state, used only by parity tests.
- Added bounded parity coverage per backend for:
  - mesh-placeholder lifecycle create/teardown/recreate behavior,
  - controller compatibility transition lifecycle effects,
  - collider enabled toggle teardown/recreate behavior,
  - stale cleanup invariants after component mutation and entity destroy.

Explicit remaining deferrals after this Phase 3 follow-up:
- No real static-mesh collision ingestion pipeline (placeholder mesh runtime path only).
- No backend-native player-controller runtime object or grounded/movement behavior.
- No backend mutation path yet for trigger/filter `UpdateRuntimeProperties` transitions beyond deterministic caching/deferral.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4a Substrate Hooks Slice (2026-02-18)
Landed in this bounded slice:
- Extended substrate contracts in `include/karma/physics/backend.hpp` and `include/karma/physics/physics_system.hpp`:
  - `BodyDesc` now carries backend-neutral collider shape descriptor (`Box`/`Sphere`/`Capsule`) and shape parameters.
  - Added backend-neutral runtime collider property APIs for trigger state and collision layer/filter mask (`set/get`).
- Implemented `PhysicsSystem` passthroughs in `src/engine/physics/physics_system.cpp` for the new runtime collider property APIs.
- Implemented backend behavior in `backend_jolt_stub.cpp` and `backend_physx_stub.cpp`:
  - `createBody` now consumes shape descriptor for `Box`/`Sphere`/`Capsule` creation.
  - runtime trigger/filter APIs now expose deterministic success/failure and coherent reported state.
  - bounded deterministic fallback behavior is explicit where runtime mutation is unsupported:
    - Jolt reports unsupported runtime collision-mask transition mutations (enabling caller-driven deterministic fallback).
- Updated ECS sync (`src/engine/physics/ecs_sync_system.cpp`) to:
  - pass collider shape parameters + trigger/filter intent into `BodyDesc` during runtime body creation,
  - execute `UpdateRuntimeProperties` via new substrate APIs,
  - preserve deterministic enabled teardown/recreate behavior,
  - apply deterministic rebuild fallback when runtime trigger/filter mutation reports unsupported.
- Added parity coverage in `physics_backend_parity_test.cpp` for:
  - Box/Sphere/Capsule creation path sanity,
  - runtime trigger/filter mutation roundtrip contracts,
  - ECS sync trigger/filter transitions with both success and deterministic fallback paths.

Explicit remaining deferrals after Phase 4a:
- No real static-mesh geometry ingestion pipeline (mesh runtime path remains placeholder-backed).
- No backend-native player-controller runtime object or grounded/movement behavior.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Current Status
- `2026-02-17`: Project created as full replacement for backend-only parity track.
- `2026-02-17`: `physics-backend.md` retired and subsumed into this plan.
- Existing backend-core parity work is preserved as foundational input, not discarded.
- `2026-02-18`: Phase 0/1 bounded scaffolding slice landed (`World`/`RigidBody`/`StaticBody`/`PlayerController` facade + minimal engine implementation over `PhysicsSystem`).
- `2026-02-18`: Required validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
- `2026-02-18`: Phase 2a landed:
  - scene physics component contracts + validation helpers,
  - parity coverage for component validity/invalid-input/ECS-view behavior,
  - world facade shutdown ownership and raycast-normal contract fixes.
- `2026-02-18`: Phase 2a validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
- `2026-02-18`: Phase 2b landed:
  - transform authority ownership contract + validation helpers,
  - shape-specific collider reconcile-action classification policy,
  - controller/collider compatibility classification policy,
  - bounded parity coverage for all Phase 2b policy helpers.
- `2026-02-18`: Phase 2b validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
- `2026-02-18`: Phase 3 ECS sync slice 1 landed:
  - new `EcsSyncSystem` runtime added with deterministic pre/post simulation ordering,
  - policy-driven runtime lifecycle + transform push/pull behavior implemented for Box colliders,
  - parity coverage added for create/rebuild/teardown/push/pull/cleanup behavior across Jolt/PhysX.
- `2026-02-18`: Phase 3 slice 1 validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
- `2026-02-18`: Phase 3 ECS sync slice 2 landed:
  - static mesh placeholder lifecycle added for `dynamic=false` mesh collider intent,
  - controller compatibility-driven runtime metadata lifecycle added,
  - deterministic `enabled` teardown/recreate behavior landed for `UpdateRuntimeProperties`.
- `2026-02-18`: Phase 3 slice 2 validation completed in `build-a3`:
  - `./abuild.py --lock-status -d build-a3` (owner verified)
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
- `2026-02-18`: Phase 4a substrate hooks slice landed:
  - backend-neutral collider-shape descriptor + runtime trigger/filter APIs added to substrate contract,
  - Jolt/PhysX substrate implementations updated for shape creation and deterministic runtime property mutation behavior,
  - ECS sync now applies trigger/filter runtime transitions through substrate APIs with deterministic rebuild fallback path.
- `2026-02-18`: Phase 4a validation completed in `build-a3`:
  - `./abuild.py --lock-status -d build-a3` (owner verified)
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
- Next implementation slice: deepen backend-native parity for runtime trigger/filter semantics and advance static-mesh/controller ingestion depth.

## Handoff Checklist
- [x] `physics-refactor.md` remains the single active physics project doc in `docs/projects/`.
- [x] `ASSIGNMENTS.md` row is updated in same handoff.
- [x] Retired project docs are moved under `docs/archive/`.
- [x] Validation commands/results are recorded for implementation slices.
- [x] Contract decisions and behavior changes are reflected in docs as they land.
