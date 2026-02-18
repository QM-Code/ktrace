# Core Engine Contracts

This is the canonical long-lived architecture contract for core engine sequencing and layered API boundaries.

Execution coordination guidance for this contract lives in
`docs/foundation/architecture/engine-defaults-program.md`.

## Scope
- Scheduler/system orchestration contracts.
- Component catalog contracts.
- Physics and audio layered contract boundaries.
- Staged implementation order and closeout evidence expectations.

## Architectural Guardrails
- Engine owns lifecycle and loop timing.
- Game submits content/logic through engine APIs.
- Engine code in `m-rewrite/src/engine/` remains game-agnostic.
- Runtime rewrite data/config lives at `m-rewrite/data/`.
- Backend-specific complexity stays behind engine interfaces, not in game code.
- Feature parity targets are capability/behavior targets, not source-layout targets.
- Platform/render/physics/audio backend APIs are engine-internal; game code must not depend on backend implementations.
- UI backend remains the intentional exception: game teams may explicitly build against chosen `imgui` or `rmlui` frontend.

---

## Stable-First Contract Implementation Order (Scheduler/Component/Physics/Audio)

Use this sequence when implementing the contract slices already codified in sections `0`, `0A`, `0B`, and `0C`.

### Stage 1: Scheduler Core (`0`)
Entry criteria:
- system scheduling contract spec is accepted as written.

Implementation focus:
- phase enum enforcement,
- deterministic registration/dependency resolution,
- cycle-fail startup behavior,
- additive vs replace-default boundary enforcement.

Exit criteria:
- all scheduler contract unit/integration tests listed in section `0` are implemented and passing.

### Stage 2: Component Catalog Core (`0A`)
Entry criteria:
- Stage 1 complete; scheduler phase ownership semantics are available.

Implementation focus:
- canonical descriptor schema registration and validation,
- ownership/source-of-truth enforcement,
- serialization mode/version-policy enforcement,
- replacement/extension boundary enforcement.

Exit criteria:
- all component catalog contract unit/integration tests listed in section `0A` are implemented and passing.

### Stage 3: Physics Core Contract (`0B`, core only)
Entry criteria:
- Stage 1 complete.
- Stage 2 complete for minimum metadata required by physics-bound components.

Implementation focus:
- `BodyId` lifecycle/validity guarantees,
- core state/query operations and deterministic failure behavior,
- backend-neutral diagnostics mapping.

Exit criteria:
- core subset of section `0B` tests passing:
  - `physics_body_id_lifecycle_contract_test`
  - `physics_core_state_query_contract_test`
  - `physics_invalid_input_failure_contract_test`
  - parity assertions for `BodyId` lifecycle/failure semantics across compiled physics backends.

### Stage 4: Physics Default Facade + Explicit Override Boundary (`0B`, facade/advanced)
Entry criteria:
- Stage 3 complete.

Implementation focus:
- bounded 95%-path facade helpers over core physics contract,
- explicit advanced/override boundary and capability gating.

Exit criteria:
- remaining section `0B` tests passing:
  - `physics_facade_boundary_contract_test`
  - `physics_override_boundary_contract_test`
  - `engine_fixed_step_physics_contract_smoke_test`.

### Stage 5: Audio Core Contract (`0C`, core only)
Entry criteria:
- Stage 1 complete.
- Stage 2 complete for minimum metadata required by audio-bound components.

Implementation focus:
- deterministic request/voice lifecycle and `VoiceId` validity semantics,
- listener update/sanitization behavior,
- deterministic invalid-input and stale-id failure mapping.

Exit criteria:
- core subset of section `0C` tests passing:
  - `audio_request_validation_contract_test`
  - `audio_voice_id_lifecycle_contract_test`
  - `audio_listener_sanitization_contract_test`
  - parity assertions for deterministic request rejection + `VoiceId` lifecycle equivalence across compiled audio backends.

### Stage 6: Audio Default Facade + Explicit Override Boundary (`0C`, facade/advanced)
Entry criteria:
- Stage 5 complete.

Implementation focus:
- bounded 95%-path facade helpers (event/bus/default routing),
- explicit advanced/override boundary and capability gating,
- headless/server policy lock-in.

Exit criteria:
- remaining section `0C` tests passing:
  - `audio_facade_boundary_contract_test`
  - `audio_override_boundary_contract_test`
  - `engine_audio_loop_contract_smoke_test`
  - `engine_headless_audio_policy_smoke_test`.

### Wrapper-Gate Rule (all stages)
- For any stage that changes physics/audio behavior or tests, close with:
  - `./scripts/test-engine-backends.sh <build-dir>` on touched assigned build dirs.
- Do not mark a stage complete until wrapper-gate expectations and relevant contract tests pass together.

### Parallelization Rule
- Scheduler and component stages are sequential by dependency.
- Physics and audio stages may run in parallel only after Stages 1 and 2 are complete.
- Facade/override stages must not start before their corresponding core stage exits.

---

## Consolidated Stages 1-6 Implementation Handoff Checklist

Use this table for implementation handoff closeout after each stage execution. Each stage owner must provide all listed evidence before marking the stage complete.

| Stage | Execution owner (role) | Required closeout evidence |
|---|---|---|
| `Stage 1` Scheduler Core (`0`) | `scheduler-core owner` (engine lifecycle/contracts implementer) | `scheduler_phase_order_contract_test`, `scheduler_registration_validation_test`, `scheduler_dependency_order_test`, `scheduler_cycle_fail_test`, `scheduler_override_boundary_test`, `engine_loop_scheduler_smoke_test` all passing; startup cycle-fail diagnostic sample captured; explicit note that no physics/audio semantics were changed. |
| `Stage 2` Component Catalog Core (`0A`) | `component-catalog owner` (engine data/contracts implementer) | `component_descriptor_schema_validation_test`, `component_descriptor_version_policy_test`, `component_ownership_mutation_guard_test`, `component_override_boundary_test`, `component_serialization_mode_contract_test`, `engine_component_catalog_bootstrap_smoke_test`, `engine_component_save_load_version_smoke_test` all passing; descriptor validation failure sample captured; explicit note that no scheduler/physics/audio semantics were changed. |
| `Stage 3` Physics Core (`0B` core) | `physics-core owner` (physics backend/contracts implementer) | `physics_body_id_lifecycle_contract_test`, `physics_core_state_query_contract_test`, `physics_invalid_input_failure_contract_test`, `physics_backend_parity_jolt`, `physics_backend_parity_physx` all passing; wrapper closeout `./scripts/test-engine-backends.sh <build-dir>` for touched assigned physics build dirs; explicit note that no scheduler/component/audio semantics and no physics-facade semantics were changed. |
| `Stage 4` Physics Facade/Override (`0B` facade/advanced) | `physics-facade owner` (physics ergonomics/advanced-path implementer) | `physics_facade_boundary_contract_test`, `physics_override_boundary_contract_test`, `engine_fixed_step_physics_contract_smoke_test` plus Stage-3 regression tests all passing; wrapper closeout `./scripts/test-engine-backends.sh <build-dir>` for touched assigned physics build dirs; explicit note that no scheduler/component/audio semantics and no unintended Stage-3 core drift occurred. |
| `Stage 5` Audio Core (`0C` core) | `audio-core owner` (audio backend/contracts implementer) | `audio_request_validation_contract_test`, `audio_voice_id_lifecycle_contract_test`, `audio_listener_sanitization_contract_test`, `audio_backend_smoke_sdl3audio`, `audio_backend_smoke_miniaudio` all passing; wrapper closeout `./scripts/test-engine-backends.sh <build-dir>` for touched assigned audio build dirs; explicit note that no scheduler/component/physics semantics and no audio-facade semantics were changed. |
| `Stage 6` Audio Facade/Override (`0C` facade/advanced) | `audio-facade owner` (audio ergonomics/advanced-path implementer) | `audio_facade_boundary_contract_test`, `audio_override_boundary_contract_test`, `engine_audio_loop_contract_smoke_test`, `engine_headless_audio_policy_smoke_test` plus Stage-5 regression tests all passing; wrapper closeout `./scripts/test-engine-backends.sh <build-dir>` for touched assigned audio build dirs; explicit note that no scheduler/component/physics semantics and no unintended Stage-5 core drift occurred. |

