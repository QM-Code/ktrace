# Decisions Log

Purpose:
- capture high-impact project decisions that must survive agent/session rotation,
- keep specialist execution aligned with current strategy.

Format:
- `Date` / `Priority` / `Decision` / `Why` / `Impact`

## 2026-02-10 Priority Order

### P0 (Top Priority Now)

#### 1) Promote generic networking boilerplate into engine-owned contracts
- Decision:
  - build an engine-level networking foundation project that moves transport/session lifecycle boilerplate out of game paths while keeping game protocol/rules game-owned.
- Why:
  - game teams should not rebuild transport wiring, connection lifecycle, and reliability scaffolding from scratch.
- Impact:
  - networking defaults become reusable engine capability; server/game tracks consume engine contracts.

#### 2) Keep renderer capability parity as equal top priority
- Decision:
  - continue renderer capability parity as a top track alongside networking-foundation work, with active execution consolidated under `docs/projects/karma-lighting-parity.md`.
- Why:
  - renderer capability gaps are still one of the largest blockers to rewrite-level feature confidence.
- Impact:
  - resource planning treats renderer and engine-network foundation as co-equal top workstreams.

#### 3) Lock the KARMA retirement objective
- Decision:
  - target near-feature-parity planning (implemented or explicitly sketched) so `KARMA-REPO` can be removed without losing direction.
- Why:
  - long-term productivity requires rewrite docs/contracts to stand on their own.
- Impact:
  - parity work must end in rewrite-owned specs and decisions, not KARMA-dependent memory.

### P1 (Next Priority, Immediately After/Alongside P0)

#### 4) System scheduling model: rewrite-native graph over ad-hoc orchestration
- Decision:
  - adopt dependency-aware system scheduling as rewrite architecture, implemented in rewrite-native form.
- Why:
  - subsystem growth needs declarative order/dependency control and composability.
- Impact:
  - future engine lifecycle work should converge toward a system-graph model owned by `src/engine/*`.

#### 5) Expand engine component catalog where it improves default-path productivity
- Decision:
  - add engine-agnostic data components/contracts (for example light/environment/controller-intent style data) without copying exploratory layouts.
- Why:
  - common runtime data should be explicit, reusable, and engine-owned.
- Impact:
  - reduces game-side implicit coupling and repeated setup logic.

#### 6) Physics API direction: keep BodyId core, add optional 95%-path facade
- Decision:
  - retain backend-neutral BodyId/system contract as source-of-truth; add higher-level convenience wrappers only where they reduce repeated common-case code.
- Why:
  - preserves backend clarity while improving ergonomics for common game tasks.
- Impact:
  - no forced wrapper-only model; layered API with clear ownership.

#### 7) Audio API direction: keep request/voice core, allow ergonomic facade
- Decision:
  - retain deterministic request/voice backend-neutral core; optionally add clip-style facade as convenience layer, not replacement.
- Why:
  - behavior determinism and backend parity remain critical while improving usability.
- Impact:
  - user-facing ergonomics can improve without changing backend contract authority.

#### 8) Launch architecture/defaults documentation track without KARMA references
- Decision:
  - create a dedicated project to document rewrite-native structure, abstraction layers, and planned 95%-path defaults for systems/components/physics/audio.
- Why:
  - architecture decisions must be durable and self-contained in rewrite docs.
- Impact:
  - future agents can implement against rewrite docs directly, without external reference lookup.

### P2 (Lower Priority Backlog)

#### 9) Debug/editor overlay remains optional backlog
- Decision:
  - keep debug/editor overlay capabilities as a lower-priority enhancement.
- Why:
  - current top priorities are rendering and engine-network foundations.
- Impact:
  - can be scheduled later with low integration risk.

#### 10) Window backend breadth (GLFW/SDL2 expansion) remains deferred
- Decision:
  - continue SDL3-first and defer GLFW/SDL2 completion unless a concrete blocker emerges.
- Why:
  - backend breadth is not currently on the critical path.
- Impact:
  - window backend expansion stays on TODO without consuming top-priority bandwidth.
  - any non-SDL3 backend proposal must satisfy the admission/conformance policy in `docs/foundation/policy/execution-policy.md`.

## Standing Operating Decisions (Always On)

Note:
- this file records the decision and rationale,
- canonical procedure text is maintained in `docs/AGENTS.md`, `docs/foundation/policy/execution-policy.md`, `docs/foundation/governance/overseer-playbook.md`, and `docs/projects/ASSIGNMENTS.md`.

### A) Documentation hierarchy and startup order are fixed
- Decision:
  - keep startup hierarchy stable with a single quick-entry page (`docs/BOOTSTRAP.md`) feeding the canonical docs stack.
- Why:
  - reduce onboarding ambiguity and duplicate startup instructions.
- Impact:
  - startup guidance remains predictable and easier to maintain.

