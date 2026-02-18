# Physics Refactor (KARMA Alignment)

## Project Snapshot
- Current owner: `specialist-physics-refactor`
- Status: `in progress` (Phase 4t validated in `build-a3`; `physics.system` runtime-command traces now emit deterministic stage+operation+outcome+failure-cause tags)
- Supersedes: `docs/projects/physics-backend.md` (retired to `docs/archive/physics-backend-retired-2026-02-17.md`)
- Immediate next task: execute a bounded Phase 4 follow-up to extend runtime-command observability with deterministic tag coverage depth while keeping API/behavior unchanged.
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
- `m-rewrite/include/karma/app/{client,server,shared}/*` and `m-rewrite/src/engine/app/{client,server,shared}/*` (physics-facing lifecycle hooks)
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

## Phase 4b Controller Ingestion Depth Slice (2026-02-18)
Landed in this bounded slice:
- Extended substrate contracts in `include/karma/physics/backend.hpp` and `include/karma/physics/physics_system.hpp` with backend-neutral runtime velocity APIs:
  - `set/get` linear velocity,
  - `set/get` angular velocity,
  - deterministic `false` on invalid/unknown/non-dynamic bodies.
- Implemented `PhysicsSystem` passthrough methods in `src/engine/physics/physics_system.cpp` for the new runtime velocity APIs.
- Implemented backend support in `backend_jolt_stub.cpp` and `backend_physx_stub.cpp`:
  - valid dynamic bodies support linear/angular velocity set/get,
  - static/invalid/unknown/destroyed bodies deterministically return failure.
- Updated ECS sync (`src/engine/physics/ecs_sync_system.hpp/.cpp`) controller path:
  - compatible controller entities now push `desired_velocity` to runtime linear velocity each pre-sim pass,
  - `PlayerControllerIntentComponent.enabled == false` applies deterministic bounded behavior by pushing zero linear velocity effect while keeping controller metadata.
- Added parity coverage in `physics_backend_parity_test.cpp` for:
  - runtime velocity API roundtrip behavior,
  - invalid/unknown/destroyed/static rejection behavior,
  - ECS sync desired-velocity application for enabled vs disabled controller states.

Explicit remaining deferrals after Phase 4b:
- No backend-native player-controller runtime object.
- No grounded/jump/controller collision behavior implementation.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4c Controller-Runtime Parity Slice (2026-02-18)
Landed in this bounded slice:
- Extended scene component policy contracts in `include/karma/scene/physics_components.hpp`:
  - added explicit controller velocity ownership policy helper (`ControllerVelocityOwnership` + classifier helpers),
  - extended controller/collider compatibility classifier to reject enabled controller intent on non-dynamic rigidbodies (`EnabledControllerRequiresDynamicRigidBody`).
- Updated ECS sync runtime behavior in `src/engine/physics/ecs_sync_system.hpp/.cpp`:
  - unified create/update runtime velocity application through policy-driven ownership,
  - enabled + compatible controller now owns runtime linear velocity (`desired_velocity`) and enforces zero runtime angular velocity,
  - disabled or absent controller now defers linear+angular runtime velocity ownership to rigidbody intent,
  - deterministic teardown/rebuild fallback remains in place when runtime velocity mutation fails,
  - enabled-controller/non-dynamic-rigidbody policy-incompatible configurations no longer churn runtime bodies and do not retain controller runtime metadata bindings.
- Added bounded parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` for:
  - enabled -> disabled -> enabled controller velocity ownership transitions,
  - angular-velocity ownership expectations under controller vs rigidbody ownership,
  - explicit incompatible classification for enabled controller + non-dynamic rigidbody,
  - repeated pre-sim stability for rejected controller/non-dynamic configurations (no controller runtime binding retention).

Explicit remaining deferrals after Phase 4c:
- No backend-native player-controller runtime object.
- No grounded/jump/controller collision behavior implementation.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4d Controller-Geometry Reconcile Slice (2026-02-18)
Landed in this bounded slice:
- Extended scene policy contracts in `include/karma/scene/physics_components.hpp`:
  - added controller-geometry reconcile contract helper (`NoOp` / `RebuildRuntimeShape` / `RejectInvalidIntent`).
- Updated ECS sync behavior in `src/engine/physics/ecs_sync_system.hpp/.cpp`:
  - controller geometry mutations (`half_extents`, `center`) now trigger deterministic runtime rebuild when required.
  - enabled + compatible controllers now derive runtime collider geometry from controller dimensions for Box/Capsule runtime creation (bounded mapping), rather than stale collider dimensions.
  - geometry-driven rebuilds preserve runtime transform and linear/angular velocity.
  - deterministic fallback behavior for rebuild/update failure paths remains intact.
- Added bounded parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` for:
  - rebuild on controller geometry mutation,
  - no rebuild when geometry is unchanged,
  - runtime transform/velocity preservation across controller-geometry rebuild.