### Handoff Evidence Format (required)
Each stage handoff must include:
- execution owner id/role,
- files touched,
- exact test commands run + pass/fail result summary,
- wrapper command(s) run + exact result (or explicit `not applicable` reason),
- scope-exception declaration if any cross-surface touch occurred.

---

## Stage-1 Scheduler Implementation Kickoff Packet (`0`)

Use this packet when starting the first scheduler implementation slice. It is scoped to section `0` only.

### Entry Checklist
- [ ] Confirm section `0` is the implementation source-of-truth for this slice.
- [ ] Confirm stage scope is limited to scheduler core:
  - phase enum enforcement,
  - registration schema enforcement,
  - dependency ordering and cycle-fail behavior,
  - additive/replace-default boundary enforcement.
- [ ] Confirm no planned changes to physics/audio contract surfaces (`0B`/`0C`) in this slice.
- [ ] Confirm test plan includes all scheduler-first required tests before any optional refactors.

### Required Tests To Run First (Concrete)
Run these tests first as the scheduler core bring-up gate:
- `scheduler_phase_order_contract_test`
- `scheduler_registration_validation_test`
- `scheduler_dependency_order_test`
- `scheduler_cycle_fail_test`
- `scheduler_override_boundary_test`
- `engine_loop_scheduler_smoke_test`

Execution rule:
- Start with the five scheduler unit tests, then run the scheduler integration smoke test.
- Do not treat the slice as implementation-complete if only smoke test passes while unit tests are failing.

### Exit Criteria Checklist
- [ ] Phase enum ordering is enforced and immutable at runtime.
- [ ] Registration validation (duplicate ids, unknown targets, self-dependency) is enforced at startup.
- [ ] Deterministic ordering rules (hard dependencies + tie-break semantics) are enforced.
- [ ] Cycle detection fails before first simulation frame with actionable diagnostics.
- [ ] Additive vs replace-default boundary behavior matches section `0`.
- [ ] All required scheduler tests listed above are passing.
- [ ] No new physics/audio behavior changes were introduced in this scheduler slice.

### Wrapper Closeout Reminders
- Scheduler-only changes do not automatically require backend wrapper closeout.
- If this slice touches shared backend test wiring, physics/audio-adjacent test registration, or causes backend wrapper impact, run:
  - `./scripts/test-engine-backends.sh <build-dir>` on touched assigned build dirs.
- If no backend wrapper-impacting surfaces are touched, explicitly note in handoff: `backend wrapper closeout not applicable (scheduler-only scope)`.

### Scope Boundaries To Prevent Physics/Audio Creep
Do not include any of the following before Stage-1 exits:
- physics `BodyId` lifecycle/query/validation changes (`0B` scope),
- physics facade or advanced capability changes (`0B` facade/override scope),
- audio request/voice lifecycle, listener, or validation changes (`0C` scope),
- audio facade/event/bus/advanced capability changes (`0C` facade/override scope),
- backend-specific behavior tuning in `jolt|physx|sdl3audio|miniaudio`.

Allowed interactions with physics/audio for this stage:
- compile-level compatibility fixes strictly required to unblock scheduler core tests,
- no semantic behavior changes in physics/audio contracts,
- any unavoidable cross-surface touch must be called out explicitly in handoff under `scope exception`.

---

## Stage-2 Component-Catalog Implementation Kickoff Packet (`0A`)

Use this packet when starting the first component-catalog implementation slice. It is scoped to section `0A` only.

### Entry Checklist
- [ ] Confirm section `0A` is the implementation source-of-truth for this slice.
- [ ] Confirm Stage-1 scheduler core exit criteria are already satisfied and stable.
- [ ] Confirm stage scope is limited to component-catalog core:
  - canonical descriptor schema registration/validation,
  - ownership and source-of-truth enforcement,
  - serialization mode + version-policy enforcement,
  - replacement/extension boundary enforcement.
- [ ] Confirm no planned semantic changes to scheduler (`0`) or physics/audio (`0B`/`0C`) in this slice.
- [ ] Confirm test plan includes all component-catalog required tests before optional refactors.

### Required Tests To Run First (Concrete)
Run these tests first as the component-catalog core bring-up gate:
- `component_descriptor_schema_validation_test`
- `component_descriptor_version_policy_test`
- `component_ownership_mutation_guard_test`
- `component_override_boundary_test`
- `component_serialization_mode_contract_test`
- `engine_component_catalog_bootstrap_smoke_test`
- `engine_component_save_load_version_smoke_test`

Execution rule:
- Start with the five component unit tests, then run the two component integration smoke tests.
- Do not treat the slice as implementation-complete if integration smoke tests pass while unit tests are failing.

### Exit Criteria Checklist
- [ ] Canonical descriptor schema and required-field validation are enforced.
- [ ] Ownership/source-of-truth and mutation-guard rules are enforced.
- [ ] Serialization mode behavior (`RuntimeOnly`, `Saveable`, `Replicated`) is enforced at contract entry points.
- [ ] Version compatibility policy and deterministic unsupported-version failures are enforced.
- [ ] Replacement/extension boundary rules match section `0A`.
- [ ] All required component-catalog tests listed above are passing.
- [ ] No scheduler semantic changes and no new physics/audio behavior changes were introduced in this slice.

### Wrapper Closeout Reminders
- Component-only changes do not automatically require backend wrapper closeout.
- If this slice touches backend test wiring, backend-facing catalog adapters, or causes physics/audio wrapper impact, run:
  - `./scripts/test-engine-backends.sh <build-dir>` on touched assigned build dirs.
- If no backend wrapper-impacting surfaces are touched, explicitly note in handoff: `backend wrapper closeout not applicable (component-catalog-only scope)`.

### Scope Boundaries To Prevent Scheduler/Physics/Audio Creep
Do not include any of the following before Stage-2 exits:
- scheduler phase/order/dependency/override semantic changes (`0` scope),
- physics `BodyId` lifecycle/query/validation/facade/advanced changes (`0B` scope),
- audio request/voice/listener/validation/facade/advanced changes (`0C` scope),
- backend-specific tuning in `jolt|physx|sdl3audio|miniaudio`.

Allowed interactions with scheduler/physics/audio for this stage:
- compile-level compatibility fixes strictly required to unblock component-catalog tests,
- no semantic contract changes in sections `0`, `0B`, or `0C`,
- any unavoidable cross-surface touch must be called out explicitly in handoff under `scope exception`.