### B) Delegated build policy is `abuild.py`-only
- Decision:
  - delegated operator flows use `./abuild.py -c -d <build-dir>` by default (omit `-c` only for intentional reuse); no raw `cmake -S/-B` flows.
- Why:
  - prevent toolchain/config drift.
- Impact:
  - consistent build behavior across specialists and sessions.

### C) Isolated build directories and explicit wrapper build dirs are mandatory in parallel work
- Decision:
  - each specialist uses assigned isolated build dirs, and wrapper gates use explicit build-dir args during concurrent work.
- Why:
  - avoid collisions and reduce `build-dev` contention.
- Impact:
  - parallel tracks can validate independently with less flake from shared artifacts.

### D) Default-first engine direction remains non-negotiable
- Decision:
  - engineer for a `~95%` engine default path with a `~5%` override path.
- Why:
  - rewrite goal is rapid game development with a small game-facing API surface.
- Impact:
  - prioritize moving game-agnostic scaffolding into engine-owned contracts.

### E) KARMA remains capability reference only, never a structure template
- Decision:
  - port behavior/capability intent, not upstream file layout.
- Why:
  - preserve rewrite architecture ownership.
- Impact:
  - implementations are evaluated against rewrite contracts and outcomes.

### F) Dual-track planning is mandatory
- Decision:
  - manager/specialist framing must explicitly identify each slice as `m-dev` parity, KARMA intake, or shared unblocker.
- Why:
  - prevent parity-only tunnel vision and structure-copy drift.
- Impact:
  - assignment board and packets stay strategically coherent across rotations.

### G) Overseer checkpointing is mandatory after accepted slice batches
- Decision:
  - overseer owns accepted-slice persistence and must satisfy the checkpoint gate before issuing new assignments.
- Why:
  - ensure recoverability independent of local session continuity.
- Impact:
  - accepted work is routinely persisted and assignment cadence is gated by successful checkpointing.

### H) Overseer startup must refresh KARMA upstream state
- Decision:
  - each overseer startup includes upstream refresh and explicit freshness reporting before target selection.
- Why:
  - intake triage must use current upstream state.
- Impact:
  - adopted-vs-deferred KARMA capability decisions remain explicit and timely.

### I) Audio backend hierarchy is primary+fallback, not co-equal expansion
- Decision:
  - keep `sdl3audio` primary/default and `miniaudio` as fallback plus contract/smoke oracle.
- Why:
  - preserve resilience without diverting top-track bandwidth to co-equal backend expansion.
- Impact:
  - audio work prioritizes default-path correctness while retaining fallback validation coverage.

### J) Local repo `./vcpkg` is mandatory for delegated builds
- Decision:
  - require local `m-rewrite/vcpkg` for delegated `abuild.py` flows; no external vcpkg fallback.
- Why:
  - remove recurring toolchain drift and cross-repo lock contention failures.
- Impact:
  - missing/unbootstrapped local `./vcpkg` is a hard build blocker.
  - specialists escalate this blocker to overseer/human instead of spending coding-slice context on environment bootstrap work.

### K) Named build-slot ownership is mandatory for parallel delegated work
- Decision:
  - managed slot pool is `build-a1` through `build-a8`, and each specialist must use explicit agent identity with lock ownership via `abuild.py` (`--claim-lock` / `--release-lock`).
- Why:
  - prevent cross-agent build-dir collisions and preserve stable per-session build ownership.
- Impact:
  - specialist packets must include agent identity + assigned slots, and lock ownership must match before build/test execution (except overseer-approved `--ignore-lock` emergency use).

### L) Specialists run in pre-provisioned slots; setup chores stay with overseer
- Decision:
  - specialist packets should assume assigned `build-a*` slots are pre-provisioned and should not include setup chores like creating new build dirs or bootstrapping local tooling.
- Why:
  - preserve specialist context for product/code slices and keep environment provisioning centralized.
- Impact:
  - when slot/toolchain readiness is missing, specialists report blockers and stop delegated build/test steps until overseer/human resolves setup.

### M) Lighting/shadow KARMA intake is algorithm/flow-first
- Decision:
  - for lighting/shadow parity slices, when `KARMA-REPO` has a proven implementation, specialists should port the upstream algorithm/integration flow directly by default.
  - adapt that logic to rewrite-owned contracts, naming, and generic-backend seams; do not mirror upstream file layout.
  - any intentional divergence from KARMA algorithm/flow requires explicit rationale plus validation evidence, and unresolved compatibility gaps must be escalated for overseer/human review.
- Why:
  - KARMA already carries mature lighting/shadow implementation experience; re-inventing core algorithms without evidence increases risk and delay.
- Impact:
  - renderer parity packets should bias toward direct capability intake and only deviate when tests/traces prove a better or required rewrite-specific path.