Explicit remaining deferrals after Phase 4d:
- No backend-native player-controller runtime object.
- No grounded/jump/controller collision behavior implementation.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4e Controller Motion Parity Slice (2026-02-18)
Landed in this bounded slice:
- Reused scene contract helpers in `include/karma/scene/physics_components.hpp` for controller-runtime velocity composition and upward jump applicability (`HasControllerJumpUpwardIntent`, `ComposeControllerRuntimeLinearVelocity`) and added bounded contract assertions in parity tests.
- Updated ECS sync controller velocity ownership behavior in `src/engine/physics/ecs_sync_system.cpp`:
  - enabled + compatible controller now preserves current runtime linear `y`, applies controller `desired_velocity.xz`, and enforces zero angular velocity,
  - one-shot jump is applied only when `jump_requested` is true with positive upward intent, and `jump_requested` is consumed only after successful runtime velocity writes,
  - disabled/absent/incompatible controller paths remain rigidbody-owned for linear+angular velocity and do not consume jump intent,
  - runtime linear-velocity read/write failure still maps to deterministic teardown behavior.
- Added bounded parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` for:
  - enabled-controller vertical-velocity preservation (no per-frame `y` clobber),
  - one-shot jump application + consumption,
  - disabled/incompatible controller non-consumption of jump intent.

Explicit remaining deferrals after Phase 4e:
- No backend-native player-controller runtime object.
- No grounded/controller collision response behavior (jump handling is velocity-intent only in this slice).
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4e Boundary Correction Slice (2026-02-18)
Landed in this bounded slice:
- Removed jump-specific APIs from engine scene-physics contracts in `include/karma/scene/physics_components.hpp`:
  - removed `jump_requested` from `PlayerControllerIntentComponent`,
  - removed jump-specific helper APIs (`HasControllerJumpUpwardIntent`, `ComposeControllerRuntimeLinearVelocity`).
- Updated ECS sync in `src/engine/physics/ecs_sync_system.cpp`:
  - removed all jump read/consume/mutate behavior,
  - restored engine-generic controller velocity ownership behavior: enabled + compatible controller writes `desired_velocity` and zero angular velocity; disabled/absent/incompatible paths stay rigidbody-owned.
- Updated parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp`:
  - removed jump behavior assertions/helper checks,
  - retained engine-generic controller/rigidbody velocity ownership and compatibility lifecycle checks.

Explicit remaining deferrals after Phase 4e boundary correction:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4f Collider Local-Offset Intake Slice (2026-02-18)
Landed in this bounded slice:
- Extended backend-neutral substrate shape descriptor in `include/karma/physics/backend.hpp` with `ColliderShapeDesc.local_center` (create-time collider local offset).
- Implemented offset-aware shape creation in both compiled backends:
  - `src/engine/physics/backends/backend_jolt_stub.cpp`: wraps Box/Sphere/Capsule shapes in a translated shape when `local_center` is non-zero.
  - `src/engine/physics/backends/backend_physx_stub.cpp`: applies `PxShape::setLocalPose` from `local_center` for Box/Sphere/Capsule.
- Kept deterministic invalid-input behavior for the new offset path by rejecting non-finite local-center input in both backends.
- Updated ECS sync runtime shape build path in `src/engine/physics/ecs_sync_system.cpp`:
  - removed controller-center placeholder extents expansion,
  - controller geometry now uses true dimensions (`half_extents`) plus `local_center = controller.center` in the substrate `BodyDesc`.
- Added parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` for:
  - backend-level shape-offset collision-query behavior consistency (including invalid local-offset rejection),
  - ECS controller-center mutation behavior proving underside query shifts in the offset direction across rebuild (distinguishes true offset from extents-expansion workaround).

Explicit remaining deferrals after Phase 4f:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4g Runtime Damping Mutation Parity Slice (2026-02-18)
Landed in this bounded slice:
- Extended scene rigidbody intent contract in `include/karma/scene/physics_components.hpp`:
  - added `linear_damping` + `angular_damping`,
  - validation now enforces finite and non-negative damping values.
- Extended backend-neutral substrate contracts in `include/karma/physics/backend.hpp` + `include/karma/physics/physics_system.hpp`:
  - `BodyDesc` now carries create-time linear/angular damping,
  - added runtime damping APIs (`set/get` linear damping, `set/get` angular damping).
- Implemented `PhysicsSystem` passthroughs in `src/engine/physics/physics_system.cpp` for damping APIs.
- Implemented backend runtime damping behavior in `backend_jolt_stub.cpp` + `backend_physx_stub.cpp`:
  - create-time damping is consumed for dynamic bodies,
  - runtime damping set/get works for valid dynamic bodies,
  - deterministic `false` is returned for invalid/unknown/non-dynamic bodies and invalid damping values.
- Updated ECS sync in `src/engine/physics/ecs_sync_system.cpp`:
  - rigidbody damping is applied in runtime body creation via `BodyDesc`,
  - valid damping intent mutations are applied through runtime substrate APIs without forcing body rebuild/churn,
  - runtime damping mutation failure follows deterministic teardown behavior.
- Added bounded parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` for:
  - damping API roundtrip + invalid/static rejection behavior,
  - ECS damping mutation updates without body-id churn,
  - scene rigidbody damping validation contract checks.