---

## Stage-3 Physics-Core Implementation Kickoff Packet (`0B` Core)

Use this packet when starting the Stage-3 physics-core implementation slice. It is scoped to section `0B` core boundaries only.

### Entry Checklist
- [ ] Confirm section `0B` is the implementation source-of-truth for this slice.
- [ ] Confirm Stage-1 (scheduler core) and Stage-2 (component-catalog core) exit criteria are satisfied and stable.
- [ ] Confirm stage scope is limited to physics core contract boundaries:
  - `BodyId` lifecycle/validity,
  - core state/query operations,
  - deterministic failure/diagnostics mapping.
- [ ] Confirm no planned physics facade/advanced work (`0B` facade/override scope) in this slice.
- [ ] Confirm no planned semantic changes to scheduler (`0`), component-catalog (`0A`), or audio (`0C`) contracts.
- [ ] Confirm test plan includes required core-first physics tests before any optional refactors.

### Required Tests To Run First (Concrete)
Run these tests first as the Stage-3 physics-core bring-up gate:
- `physics_body_id_lifecycle_contract_test`
- `physics_core_state_query_contract_test`
- `physics_invalid_input_failure_contract_test`
- `physics_backend_parity_jolt`
- `physics_backend_parity_physx`

Execution rule:
- Start with the three physics core unit/contract tests, then run both backend parity tests.
- Do not treat the slice as implementation-complete if parity passes while any core contract test is failing.
- Defer facade/override/integration smoke tests (`physics_facade_boundary_contract_test`, `physics_override_boundary_contract_test`, `engine_fixed_step_physics_contract_smoke_test`) to Stage-4.

### Exit Criteria Checklist
- [ ] `BodyId` lifecycle/validity behavior matches section `0B` core contract rules.
- [ ] Core state/query operations and deterministic failure behavior are enforced.
- [ ] Core failure diagnostics (operation + reason category + `BodyId` when applicable) are emitted as specified.
- [ ] All required Stage-3 tests listed above are passing.
- [ ] No scheduler/component/audio semantic changes were introduced in this slice.
- [ ] No physics facade/advanced boundary changes were introduced in this slice.

### Wrapper Closeout Reminders
- Stage-3 changes are physics-affecting by definition; backend wrapper closeout is expected when implementation touches physics behavior/tests:
  - `./scripts/test-engine-backends.sh <build-dir>` on touched assigned physics build dirs.
- If a docs-only or test-plan-only update has no backend-behavior/test-registration impact, explicitly note in handoff:
  - `backend wrapper closeout not applicable (no physics behavior/test wiring changes)`.
- Do not mark Stage-3 implementation complete until required tests and applicable wrapper closeout both pass.

### Scope Boundaries To Prevent Scheduler/Component/Audio Creep
Do not include any of the following before Stage-3 exits:
- scheduler phase/order/dependency/override semantic changes (`0` scope),
- component descriptor schema/ownership/serialization/version-policy semantic changes (`0A` scope),
- audio request/voice/listener/validation/facade/advanced changes (`0C` scope),
- physics facade helpers or advanced override-capability work reserved for Stage-4.

Allowed interactions with scheduler/component/audio for this stage:
- compile-level compatibility fixes strictly required to unblock Stage-3 physics-core tests,
- no semantic contract changes in sections `0`, `0A`, or `0C`,
- any unavoidable cross-surface touch must be called out explicitly in handoff under `scope exception`.

---

## Stage-4 Physics-Facade/Override Implementation Kickoff Packet (`0B` Facade/Advanced)

Use this packet when starting the Stage-4 physics implementation slice. It is scoped to section `0B` facade/advanced boundaries only.

### Entry Checklist
- [ ] Confirm section `0B` is the implementation source-of-truth for this slice.
- [ ] Confirm Stage-3 physics-core exit criteria are satisfied and stable.
- [ ] Confirm stage scope is limited to physics facade/override boundaries:
  - bounded 95%-path facade helpers over core `PhysicsSystem`,
  - explicit advanced/override capability-gated paths.
- [ ] Confirm no planned semantic changes to scheduler (`0`), component-catalog (`0A`), or audio (`0C`) contracts.
- [ ] Confirm no planned semantic drift in physics core boundaries (`BodyId` lifecycle, core state/query/failure behavior) beyond compatibility fixes.
- [ ] Confirm test plan includes Stage-4 facade/override tests first, then required regression tests.

### Required Tests To Run First (Concrete)
Run these tests first as the Stage-4 facade/override bring-up gate:
- `physics_facade_boundary_contract_test`
- `physics_override_boundary_contract_test`
- `engine_fixed_step_physics_contract_smoke_test`

Then run mandatory regression checks before closeout:
- `physics_body_id_lifecycle_contract_test`
- `physics_core_state_query_contract_test`
- `physics_invalid_input_failure_contract_test`
- `physics_backend_parity_jolt`
- `physics_backend_parity_physx`

Execution rule:
- Start with the three Stage-4 tests, then run the five regression checks.
- Do not treat the slice as implementation-complete if facade/override tests pass while any regression check fails.

### Exit Criteria Checklist
- [ ] Facade helpers are bounded to common-path ergonomics and map to core `PhysicsSystem` calls.
- [ ] Advanced/override behavior is explicitly separated and capability-gated per section `0B`.
- [ ] No backend-specific options or handles leak through game-facing facade/advanced boundaries.
- [ ] All Stage-4 required tests and mandatory regression checks listed above are passing.
- [ ] No scheduler/component/audio semantic changes were introduced in this slice.
- [ ] No unintended physics-core semantic drift was introduced in this slice.

### Wrapper Closeout Reminders
- Stage-4 changes are physics-affecting; backend wrapper closeout is expected when implementation touches physics behavior/tests:
  - `./scripts/test-engine-backends.sh <build-dir>` on touched assigned physics build dirs.
- If a docs-only or test-plan-only update has no backend-behavior/test-registration impact, explicitly note in handoff:
  - `backend wrapper closeout not applicable (no physics behavior/test wiring changes)`.
- Do not mark Stage-4 implementation complete until required tests and applicable wrapper closeout both pass.

### Scope Boundaries To Prevent Scheduler/Component/Audio/Core-Physics Creep
Do not include any of the following before Stage-4 exits:
- scheduler phase/order/dependency/override semantic changes (`0` scope),
- component descriptor schema/ownership/serialization/version-policy semantic changes (`0A` scope),
- audio request/voice/listener/validation/facade/advanced changes (`0C` scope),
- physics core contract semantic changes reserved for Stage-3 (`BodyId` lifecycle, core state/query semantics, core failure mapping),
- backend-specific tuning unrelated to facade/override boundary behavior.

Allowed interactions with scheduler/component/audio/core-physics for this stage:
- compile-level compatibility fixes strictly required to unblock Stage-4 tests,
- no semantic contract changes in sections `0`, `0A`, `0C`, or Stage-3 core `0B` boundaries,
- any unavoidable cross-surface touch must be called out explicitly in handoff under `scope exception`.

---

## Stage-5 Audio-Core Implementation Kickoff Packet (`0C` Core)

Use this packet when starting the Stage-5 audio-core implementation slice. It is scoped to section `0C` core boundaries only.

### Entry Checklist
- [ ] Confirm section `0C` is the implementation source-of-truth for this slice.
- [ ] Confirm Stage-1 (scheduler core) and Stage-2 (component-catalog core) exit criteria are satisfied and stable.
- [ ] Confirm stage scope is limited to audio core contract boundaries:
  - deterministic request/voice lifecycle and `VoiceId` validity semantics,
  - listener update/sanitization behavior,
  - deterministic invalid-input/stale-id failure mapping.
- [ ] Confirm no planned audio facade/advanced work (`0C` facade/override scope) in this slice.
- [ ] Confirm no planned semantic changes to scheduler (`0`), component-catalog (`0A`), or physics (`0B`) contracts.
- [ ] Confirm test plan includes required core-first audio tests before optional refactors.

### Required Tests To Run First (Concrete)
Run these tests first as the Stage-5 audio-core bring-up gate:
- `audio_request_validation_contract_test`
- `audio_voice_id_lifecycle_contract_test`
- `audio_listener_sanitization_contract_test`
- `audio_backend_smoke_sdl3audio`
- `audio_backend_smoke_miniaudio`

Execution rule:
- Start with the three audio core unit/contract tests, then run both backend smoke/parity tests.
- Do not treat the slice as implementation-complete if backend smoke/parity tests pass while any core contract test is failing.
- Defer facade/override/integration-policy tests (`audio_facade_boundary_contract_test`, `audio_override_boundary_contract_test`, `engine_audio_loop_contract_smoke_test`, `engine_headless_audio_policy_smoke_test`) to Stage-6.

### Exit Criteria Checklist
- [ ] Core request/voice lifecycle and `VoiceId` validity behavior match section `0C` core contract rules.
- [ ] Listener update/sanitization behavior is enforced and deterministic.
- [ ] Core failure diagnostics (operation + reason category + `VoiceId` when applicable) are emitted as specified.
- [ ] All required Stage-5 tests listed above are passing.
- [ ] No scheduler/component/physics semantic changes were introduced in this slice.
- [ ] No audio facade/advanced boundary changes were introduced in this slice.

### Wrapper Closeout Reminders
- Stage-5 changes are audio-affecting by definition; backend wrapper closeout is expected when implementation touches audio behavior/tests:
  - `./scripts/test-engine-backends.sh <build-dir>` on touched assigned audio build dirs.
- If a docs-only or test-plan-only update has no backend-behavior/test-registration impact, explicitly note in handoff:
  - `backend wrapper closeout not applicable (no audio behavior/test wiring changes)`.
- Do not mark Stage-5 implementation complete until required tests and applicable wrapper closeout both pass.

### Scope Boundaries To Prevent Scheduler/Component/Physics Creep
Do not include any of the following before Stage-5 exits:
- scheduler phase/order/dependency/override semantic changes (`0` scope),
- component descriptor schema/ownership/serialization/version-policy semantic changes (`0A` scope),
- physics core semantic changes reserved for Stage-3 (`0B` core scope),
- physics facade/override semantic changes reserved for Stage-4 (`0B` facade/advanced scope),
- audio facade/event/bus/advanced capability changes reserved for Stage-6 (`0C` facade/override scope),
- backend-specific tuning unrelated to audio-core contract behavior.

Allowed interactions with scheduler/component/physics for this stage:
- compile-level compatibility fixes strictly required to unblock Stage-5 audio-core tests,
- no semantic contract changes in sections `0`, `0A`, or `0B` (including Stage-3/Stage-4 physics scopes),
- any unavoidable cross-surface touch must be called out explicitly in handoff under `scope exception`.

---

## Stage-6 Audio-Facade/Override Implementation Kickoff Packet (`0C` Facade/Advanced)

Use this packet when starting the Stage-6 audio implementation slice. It is scoped to section `0C` facade/advanced boundaries only.

### Entry Checklist
- [ ] Confirm section `0C` is the implementation source-of-truth for this slice.
- [ ] Confirm Stage-5 audio-core exit criteria are satisfied and stable.
- [ ] Confirm stage scope is limited to audio facade/override boundaries:
  - bounded 95%-path facade helpers over core `AudioSystem`,
  - explicit advanced/override capability-gated paths.
- [ ] Confirm no planned semantic changes to scheduler (`0`), component-catalog (`0A`), or physics (`0B`) contracts.
- [ ] Confirm no planned semantic drift in Stage-5 audio-core boundaries (request/voice lifecycle, listener sanitization, core failure mapping) beyond compatibility fixes.
- [ ] Confirm test plan includes Stage-6 facade/override tests first, then required regression tests.

### Required Tests To Run First (Concrete)
Run these tests first as the Stage-6 facade/override bring-up gate:
- `audio_facade_boundary_contract_test`
- `audio_override_boundary_contract_test`
- `engine_audio_loop_contract_smoke_test`
- `engine_headless_audio_policy_smoke_test`

Then run mandatory regression checks before closeout:
- `audio_request_validation_contract_test`
- `audio_voice_id_lifecycle_contract_test`
- `audio_listener_sanitization_contract_test`
- `audio_backend_smoke_sdl3audio`
- `audio_backend_smoke_miniaudio`

Execution rule:
- Start with the four Stage-6 tests, then run the five regression checks.
- Do not treat the slice as implementation-complete if facade/override tests pass while any regression check fails.

### Exit Criteria Checklist
- [ ] Facade helpers are bounded to common-path ergonomics and map to core `AudioSystem` requests/voice primitives.
- [ ] Advanced/override behavior is explicitly separated and capability-gated per section `0C`.
- [ ] No backend-specific device/session handles leak through game-facing facade/advanced boundaries.
- [ ] All Stage-6 required tests and mandatory regression checks listed above are passing.
- [ ] No scheduler/component/physics semantic changes were introduced in this slice.
- [ ] No unintended Stage-5 audio-core semantic drift was introduced in this slice.

### Wrapper Closeout Reminders
- Stage-6 changes are audio-affecting; backend wrapper closeout is expected when implementation touches audio behavior/tests:
  - `./scripts/test-engine-backends.sh <build-dir>` on touched assigned audio build dirs.
- If a docs-only or test-plan-only update has no backend-behavior/test-registration impact, explicitly note in handoff:
  - `backend wrapper closeout not applicable (no audio behavior/test wiring changes)`.
- Do not mark Stage-6 implementation complete until required tests and applicable wrapper closeout both pass.

### Scope Boundaries To Prevent Scheduler/Component/Physics/Stage-5-Audio-Core Creep
Do not include any of the following before Stage-6 exits:
- scheduler phase/order/dependency/override semantic changes (`0` scope),
- component descriptor schema/ownership/serialization/version-policy semantic changes (`0A` scope),
- physics core semantic changes reserved for Stage-3 (`0B` core scope),
- physics facade/override semantic changes reserved for Stage-4 (`0B` facade/advanced scope),
- audio core semantic changes reserved for Stage-5 (`0C` core scope: request/voice lifecycle, listener sanitization, core failure mapping),
- backend-specific tuning unrelated to audio-facade/override boundary behavior.

Allowed interactions with scheduler/component/physics/stage-5-audio-core for this stage:
- compile-level compatibility fixes strictly required to unblock Stage-6 tests,
- no semantic contract changes in sections `0`, `0A`, `0B`, or Stage-5 core `0C` boundaries,
- any unavoidable cross-surface touch must be called out explicitly in handoff under `scope exception`.