Explicit remaining deferrals after Phase 4g:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4h Runtime Motion-Lock Mutation Parity Slice (2026-02-18)
Landed in this bounded slice:
- Extended backend-neutral substrate contracts in `include/karma/physics/backend.hpp` + `include/karma/physics/physics_system.hpp`:
  - added runtime motion-lock APIs (`set/get` rotation lock, `set/get` translation lock).
- Implemented `PhysicsSystem` passthroughs in `src/engine/physics/physics_system.cpp` for motion-lock APIs.
- Implemented backend runtime behavior:
  - `src/engine/physics/backends/backend_physx_stub.cpp`: true runtime lock mutation for dynamic bodies using PhysX lock flags.
  - `src/engine/physics/backends/backend_jolt_stub.cpp`: deterministic unsupported runtime mutation for lock changes (`false`), with coherent lock-state query support for current runtime state.
- Updated ECS sync in `src/engine/physics/ecs_sync_system.cpp`:
  - lock transitions are no longer unconditional rebuild triggers,
  - runtime lock mutation is attempted first,
  - deterministic rebuild fallback is applied on unsupported/failure paths (no silent no-op).
- Added bounded parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp`:
  - runtime motion-lock API behavior (roundtrip where supported, deterministic unsupported semantics where not),
  - invalid/unknown/non-dynamic rejection checks,
  - ECS lock-mutation behavior (runtime update on supported backend, deterministic rebuild fallback on unsupported backend).
- Phase 4h blocker closure landed:
  - dynamic bodies with both `rotation_locked` and `translation_locked` are now engine-contract invalid intent,
  - scene intent validation rejects this combination before runtime (`ConflictingMotionLocks`),
  - Jolt/PhysX backends reject create/mutation paths deterministically for this combination,
  - ECS sync parity checks now assert teardown/stable rejection/recovery behavior for invalid both-lock intent.

Explicit remaining deferrals after Phase 4h:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4i Runtime Material-Property Mutation Parity Slice (2026-02-18)
Landed in this bounded slice:
- Extended scene component contract in `include/karma/scene/physics_components.hpp`:
  - added collider material intent fields (`friction`, `restitution`),
  - added explicit validation contract (`friction >= 0`, `restitution in [0, 1]`),
  - material deltas are now classified as `ColliderReconcileAction::UpdateRuntimeProperties`.
- Extended backend-neutral substrate contracts:
  - `include/karma/physics/backend.hpp`: `BodyDesc` now carries create-time `friction`/`restitution`,
  - added runtime APIs (`set/get` friction, `set/get` restitution),
  - `include/karma/physics/physics_system.hpp` + `src/engine/physics/physics_system.cpp` passthroughs added.
- Backend implementation details:
  - `src/engine/physics/backends/backend_physx_stub.cpp`:
    - per-body material allocation at runtime body create,
    - runtime friction/restitution set/get implemented (deterministic invalid/unknown rejection).
  - `src/engine/physics/backends/backend_jolt_stub.cpp`:
    - create-time friction/restitution ingestion implemented,
    - runtime friction set/get supported,
    - runtime restitution mutation intentionally reports unsupported on value change (deterministic `false`) to drive fallback coverage.
- Updated ECS sync in `src/engine/physics/ecs_sync_system.cpp`:
  - collider material values are now passed through body creation/rebuild descriptor,
  - `UpdateRuntimeProperties` now applies trigger/filter/material runtime mutation first,
  - deterministic rebuild fallback preserved on any unsupported/failure mutation result.
- Added parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp`:
  - material API checks (roundtrip + invalid/unknown/destroyed rejection),
  - ECS material mutation no-churn success path (friction),
  - ECS deterministic rebuild fallback path (restitution mutation unsupported/failure path).

Explicit remaining deferrals after Phase 4i:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4j Runtime Kinematic-Intent Mutation Parity Slice (2026-02-18)
Landed in this bounded slice:
- Extended scene rigidbody contracts in `include/karma/scene/physics_components.hpp`:
  - added explicit `kinematic` intent field on `RigidBodyIntentComponent`,
  - added validation rule rejecting static+kinematic intent (`InvalidKinematicState`),
  - added `RigidBodyKinematicReconcileAction` classifier (`NoOp`/`UpdateRuntimeKinematic`/`RejectInvalidIntent`).
- Extended backend-neutral substrate contracts:
  - `include/karma/physics/backend.hpp`: `BodyDesc` now carries create-time `is_kinematic`,
  - added runtime APIs (`set/get` kinematic enabled state),
  - `include/karma/physics/physics_system.hpp` + `src/engine/physics/physics_system.cpp` passthroughs landed.
- Backend implementation details:
  - `src/engine/physics/backends/backend_jolt_stub.cpp`:
    - create-time dynamic kinematic ingestion mapped to Jolt `EMotionType::Kinematic`,
    - runtime set/get kinematic mutation support for dynamic bodies,
    - deterministic `false` for invalid/unknown/non-dynamic/static-kinematic-invalid requests.
  - `src/engine/physics/backends/backend_physx_stub.cpp`:
    - create-time kinematic ingestion via `PxRigidBodyFlag::eKINEMATIC`,
    - runtime set/get kinematic mutation support for dynamic bodies,
    - deterministic `false` for invalid/unknown/non-dynamic/static-kinematic-invalid requests.