---

## 0) Engine System Scheduling Contract (Implementation-Ready First Slice)

## Scope
Define the first engine-owned scheduler contract slice that can be implemented without changing gameplay semantics.

## Phase Enum (authoritative)
Engine systems execute in this fixed order:
1. `Bootstrap` (one-time startup only)
2. `PreFixed`
3. `Fixed`
4. `PostFixed`
5. `PreFrame`
6. `Frame`
7. `LateFrame`
8. `RenderPrepare`
9. `RenderSubmit`
10. `PostFrame`
11. `Shutdown` (one-time teardown only)

Contract rules:
- `Bootstrap` and `Shutdown` are lifecycle phases and run exactly once.
- All other phases are per-tick/per-frame phases under engine loop ownership.
- Phase ordering is immutable at runtime.

## System Registration Schema (first slice)
Each registered system descriptor must include:
- `system_id` (`string`, unique, stable)
- `phase` (`enum` from list above)
- `enabled_by_default` (`bool`, default `true`)
- `runs_when_paused` (`bool`, default `false`)
- `depends_on` (`list<string>`, hard dependencies, default empty)
- `before` (`list<string>`, soft ordering hints, default empty)
- `after` (`list<string>`, soft ordering hints, default empty)
- `override_mode` (`enum`: `Additive` or `ReplaceDefault`; default `Additive`)

Execution callbacks:
- `on_bootstrap` for `Bootstrap`
- `on_tick` for per-frame/per-step phases
- `on_shutdown` for `Shutdown`

First-slice restriction:
- all callbacks execute on main thread.
- no custom phase registration.
- no dynamic system registration/unregistration after `Bootstrap`.

## Dependency Rules (deterministic)
- `system_id` must be globally unique across engine and game registrations.
- `depends_on`, `before`, and `after` may only reference systems in the same phase.
- unknown dependency/hint targets are startup errors.
- self-dependency (`A -> A`) is a startup error.
- hard dependencies (`depends_on`) define required directed edges.
- soft hints (`before`/`after`) define optional ordering edges and are ignored only if they conflict with hard dependencies.
- final order inside a phase is computed via topological sort over hard edges + valid soft edges.
- deterministic tie-breaker is ascending lexical `system_id`.

## Cycle-Fail Behavior (mandatory)
- Any cycle in hard dependencies fails scheduler build before first simulation frame.
- Engine startup must fail-fast with clear diagnostics listing:
  - phase name,
  - involved system ids,
  - at least one detected cycle chain.
- When scheduler build fails, no partial system execution is allowed.

## Override Boundaries (95% defaults / 5% override)
Default-path behavior (95%):
- engine provides baseline default systems and ordering.
- game adds additive systems in existing phases.
- game does not redefine phase ordering.

Override behavior (5%, explicit):
- `ReplaceDefault` is allowed only for default systems explicitly marked replaceable by engine.
- replacement must preserve the replaced system's phase.
- replacement must declare `depends_on` and ordering hints explicitly; implicit inheritance is disallowed.
- replacing non-replaceable defaults is a startup error.

Non-goals for first slice:
- custom user-defined phases.
- backend-specific scheduler behavior.
- runtime hot-swap of system registration.

## Acceptance Criteria (first implementation slice)
- Scheduler supports full phase enum above with immutable ordering.
- Registration schema is enforced with startup validation for uniqueness/targets/self-edges.
- Per-phase deterministic ordering is reproducible across runs given same registrations.
- Hard-dependency cycle detection fails startup before first frame and produces actionable diagnostics.
- Additive registration works for game systems without engine phase ownership changes.
- Replace-default path enforces replaceable allowlist + same-phase constraint.

## Required Tests (first implementation slice)
Unit tests:
- `scheduler_phase_order_contract_test`: verifies fixed phase order and one-time lifecycle phases.
- `scheduler_registration_validation_test`: verifies duplicate id, unknown target, and self-dependency failures.
- `scheduler_dependency_order_test`: verifies hard-dependency topological ordering and lexical tie-break behavior.
- `scheduler_cycle_fail_test`: verifies cycle detection diagnostics and fail-before-first-frame semantics.
- `scheduler_override_boundary_test`: verifies additive success, replaceable-default success, and forbidden replacement failure.

Integration test:
- `engine_loop_scheduler_smoke_test`: verifies scheduler build + execution across bootstrap/tick/shutdown in real engine loop path.

Validation gate for this slice:
- integrate the above tests into existing engine test wrapper coverage used by CI (`./scripts/test-engine-backends.sh`) once implementation lands.

---

## 0A) Engine Component Catalog Contract (Implementation-Ready First Slice)

## Scope
Define the first engine-owned component catalog contract slice so entity/component composition is deterministic, backend-neutral, and implementation-ready.

## Canonical Metadata Schema (authoritative)
Each engine or game-registered component type must declare a `ComponentTypeDescriptor` with:
- `component_id` (`string`, unique, stable, immutable after first release)
- `display_name` (`string`, human-readable)
- `owner_domain` (`enum`: `EngineCore`, `EngineSubsystem`, `Game`)
- `storage_class` (`enum`: `DenseTable`, `SparseTable`, `Singleton`)
- `mutability_window` (`enum`: `BootstrapOnly`, `PhaseBound`, `AnyTick`)
- `phase_owner` (`SchedulerPhase`, required when `mutability_window = PhaseBound`)
- `serialization_mode` (`enum`: `RuntimeOnly`, `Saveable`, `Replicated`)
- `schema_version` (`uint32`, starts at `1`)
- `compatible_read_versions` (`list<uint32>`, default `[schema_version]`)
- `default_init_policy` (`enum`: `ZeroInit`, `ExplicitDefaults`, `FactoryRequired`)
- `factory_id` (`string`, optional, required when `default_init_policy = FactoryRequired`)
- `replaceability` (`enum`: `NotReplaceable`, `ReplaceableByGame`)
- `tags` (`list<string>`, optional, for tooling/query only; no runtime semantics)

## Ownership and Source-of-Truth Rules
- Engine-owned component descriptors are authoritative and versioned under engine contracts.
- Game-owned component descriptors are authoritative only for `owner_domain = Game`.
- Descriptor registration happens during `Bootstrap`; descriptor mutation after bootstrap is disallowed.
- Runtime component data is source-of-truth in engine-owned storage; backend state is derived/cached from component data and cannot be treated as authoritative.
- System ownership is explicit:
  - exactly one owning system may perform structural mutation (add/remove) for a component type.
  - non-owning systems may only perform value mutation within allowed mutability windows.
- Cross-system writes outside declared windows are contract violations and must fail validation in debug/test builds.

## Serialization and Versioning Policy
- `RuntimeOnly`: never persisted or replicated; excluded from save/network snapshots.
- `Saveable`: included in save/load path with schema-versioned decode.
- `Replicated`: included in network replication snapshots with deterministic field ordering.
- Versioning rules:
  - schema upgrades must be monotonic (`schema_version` increases).
  - readers may accept prior versions listed in `compatible_read_versions`.
  - unknown future versions fail with explicit unsupported-version diagnostics.
  - removing or renaming serialized fields requires a migration adapter or explicit incompatibility declaration.
- Default initialization policy is mandatory for every serializable field to avoid undefined load behavior.

## Validation and Failure Rules (mandatory)
Startup validation:
- duplicate `component_id` fails startup.
- missing required descriptor fields fail startup.
- `phase_owner` missing for `PhaseBound` mutability fails startup.
- `factory_id` missing when `FactoryRequired` fails startup.
- invalid `compatible_read_versions` set (empty or containing versions > `schema_version`) fails startup.

Runtime validation:
- structural mutation by non-owner system fails.
- value mutation outside `mutability_window` fails in debug/test builds and emits high-signal diagnostics.
- deserialization with incompatible/unsupported version fails deterministically (no partial apply).
- replicated component with non-deterministic field ordering is a contract violation and must fail test validation.

Failure diagnostics must include:
- `component_id`,
- owning/mutating system id (if applicable),
- rule violated,
- phase context when relevant.

## Override and Extension Boundaries (95% defaults / 5% override)
Default path (95%):
- games compose entities using engine-owned catalog components and defaults.
- games may tune behavior through data/config values, not by redefining engine component semantics.
- engine-owned components marked `NotReplaceable` are immutable contracts.

Extension path (5%, explicit):
- game may add new `owner_domain = Game` components with full descriptor metadata.
- game may replace only components explicitly marked `ReplaceableByGame`.
- replacement component must:
  - preserve the original component's `serialization_mode`,
  - preserve declared mutability/phase ownership constraints,
  - declare migration behavior for existing serialized data.
- replacing non-replaceable components or relaxing ownership/mutability constraints is a startup error.

Non-goals for first slice:
- runtime hot registration/unregistration of component descriptors.
- backend-specific component descriptor fields.
- implicit replacement by name collision.

## Acceptance Criteria (first implementation slice)
- Descriptor registration enforces canonical schema and required fields.
- Engine/game ownership boundaries and structural-mutation ownership are enforced.
- Serialization mode behavior (`RuntimeOnly`, `Saveable`, `Replicated`) is explicitly enforced by pipeline entry points.
- Version compatibility policy is enforced with deterministic pass/fail behavior.
- Extension and replacement boundaries are validated at startup with clear diagnostics.
- No backend API types are required in component descriptors.

## Required Tests (first implementation slice)
Unit tests:
- `component_descriptor_schema_validation_test`:
  - duplicate ids, missing fields, invalid factory/mutability metadata.
- `component_descriptor_version_policy_test`:
  - compatible read versions, unsupported future-version rejection, monotonic upgrade constraints.
- `component_ownership_mutation_guard_test`:
  - structural mutation owner enforcement and phase-window value mutation enforcement.
- `component_override_boundary_test`:
  - replaceable-vs-non-replaceable behavior and serialization/mutability constraint preservation checks.
- `component_serialization_mode_contract_test`:
  - runtime-only exclusion, saveable inclusion, replicated inclusion with deterministic field order checks.

Integration tests:
- `engine_component_catalog_bootstrap_smoke_test`:
  - bootstrap descriptor registration for engine + game components and deterministic startup ordering.
- `engine_component_save_load_version_smoke_test`:
  - save/load round-trip with one forward-compatible schema migration path.

Validation gate for this slice:
- integrate these tests into existing engine test wrapper coverage used by CI (`./scripts/test-engine-backends.sh`) once implementation lands.

---

## 0B) Physics API Layering Contract (Implementation-Ready First Slice)

## Scope
Define the first engine-owned physics layering contract slice that keeps game code backend-neutral while preserving a clear default path and explicit advanced boundaries.

## Core `BodyId`/System Contract Boundaries (authoritative)
Core physics API surface is the mandatory backend-neutral contract. The first slice must expose:
- world lifecycle:
  - `PhysicsSystem::init(config)`,
  - `PhysicsSystem::step(fixed_dt)`,
  - `PhysicsSystem::shutdown()`.
- body lifecycle:
  - `createBody(BodyDesc) -> BodyId`,
  - `destroyBody(BodyId)`,
  - `isValidBody(BodyId) -> bool`.
- state read/write:
  - transform read/write (`setBodyTransform`, `getBodyTransform`),
  - velocity read/write (`set/get linear`, `set/get angular`),
  - body flags (`set/get gravity enabled`, lock constraints under declared policy).
- query primitives:
  - `raycastClosest`,
  - minimal overlap query primitive(s) defined by engine contract.

`BodyId` contract rules:
- `BodyId` is opaque and engine-owned; callers cannot infer backend handles from it.
- stale/invalid `BodyId` use must never crash; operations return deterministic failure.
- `BodyId` lifetime is bound to current physics world instance; ids are invalid after world shutdown/reinit.
- backend-specific actor/body pointers or ids must never cross engine/game boundary.

System boundary rules:
- only `PhysicsSystem` owns backend interaction and stepping.
- game and other engine systems consume physics via the `PhysicsSystem` contract only.
- fixed-step timing ownership remains with engine scheduler/loop; physics callers do not drive backend frame timing directly.

## Optional 95%-Path Facade Boundaries
Optional facade APIs provide common-path ergonomics without expanding core semantics:
- convenience spawn helpers (static body, dynamic body, common shape presets),
- common query helpers (ground probe, forward probe),
- default material/constraint presets from config.

Facade boundaries:
- facade must compile to core `PhysicsSystem` calls only (no backend bypass).
- facade cannot introduce new backend-dependent behavior or handle types.
- facade defaults must be deterministic and documented.

## Override and Extension Limits (5% path)
- advanced/override APIs must live in a separate explicit contract surface (for example `PhysicsAdvanced`), not mixed into default facade.
- overrides are capability-gated and backend-neutral at the call boundary.
- extension points may add advanced options via explicit structs, but cannot weaken core safety/lifecycle guarantees.
- disallowed for first slice:
  - direct backend SDK access from game code,
  - backend-specific branching in gameplay logic,
  - alternate physics-step ownership outside engine fixed-step loop.

## Validation and Failure Rules (mandatory)
Startup validation:
- invalid physics backend selection fails fast before first simulation tick.
- missing required physics config for selected backend fails startup with actionable diagnostics.

Runtime validation:
- operations on invalid/stale `BodyId` return deterministic failure/no-op and emit high-signal diagnostics in debug/test builds.
- queries with invalid parameters (for example non-finite ray data) return deterministic failure and do not mutate world state.
- creation requests violating contract invariants (invalid shape/mass/flag combinations) fail deterministically and do not allocate partial backend state.
- backend failure paths map to contract-level failures; raw backend exceptions/error objects must not leak through public API.

Failure diagnostics must include:
- operation name,
- `BodyId` (if applicable),
- reason category (`invalid_id`, `invalid_input`, `backend_unavailable`, `unsupported_capability`),
- phase/frame context when relevant.

## Acceptance Criteria (first implementation slice)
- Core `BodyId` lifecycle and state/query operations are enforced through a backend-neutral contract.
- `BodyId` validity and stale-id behavior are deterministic and identical at contract level across compiled backends.
- Optional 95%-path facade is bounded to common helpers and does not bypass core contract.
- Override/advanced path is explicitly separated and capability-gated.
- Validation/failure behavior is deterministic, actionable, and non-leaky (no backend object exposure).