- Updated ECS sync in `src/engine/physics/ecs_sync_system.cpp`:
  - body creation now propagates rigidbody kinematic intent into `BodyDesc`,
  - kinematic intent transitions now reconcile via runtime mutation first,
  - deterministic rebuild fallback is applied when runtime kinematic mutation fails.
- Added parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp`:
  - kinematic API roundtrip + invalid/unknown/destroyed/static rejection checks,
  - scene contract checks for static+kinematic rejection + kinematic reconcile classification,
  - ECS kinematic toggle behavior with runtime success path and deterministic forced-failure rebuild fallback path.

Explicit remaining deferrals after Phase 4j:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4k Runtime Sleep/Wake Mutation Parity Slice (2026-02-18)
Landed in this bounded slice:
- Extended scene rigidbody contracts in `include/karma/scene/physics_components.hpp`:
  - added explicit runtime awake intent (`awake`) on `RigidBodyIntentComponent`,
  - added `RigidBodyAwakeReconcileAction` classifier (`NoOp`/`UpdateRuntimeAwakeState`/`RejectInvalidIntent`).
- Extended backend-neutral substrate contracts:
  - `include/karma/physics/backend.hpp`: `BodyDesc` now carries create-time `awake`,
  - added runtime awake APIs (`setBodyAwake`/`getBodyAwake`) on backend + `PhysicsSystem` contracts,
  - `include/karma/physics/physics_system.hpp` + `src/engine/physics/physics_system.cpp` passthroughs landed.
- Backend implementation details:
  - `src/engine/physics/backends/backend_jolt_stub.cpp`:
    - create-time awake ingestion mapped to activation/deactivation behavior for dynamic bodies,
    - runtime awake set/get supported for dynamic bodies,
    - deterministic `false` for invalid/unknown/non-dynamic requests.
  - `src/engine/physics/backends/backend_physx_stub.cpp`:
    - create-time awake ingestion mapped to `wakeUp`/`putToSleep` for dynamic bodies,
    - runtime awake set/get supported for dynamic bodies,
    - deterministic `false` for invalid/unknown/non-dynamic requests.
- Updated ECS sync in `src/engine/physics/ecs_sync_system.cpp`:
  - body creation now propagates rigidbody awake intent into `BodyDesc`,
  - awake transitions reconcile via runtime mutation first,
  - runtime mutation failures in velocity/awake paths now deterministically rebuild runtime bodies instead of leaving stale teardown-only state,
  - dynamic-body creation/rebuild now enforces awake intent as final runtime state after velocity policy application.
- Added parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp`:
  - awake API behavior checks (roundtrip + invalid/unknown/destroyed/static rejection),
  - scene contract classifier checks for awake reconcile behavior,
  - ECS awake toggle convergence checks with no body-id churn on runtime success path,
  - deterministic rebuild/recovery coverage for forced runtime mutation failure path.

Explicit remaining deferrals after Phase 4k:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4l Runtime Force/Impulse Mutation Parity Slice (2026-02-18)
Landed in this bounded slice:
- Extended scene rigidbody contracts in `include/karma/scene/physics_components.hpp`:
  - added runtime command intent fields on `RigidBodyIntentComponent` (`linear_force`, `linear_impulse`),
  - clarified bounded command semantics in contract comments:
    - force is applied each pre-sim while non-zero,
    - impulse is one-shot and consumed only after successful runtime application,
  - extended rigidbody intent validation to reject non-finite force/impulse command vectors.
- Extended backend-neutral substrate contracts:
  - `include/karma/physics/backend.hpp`:
    - added runtime command APIs `addBodyForce` + `addBodyLinearImpulse`,
    - defined deterministic invalid/unknown/non-dynamic/ineligible rejection contract for command mutation calls.
  - `include/karma/physics/physics_system.hpp` + `src/engine/physics/physics_system.cpp`:
    - passthrough methods for force/impulse runtime command APIs landed.
- Backend implementation details:
  - `src/engine/physics/backends/backend_jolt_stub.cpp`:
    - runtime force/impulse command support added for dynamic non-kinematic bodies,
    - deterministic `false` returned for invalid/unknown/non-dynamic/ineligible requests,
    - zero-vector commands treated as deterministic bounded no-op success.
  - `src/engine/physics/backends/backend_physx_stub.cpp`:
    - runtime force (`PxForceMode::eFORCE`) and impulse (`PxForceMode::eIMPULSE`) support added for dynamic non-kinematic bodies,
    - deterministic `false` returned for invalid/unknown/non-dynamic/ineligible requests,
    - zero-vector commands treated as deterministic bounded no-op success.
- Updated ECS sync in `src/engine/physics/ecs_sync_system.cpp`:
  - pre-sim runtime command application helper added and invoked in both create/update paths,
  - force commands are applied each pre-sim when non-zero,
  - impulse commands are consumed only after successful runtime application,
  - runtime command failure follows deterministic fallback behavior (teardown/rebuild/recovery path),
  - impulse command remains pending when runtime command application fails.
- Added parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp`:
  - new `RunBodyForceImpulseApiChecks` coverage:
    - dynamic force/impulse success path,
    - static/invalid/unknown/destroyed rejection.
  - expanded invalid/uninitialized API checks to cover force/impulse APIs.
  - expanded scene contract checks to cover non-finite force/impulse intent rejection.
  - expanded ECS sync checks for:
    - force application path,
    - one-shot impulse consume-on-success behavior,
    - deterministic failure path with no-consume-on-failure + stable rejection/recovery behavior.

Explicit remaining deferrals after Phase 4l:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4m Runtime Torque/Angular-Impulse Mutation Parity Slice (2026-02-18)
Landed in this bounded slice:
- Extended scene rigidbody contracts in `include/karma/scene/physics_components.hpp`:
  - added runtime command intent fields on `RigidBodyIntentComponent` (`angular_torque`, `angular_impulse`),
  - extended rigidbody intent validation to reject non-finite torque/angular-impulse command vectors,
  - added bounded command helper predicates (`HasRuntimeAngularTorqueCommand`, `HasRuntimeAngularImpulseCommand`).
- Extended backend-neutral substrate contracts:
  - `include/karma/physics/backend.hpp`:
    - added runtime command APIs `addBodyTorque` + `addBodyAngularImpulse`,
    - maintained deterministic invalid/unknown/non-dynamic/ineligible rejection contract for runtime command mutation calls.
  - `include/karma/physics/physics_system.hpp` + `src/engine/physics/physics_system.cpp`:
    - passthrough methods for torque/angular-impulse runtime command APIs landed.
- Backend implementation details:
  - `src/engine/physics/backends/backend_jolt_stub.cpp`:
    - runtime torque/angular-impulse command support added for dynamic non-kinematic bodies,
    - deterministic `false` returned for invalid/unknown/non-dynamic/ineligible requests,
    - zero-vector commands treated as deterministic bounded no-op success.
  - `src/engine/physics/backends/backend_physx_stub.cpp`:
    - runtime torque (`PxForceMode::eFORCE`) and angular impulse (`PxForceMode::eIMPULSE`) support added for dynamic non-kinematic bodies,
    - deterministic `false` returned for invalid/unknown/non-dynamic/ineligible requests,
    - zero-vector commands treated as deterministic bounded no-op success.
- Updated ECS sync in `src/engine/physics/ecs_sync_system.cpp`:
  - pre-sim runtime command helper now applies torque each pre-sim when non-zero,
  - angular-impulse commands are one-shot and consumed only after successful runtime application,
  - runtime command failure follows deterministic fallback behavior (teardown/rebuild/recovery path),
  - one-shot angular-impulse command remains pending when runtime command application fails.
- Added parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp`:
  - `RunBodyForceImpulseApiChecks` now covers torque/angular-impulse success and static/invalid/unknown/destroyed rejection,
  - invalid/uninitialized API checks expanded to include torque/angular-impulse APIs,
  - scene contract checks expanded for non-finite torque/angular-impulse intent rejection,
  - ECS sync checks expanded for torque application path, one-shot angular-impulse consume-on-success, and deterministic no-consume-on-failure fallback behavior.

Explicit remaining deferrals after Phase 4m:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4n Runtime Command Lifecycle Controls Slice (2026-02-18)
Landed in this bounded slice:
- Extended scene rigidbody command lifecycle contract in `include/karma/scene/physics_components.hpp`:
  - added persistent command enable flags (`linear_force_enabled`, `angular_torque_enabled`),
  - added explicit command clear/reset request (`clear_runtime_commands_requested`),
  - added lifecycle helpers (`HasRuntimeCommandClearRequest`, `ClearRuntimeCommandIntents`),
  - force/torque command detection helpers now honor persistent enable flags.
- Updated ECS command reconciliation in `src/engine/physics/ecs_sync_system.cpp`:
  - clear/reset request is reconciled first, clears force/torque/impulse intents, and consumes clear request on successful reconciliation,
  - command intents on ineligible bodies (static or kinematic) are now deterministically preserved without teardown/rebuild churn,
  - existing eligible-body runtime failure policy is preserved for real backend API failures (runtime call failure still returns failure for deterministic fallback path).
- Extended parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp`:
  - scene contract checks now cover persistent enable-gating behavior and clear helper reset semantics,
  - ECS checks now cover clear/reset consume-on-success behavior and ineligible command stability without body-id churn,
  - existing eligible force/torque + one-shot impulse/angular-impulse behavior remains covered and passing.

Explicit remaining deferrals after Phase 4n:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4o Runtime Command Failure-Path Coverage Slice (2026-02-18)
Landed in this bounded slice:
- Extended parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` with focused runtime-command failure-path fixtures:
  - forced runtime command-apply failure via backend runtime-state desync (temporary backend kinematic mutation on ECS-owned dynamic intent),
  - asserted one-shot command recovery semantics (`linear_impulse`, `angular_impulse`) with deterministic consume behavior after successful recovery,
  - asserted persistent command stability (`linear_force`, `angular_torque`) across repeated induced failure cycles,
  - asserted deterministic clear/reset reconciliation with stable no-churn runtime convergence.