## Required Tests (first implementation slice)
Unit tests:
- `physics_body_id_lifecycle_contract_test`:
  - create/destroy/validity behavior, stale-id rejection, world-reinit invalidation.
- `physics_core_state_query_contract_test`:
  - deterministic transform/velocity/gravity/lock semantics at contract level.
- `physics_invalid_input_failure_contract_test`:
  - invalid ray/query/body descriptors fail deterministically with no partial mutation.
- `physics_facade_boundary_contract_test`:
  - facade helpers map to core contract semantics and reject backend-specific options.
- `physics_override_boundary_contract_test`:
  - advanced/capability-gated path acceptance and forbidden backend-leak path rejection.

Parity tests:
- extend existing backend parity coverage to assert `BodyId` lifecycle and failure semantics match for all compiled physics backends.

Integration tests:
- `engine_fixed_step_physics_contract_smoke_test`:
  - verifies physics stepping ownership stays engine-driven and deterministic under fixed-step loop integration.

Validation gate for this slice:
- integrate these tests into existing engine backend wrapper coverage used by CI (`./scripts/test-engine-backends.sh`) once implementation lands.

---

## 0C) Audio API Layering Contract (Implementation-Ready First Slice)

## Scope
Define the first engine-owned audio layering contract slice that keeps game code backend-neutral while guaranteeing deterministic request/voice behavior.

## Core Deterministic Request/Voice Contract Boundaries (authoritative)
Core audio API surface is the mandatory backend-neutral contract. The first slice must expose:
- system lifecycle:
  - `AudioSystem::init(config)`,
  - `AudioSystem::update(frame_dt)`,
  - `AudioSystem::shutdown()`.
- listener state:
  - explicit listener transform/update entry points,
  - deterministic sanitization behavior for non-finite listener values.
- playback requests:
  - `playOneShot(PlayRequest)`,
  - `startVoice(PlayRequest) -> VoiceId`,
  - `stopVoice(VoiceId)`,
  - `setVoiceGain/Pitch/Looping/Position(...)` where supported by contract.
- voice validity and query:
  - `isValidVoice(VoiceId) -> bool`,
  - minimal state queries required for deterministic control-flow decisions.

Request/voice contract rules:
- `VoiceId` is opaque and engine-owned; no backend handle leakage.
- identical valid requests under identical starting state produce contract-equivalent outcomes across backends.
- invalid requests (non-finite gain/pitch/position or missing required assets) fail deterministically and do not allocate hidden voice state.
- operations on stale/invalid `VoiceId` are deterministic no-op/failure and never crash.
- backend device objects and backend voice objects never cross engine/game boundary.

System boundary rules:
- only `AudioSystem` owns backend device/context lifecycle.
- game and engine systems submit requests through `AudioSystem` contracts only.
- server/headless behavior remains policy-driven by engine config with explicit enablement.

## Optional 95%-Path Facade Boundaries
Optional facade APIs provide common-path ergonomics:
- event-level playback helpers (`playEvent`, `stopEventGroup`),
- default bus routing helpers (`ui`, `world`, `music`, `sfx`),
- default attenuation/pan helpers for positional one-shots,
- engine-managed voice budgeting/priority helpers.

Facade boundaries:
- facade must compile to core `AudioSystem` request/voice primitives only.
- facade cannot introduce backend-specific device/session controls.
- facade defaults must remain deterministic, documented, and test-asserted.

## Override and Extension Limits (5% path)
- advanced/override APIs must live in separate explicit contract surface (for example `AudioAdvanced`), not mixed into default facade.
- advanced features are capability-gated and backend-neutral at call boundary.
- extension points may add advanced option structs, but cannot relax core request/voice determinism guarantees.
- disallowed for first slice:
  - direct backend SDK/device access in game code,
  - backend-specific branching in gameplay sound dispatch,
  - caller-owned backend mixer/device lifetime.

## Validation and Failure Rules (mandatory)
Startup validation:
- invalid audio backend selection fails fast with actionable diagnostics.
- backend initialization failure must map to deterministic contract-level init failure.

Runtime validation:
- non-finite or out-of-contract request parameters fail deterministically and leave voice allocation state unchanged.
- stale/invalid `VoiceId` control calls return deterministic no-op/failure and preserve system stability.
- unsupported optional capabilities fail with explicit `unsupported_capability` diagnostics rather than backend-specific errors.
- backend failure paths must be mapped to contract-level failures; no raw backend errors exposed to game consumers.

Failure diagnostics must include:
- operation name,
- `VoiceId` (if applicable),
- reason category (`invalid_id`, `invalid_input`, `backend_unavailable`, `unsupported_capability`),
- frame/tick context when relevant.

## Acceptance Criteria (first implementation slice)
- core request/voice lifecycle and listener update semantics are enforced through backend-neutral contracts.
- request rejection and voice-id invalidation behavior are deterministic and contract-equivalent across compiled backends.
- optional 95%-path facade is bounded to event/bus/default helpers and does not bypass core contract.
- override/advanced path is explicitly separated and capability-gated.
- validation/failure behavior is deterministic, actionable, and backend-object-leak free.

## Required Tests (first implementation slice)
Unit tests:
- `audio_request_validation_contract_test`:
  - deterministic rejection of non-finite/invalid request parameters with no hidden voice allocation.
- `audio_voice_id_lifecycle_contract_test`:
  - start/stop/validity behavior, stale-id handling, and shutdown invalidation.
- `audio_listener_sanitization_contract_test`:
  - deterministic listener sanitization and stable positional request behavior.
- `audio_facade_boundary_contract_test`:
  - event/bus helpers map to core contract semantics and reject backend-specific options.
- `audio_override_boundary_contract_test`:
  - advanced capability-gated path acceptance and forbidden backend-leak path rejection.

Parity tests:
- extend existing audio backend parity/smoke coverage to assert deterministic request rejection and `VoiceId` lifecycle equivalence across compiled audio backends.

Integration tests:
- `engine_audio_loop_contract_smoke_test`:
  - verifies request/voice behavior, listener updates, and deterministic failure handling under real engine loop integration.
- `engine_headless_audio_policy_smoke_test`:
  - verifies explicit server/headless audio policy behavior remains deterministic and opt-in.

Validation gate for this slice:
- integrate these tests into existing engine backend wrapper coverage used by CI (`./scripts/test-engine-backends.sh`) once implementation lands.

---

## 1) Physics Infrastructure

## Current state
- Contract/lifecycle is in place and wired in `EngineApp` + `EngineServerApp`.
- Minimal body-oriented API surface exists.
- Selection and diagnostics are stable.

## Next work
- Implement real Jolt backend behind existing contract first.
- Implement PhysX backend with matching semantics and trace shape.
- Keep gameplay concepts out of physics core.

## Acceptance criteria
- `jolt` backend runs real simulation path.
- `physx` backend runs real simulation path with equivalent contract behavior.
- No call-site changes required in app/server code when swapping backend.

---

## 2) Audio Infrastructure

## Current state
- Contract/lifecycle is in place and wired in `EngineApp` + `EngineServerApp`.
- Listener update hooks exist in client path.
- Selection and diagnostics are stable.

## Next work
- Implement real SDL3audio/miniaudio output paths behind current interface.
- Preserve server headless defaults (`audio` disabled unless explicitly enabled).
- Keep backend-specific details fully hidden from game code.