- Kept slice bounded to parity-tests/docs ownership:
  - no new substrate API surface,
  - no backend feature expansion,
  - no telemetry/trace feature work.

Explicit remaining deferrals after Phase 4o:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4p Runtime Command Failure Classification Matrix Coverage Slice (2026-02-18)
Landed in this bounded slice:
- Extended parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` with an explicit stale-runtime vs ineligible-state command-failure matrix for:
  - `linear_force`,
  - `linear_impulse`,
  - `angular_torque`,
  - `angular_impulse`.
- Added explicit non-dynamic ineligible-state ECS coverage:
  - runtime command APIs reject deterministically,
  - one-shot commands remain pending while ineligible,
  - persistent commands remain stable until explicit clear/reset,
  - deterministic recovery convergence is asserted once body intent returns to eligible dynamic state.
- Tightened kinematic ineligible-state coverage with explicit runtime API rejection assertions for all four command paths.
- Added stale-runtime (destroyed runtime body / stale binding id) coverage:
  - API-level command apply fails deterministically for stale ids,
  - ECS pre-sim fallback rebuild path preserves pending command intents while failure conditions persist,
  - clear/reset and recovery convergence remain deterministic once state is valid again.
- Kept slice bounded to parity-tests/docs ownership:
  - no backend API contract expansion,
  - no gameplay semantics work,
  - no lock ownership changes.

Explicit remaining deferrals after Phase 4p:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4q Runtime Command Failure Observability Tags Slice (2026-02-18)
Landed in this bounded slice:
- Added explicit runtime-command outcome classification + tag mapping helpers in `src/engine/physics/ecs_sync_system.cpp` for `physics.system` trace emission:
  - `stale_runtime_binding_body`,
  - `ineligible_non_dynamic`,
  - `ineligible_kinematic`,
  - `runtime_apply_failed`,
  - `recovery_applied`.
- Added deterministic trace emission at ECS pre-sim command decision points:
  - command-intent ineligible state classification (non-dynamic vs kinematic),
  - stale runtime-body/binding failure detection on runtime apply failure,
  - runtime apply generic failure classification,
  - recovery classification when fallback rebuild converges after a prior command failure path.
- Added bounded parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` for classification/tag mapping behavior used by traces (no log-capture harness).
- Kept slice bounded:
  - no substrate/backend API additions,
  - no gameplay semantics changes,
  - no command-lifecycle behavior changes.

Explicit remaining deferrals after Phase 4q:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4r Runtime Command Operation+Outcome Trace Tags Slice (2026-02-18)
Landed in this bounded slice:
- Extended runtime-command trace tagging in `src/engine/physics/ecs_sync_system.cpp` with deterministic per-command operation labels:
  - `linear_force`,
  - `linear_impulse`,
  - `angular_torque`,
  - `angular_impulse`.
- Updated `physics.system` ECS pre-sim trace emission to include both operation + outcome classification at command decision points.
- Preserved existing runtime-command semantics:
  - no command lifecycle/consume/fallback behavior changes,
  - no substrate/backend/API expansion.
- Added bounded helper coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` for:
  - operation-label deterministic priority mapping,
  - outcome-tag mapping,
  - combined operation/outcome helper expectations (without log-capture harness).

Explicit remaining deferrals after Phase 4r:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4s Runtime Command Stage+Operation+Outcome Trace Tags Slice (2026-02-18)
Landed in this bounded slice:
- Extended runtime-command trace helper surface in `src/engine/physics/ecs_sync_system.cpp` with deterministic stage classification/tag mapping:
  - `create`,
  - `update`,
  - `recovery`.
- Updated `physics.system` runtime-command trace emission to include full metadata tuple:
  - stage + operation + outcome.
- Bound stage emission to existing ECS pre-sim command decision points:
  - create path (`create`),
  - existing-binding command reconciliation path (`update`),
  - fallback/rebuild convergence path (`recovery`).
- Added bounded helper coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` for:
  - stage classification/tag mapping,
  - operation mapping,
  - outcome mapping,
  - combined stage+operation+outcome expectations.
- Kept slice behavior-neutral:
  - no command lifecycle, fallback, or consume-rule changes,
  - no substrate/backend/API expansion.

Explicit remaining deferrals after Phase 4s:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
- No engine loop integration in `src/engine/app/*`.
- No gameplay migration wiring.

## Phase 4t Runtime Command Failure-Cause Trace Sub-Tag Slice (2026-02-18)
Landed in this bounded slice:
- Extended runtime-command trace helper surface in `src/engine/physics/ecs_sync_system.cpp` with deterministic failure-cause sub-tag mapping:
  - `stale_binding`,
  - `backend_reject`.