## Acceptance criteria
- One-shot + looping playback work through engine API on both backends.
- Listener updates affect positional audio behavior.
- Backend swaps do not require game-side API changes.

---

## 3) Core Hardening

## 3A) Fixed-step simulation clock

## Current state
- Implemented and integrated.
- Base trace channel is no longer per-frame noisy.

## Remaining follow-up
- Keep per-frame details leaf-only (`engine.sim.frames`).
- Add optional tests for deterministic step-count behavior under synthetic dt sequences.

## 3B) Content mount/package abstraction

## Current state
- Zip/archive helpers exist for world packaging/extraction.
- Resolver mount precedence and mount-point matching are implemented.
- Runtime server->client world package send/receive/apply flow is implemented.
- Hash/identity cache-hint handshake is implemented to allow no-payload init when cached package identity matches (`id + revision` with `hash` or `content_hash`).
- Init payload now includes manifest summary metadata (`world_manifest_hash`, `world_manifest_file_count`) as groundwork for incremental patch transfer.
- Init payload now includes manifest entry list metadata (`world_manifest[]`: path/size/hash) for file-level delta planning.
- Client now persists per-server manifest entries and emits trace-only manifest diff-plan summaries to guide future patch/chunk transfer work.
- Join handshake now also carries cached manifest summary hints (`cached_world_manifest_hash`, `cached_world_manifest_file_count`) for upcoming delta negotiation.
- Join handshake now also carries cached manifest entry hints (`cached_world_manifest[]`: path/size/hash) for file-level delta planning.
- Server cache-hit/no-payload init decision now also accepts matching manifest summary (`manifest_hash` + file-count) alongside package/content hash.
- Client no-payload cache validation now also accepts matching manifest summary (`manifest_hash` + file-count) alongside package/content hash.
- Server may omit `world_manifest[]` entries on cache-hit with matching manifest summary, and client reuses cached manifest sidecar when summary metadata matches.
- Runtime chunk transfer path (`world_transfer_begin/chunk/end`) is active for cache-miss full package delivery; cache-hit still uses no-payload init.
- Server now emits trace-only manifest diff-plan summaries (from client-cached manifest entries vs authoritative manifest) as groundwork for file-level delta transfer.
- Runtime manifest-driven delta transfer path is active:
  - server may select `chunked_delta` and stream delta archive payloads when cached manifest overlap exists and delta is smaller than full package,
  - client applies delta archive over cached base package and promotes it as the new revision cache.
- Client world-package apply safety is now hardened for both transfer modes:
  - full/delta payloads are applied in staging and verified against transfer metadata before activation,
  - activation uses atomic promote/rollback to avoid replacing a known-good cached package with a partial/bad apply.
- Added integration test coverage for rollback safety on corrupted delta metadata:
  - `src/game/tests/client_world_package_safety_integration_test.cpp`
  - validates failed world update keeps prior cached identity/package active.
- See `docs/projects/content-mount.md` for current in-progress status, locked semantics, and phased execution.

## Next work
- Extend cache/revision identity contract beyond current baseline (`world_id/world_revision` metadata + client cache identity checks are implemented).
- Harden archive handling further (additional extraction constraints and cache retention telemetry/policy tuning).
- Evolve current delta-archive transfer toward direct file/object delta transport and harden chunk transfer (resume/retry/integrity controls).

## Acceptance criteria
- Current filesystem loading remains unchanged from user perspective.
- Reconnect to unchanged world can avoid full package transfer.
- Package apply path is robust against malformed/untrusted archives.

## 3C) Backend capability + selection contract

## Current state
- Render/physics/audio contracts are aligned and traced.
- UI runtime selection is working, but keep parity checks in matrix validation.
- UI backend policy is choice-based: game teams pick ImGui or RmlUi as primary; engine support for both does not imply forced UI abstraction.

## Remaining follow-up
- Keep help text/config keys/trace wording consistent as backend matrix expands.
- Validate all supported runtime override combinations in `build-dev`.

## 3D) Host/CLI Boundary Cleanup (Deferred)

## Context
- Current backend selection parsing already lives in game host/runtime code (client/server), which is correct.
- One boundary leak remains: engine/common data-dir override logic currently parses `argc/argv` directly.

## Why this matters
- Engine libraries should not need to know about command-line shape.
- CLI is a host concern; engine should receive resolved settings via config/service contracts.
- Keeping this boundary clean makes engine reuse easier (embedded host, tools, tests, non-CLI launchers).

## Deferred target
- Move direct CLI parsing (`-d`, `-c`) out of engine/common and into host-layer bootstrap code.
- Replace argv-based engine helper usage with a host-provided data/config override struct or service call.
- Preserve existing resolution precedence and behavior while moving ownership to host code.

## 4) Renderer Feature Capability Envelope (Rewrite-Owned)

## Objective
Achieve renderer feature parity with the rewrite-owned capability envelope while preserving `m-rewrite` architecture decisions and subsystem boundaries.

## Capability targets (authoritative)
- Material shading contract completeness:
  - engine-owned material semantics are consumed consistently in BGFX and Diligent.
- Shadowing pipeline completeness:
  - directional shadow pass + sampling path remains bounded, deterministic, and backend-parity aligned.
- Environment/lighting completeness:
  - environment/sky/IBL contracts are explicit and consumed consistently across backends.
- Material texture-set lifecycle completeness:
  - beyond albedo, texture-set ingestion/sampling/fallback behavior is contract-owned and parity-tested.
- Renderer/UI coupling boundary completeness:
  - renderer-side UI draw/depth integration behavior is explicit, bounded, and does not violate UI ownership boundaries.
- Renderer observability/guardrail completeness:
  - direct/fallback readiness semantics and asset-integrity/packaging guardrails are deterministic and test-backed.
- Platform presentation-path completeness:
  - backend presentation path behavior (for example Linux X11/Wayland) is tracked and stabilized behind backend layers.

## Rules
- Do not port by file shape; port by capability and behavior.
- Keep backend-specific code inside backend layers.
- Keep engine/game boundaries strict while expanding renderer contracts.

## Acceptance criteria
- Missing renderer features are tracked as explicit capabilities from the list above.
- BGFX and Diligent remain behaviorally aligned as capabilities are added.
- Features integrate through engine-owned contracts, not ad-hoc game/backend coupling.

---

## Delegation Readiness Checklist
Ready for broad multi-agent parallelization when all are true:
1. UI project file and active integration tracking docs are current.
2. Physics/audio backends satisfy their stub-closure criteria (sections `0B`/`0C` + Stage 3-6 closeouts) behind stable engine contracts.
3. Fixed-step simulation remains stable and trace-disciplined.
4. Content mount abstraction exists and preserves current filesystem behavior.
5. Backend selection semantics are consistent across render/ui/physics/audio.
6. Base trace channels stay readable; frame-level noise remains leaf-channel only.

---

## References
- UI engine project: `docs/projects/ui-engine.md`
- Gameplay migration project (includes netcode lane): `docs/projects/gameplay-migration.md`
- Engine backend testing governance: `docs/foundation/governance/engine-backend-testing.md`
- Testing/CI governance: `docs/foundation/governance/testing-ci-governance.md`
- Delegation guidance: `docs/foundation/policy/execution-policy.md`
- Root rewrite guide: `AGENTS.md`