- Trace emission now includes failure cause only for runtime command failure outcomes:
  - `stale_runtime_binding_body`,
  - `runtime_apply_failed`.
- Kept command runtime behavior unchanged:
  - no substrate/backend API changes,
  - no command lifecycle/consume/fallback policy changes.
- Added bounded parity coverage in `src/engine/physics/tests/physics_backend_parity_test.cpp` for:
  - failure-cause helper mapping/tag priority behavior,
  - stale-binding fixture classification path,
  - backend-reject fixture classification path.

Explicit remaining deferrals after Phase 4t:
- No backend-native player-controller runtime object.
- No grounded/controller movement gameplay semantics in engine physics contracts.
- No real static-mesh geometry ingestion pipeline.
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
- `2026-02-18`: Phase 4b controller-ingestion depth slice landed:
  - backend-neutral runtime linear/angular velocity APIs added to substrate and `PhysicsSystem`,
  - Jolt/PhysX velocity runtime support landed with deterministic invalid/unknown/static rejection semantics,
  - ECS sync now applies controller desired velocity to runtime bodies for compatible enabled controllers and pushes zero effect when disabled.
- `2026-02-18`: Phase 4b validation completed in `build-a3`:
  - `./abuild.py --lock-status -d build-a3` (owner verified)
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
- `2026-02-18`: Phase 4c controller-runtime parity slice landed:
  - controller velocity ownership policy helpers added to scene contracts,
  - ECS sync now deterministically applies controller-vs-rigidbody velocity ownership (linear + angular) in both create/update paths,
  - enabled-controller/non-dynamic-rigidbody combinations are now policy-incompatible without runtime churn.
- `2026-02-18`: Phase 4c validation completed in `build-a3`:
  - `if [ -x ./vcpkg/vcpkg ] || [ -x ./vcpkg/bootstrap-vcpkg.sh ] || [ -f ./vcpkg/.bootstrap-complete ]; then echo VCPKG_BOOTSTRAPPED; else echo VCPKG_MISSING; fi` (`VCPKG_BOOTSTRAPPED`)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4d controller-geometry reconcile slice landed:
  - controller-geometry reconcile policy helper added (`NoOp`/`RebuildRuntimeShape`/`RejectInvalidIntent`),
  - ECS sync now rebuilds deterministically on controller geometry mutation and derives runtime Box/Capsule geometry from controller dimensions for enabled compatible controllers,
  - runtime transform and runtime linear/angular velocity are preserved across controller-geometry-triggered rebuilds.
- `2026-02-18`: Phase 4d validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4e controller-motion parity slice landed:
  - ECS sync controller-owned velocity updates now preserve runtime `y`, apply `desired_velocity.xz`, and keep angular velocity zero,
  - one-shot upward jump intent is applied and consumed deterministically only on successful runtime writes,
  - disabled/incompatible controller states keep rigidbody-owned runtime velocity and do not consume jump intent.
- `2026-02-18`: Phase 4e validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4e boundary correction landed:
  - removed `jump_requested` and jump-specific helper APIs from engine scene-physics contracts,
  - removed jump consume/mutate behavior from ECS sync,
  - parity checks now cover engine-generic controller/rigidbody velocity ownership without jump semantics.
- `2026-02-18`: Phase 4e boundary-correction validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4f collider local-offset intake slice landed:
  - substrate shape descriptor now carries backend-neutral collider local-center offset,
  - Jolt/PhysX runtime shape creation now consumes local-center for Box/Sphere/Capsule,
  - ECS controller-center runtime mapping now uses shape local offset instead of placeholder extents expansion,
  - parity checks now cover backend offset query behavior and ECS controller-center offset rebuild behavior.
- `2026-02-18`: Phase 4f validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4g runtime damping mutation parity slice landed:
  - scene rigidbody intent now carries validated linear/angular damping,
  - substrate/backends now expose runtime damping set/get APIs with dynamic-body deterministic rejection semantics,
  - ECS sync now applies damping on create and valid mutation updates without runtime body-id churn.
- `2026-02-18`: Phase 4g validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4h runtime motion-lock mutation parity slice implemented:
  - substrate/backends/`PhysicsSystem` runtime lock APIs added,
  - ECS sync lock-mutation runtime-first + deterministic rebuild fallback path landed,
  - parity tests extended for motion-lock API and ECS fallback behavior.
- `2026-02-18`: Phase 4h blocker-fix follow-up landed:
  - dynamic both-lock motion intent is now explicitly invalid (`ConflictingMotionLocks`) for dynamic rigidbodies,
  - Jolt/PhysX now reject dynamic both-lock create/mutation paths deterministically (prevents Jolt SIGTRAP assert),
  - ECS parity coverage now asserts deterministic invalid-intent teardown and stable reject/recovery behavior.
- `2026-02-18`: Phase 4h validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4i runtime material-property mutation parity slice landed:
  - collider material intent (`friction`/`restitution`) is now validated and reconciled as runtime properties,
  - substrate + `PhysicsSystem` material create/runtime APIs landed,
  - PhysX runtime material mutation is native per-body; Jolt restitution runtime mutation reports unsupported deterministically for ECS fallback coverage.
- `2026-02-18`: Phase 4i validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4j runtime kinematic-intent mutation parity slice landed:
  - scene rigidbody kinematic intent + validation/classification helpers landed,
  - substrate/backends now expose runtime kinematic set/get APIs with deterministic invalid/non-dynamic rejection,
  - ECS sync now applies runtime kinematic mutation first with deterministic rebuild fallback.
- `2026-02-18`: Phase 4j validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4k runtime sleep/wake mutation parity slice landed:
  - scene rigidbody awake intent + reconcile classifier landed,
  - substrate/backends now expose runtime awake set/get APIs with deterministic invalid/non-dynamic rejection,
  - ECS sync now applies awake mutation runtime-first and uses deterministic rebuild fallback for velocity/awake runtime mutation failures.
- `2026-02-18`: Phase 4k validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4l runtime force/impulse mutation parity slice landed:
  - scene rigidbody runtime command intent fields + finite-value validation landed,
  - substrate/backends now expose runtime force/impulse command APIs with deterministic invalid/non-dynamic/ineligible rejection,
  - ECS sync now applies force per pre-sim, consumes impulse on successful apply, and preserves impulse on runtime command failure.
- `2026-02-18`: Phase 4l validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4m runtime torque/angular-impulse mutation parity slice landed:
  - scene rigidbody runtime command intent now includes torque/angular-impulse with finite-value validation,
  - substrate/backends now expose runtime torque/angular-impulse command APIs with deterministic invalid/non-dynamic/ineligible rejection,
  - ECS sync now applies torque per pre-sim, consumes angular impulse on successful apply, and preserves angular impulse on runtime command failure.
- `2026-02-18`: Phase 4m validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4n runtime command lifecycle controls slice landed:
  - scene rigidbody intent now carries persistent force/torque enable flags and explicit clear/reset request,
  - ECS sync now reconciles clear requests deterministically and preserves ineligible command state without teardown churn,
  - parity coverage now asserts clear/reset consume-on-success and ineligible state stability while keeping eligible command behavior green.
- `2026-02-18`: Phase 4n validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4o runtime-command failure-path coverage slice landed:
  - parity fixtures now force runtime command-apply failure conditions and validate deterministic recovery convergence,
  - one-shot command recovery behavior and persistent-command repeated-failure stability are asserted per backend,
  - clear/reset reconciliation behavior remains deterministic under repeated pre-sim checks.
- `2026-02-18`: Phase 4o validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4p runtime-command failure classification matrix slice landed:
  - parity coverage now explicitly separates stale-runtime-id failures from ineligible non-dynamic and ineligible kinematic command-apply failures,
  - one-shot no-consume-on-failure, persistent-command stability, and deterministic clear/recovery convergence are asserted across matrix cases.
- `2026-02-18`: Phase 4p validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4q runtime-command failure observability tag slice landed:
  - ECS pre-sim now emits deterministic `physics.system` runtime-command outcome tags for stale-runtime/ineligible-state/apply-failure/recovery paths,
  - parity tests now include direct classification/tag mapping checks for the trace helper path (no log capture).
- `2026-02-18`: Phase 4q validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4r runtime-command operation+outcome trace tag slice landed:
  - ECS pre-sim `physics.system` traces now include deterministic operation labels plus existing outcome classification tags.
  - Helper coverage now asserts operation-label mapping and combined operation/outcome tagging expectations.
- `2026-02-18`: Phase 4r validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4s runtime-command stage+operation+outcome trace tag slice landed:
  - ECS pre-sim `physics.system` traces now include deterministic command-apply stage tags in addition to operation+outcome tags.
  - helper coverage now validates stage mapping and combined stage/operation/outcome tagging logic.
- `2026-02-18`: Phase 4s validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- `2026-02-18`: Phase 4t runtime-command failure-cause trace sub-tag slice landed:
  - ECS pre-sim `physics.system` runtime-command traces now include deterministic failure-cause tags for failure outcomes (`stale_binding` vs `backend_reject`) in addition to stage/operation/outcome tags.
  - Helper and fixture-level parity coverage now validates both cause classifications deterministically without log-capture harness.
- `2026-02-18`: Phase 4t validation completed in `build-a3`:
  - `./abuild.py -c --test-physics -d build-a3 -b jolt,physx` (pass)
  - `./scripts/test-engine-backends.sh build-a3` (pass)
  - `./docs/scripts/lint-project-docs.sh` (pass)
  - `./abuild.py --lock-status -d build-a3` (owner verified)
- Next implementation slice: execute a bounded Phase 4 follow-up to deepen runtime-command observability coverage while preserving current API and runtime behavior.

## Handoff Checklist
- [x] `physics-refactor.md` remains the single active physics project doc in `docs/projects/`.
- [x] `ASSIGNMENTS.md` row is updated in same handoff.
- [x] Retired project docs are moved under `docs/archive/`.
- [x] Validation commands/results are recorded for implementation slices.
- [x] Contract decisions and behavior changes are reflected in docs as they land.
