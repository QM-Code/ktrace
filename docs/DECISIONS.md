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
  - continue `renderer-parity.md` as a top track alongside networking-foundation work.
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

#### 10) Platform backend breadth (GLFW/SDL2 expansion) remains deferred
- Decision:
  - continue SDL3-first and defer GLFW/SDL2 completion unless a concrete blocker emerges.
- Why:
  - backend breadth is not currently on the critical path.
- Impact:
  - platform expansion stays on TODO without consuming top-priority bandwidth.

## Standing Execution Policies (Always On)

### A) Documentation hierarchy and startup order are fixed
- Decision:
  - startup hierarchy is `AGENTS.md` -> `docs/AGENTS.md` -> `docs/projects/<project>.md`.
- Why:
  - reduce onboarding ambiguity.
- Impact:
  - each specialist operates from one canonical project doc.

### B) Delegated build policy is `bzbuild.py`-only
- Decision:
  - delegated specialists must use `./bzbuild.py <build-dir>` and avoid raw `cmake -S/-B`.
- Why:
  - prevent toolchain/config drift.
- Impact:
  - consistent build behavior and fewer setup regressions.

### C) Isolated build directories are required for parallel specialists
- Decision:
  - each active specialist uses assigned build-dir pairs.
- Why:
  - avoid collisions in shared worktrees.
- Impact:
  - parallel tracks can validate independently.

### D) Wrapper scripts accept explicit build dirs and should be used that way in parallel work
- Decision:
  - use `./scripts/test-engine-backends.sh <build-dir>` and `./scripts/test-server-net.sh <build-dir>` during concurrent specialist execution.
- Why:
  - reduce default `build-dev` contention.
- Impact:
  - closeout gates can run with less interference.

### E) Default-first engine direction is non-negotiable
- Decision:
  - engineer for a `~95%` engine default path and `~5%` explicit override path.
- Why:
  - rewrite goal is rapid game development with small game-facing API surface.
- Impact:
  - favor moving game-agnostic behavior into engine-owned contracts.

### F) KARMA is capability reference only, never structure template
- Decision:
  - parity work ports behavior/capability intent, not file layout.
- Why:
  - rewrite architecture ownership must stay clean and maintainable.
- Impact:
  - implementation choices are judged by rewrite contracts and outcomes.

### G) KARMA capability intake runs continuously, but rewrite priorities remain authoritative
- Decision:
  - track significant `KARMA-REPO` feature deltas continuously and convert accepted deltas into rewrite-owned slices, while keeping `m-dev` parity/stability and active rewrite priorities as the scheduling authority.
- Why:
  - rewrite must gain modern engine capability without becoming blocked by upstream cadence or upstream structure.
- Impact:
  - overseer assignments explicitly separate:
    - adopted now (high-value capability deltas),
    - deferred (low-signal or low-priority deltas),
    - rewrite-owned documentation updates that remove KARMA dependency over time.

### H) Dual-track strategy is mandatory in all manager/specialist task framing
- Decision:
  - all top-level coordination docs and specialist task packets must explicitly frame work as `m-dev` parity, `KARMA-REPO` capability intake, or a shared unblocker.
- Why:
  - keeps delegation coherent as multiple specialists rotate and prevents accidental drift toward structure-copying or parity-only tunnel vision.
- Impact:
  - overseer prompts, assignment board updates, and handoff packets must include explicit strategic-track alignment.

### I) Overseer owns commit/push checkpointing after accepted slice batches
- Decision:
  - overseer performs scoped git checkpoint commits and push-to-remote in `m-rewrite` after accepted slice batches.
- Why:
  - project recovery must not depend on local terminal/session continuity.
- Impact:
  - accepted work is frequently persisted (`commit` + `push`) with slice-level traceability,
  - unreviewed/out-of-scope dirty files remain excluded from checkpoint commits.

### J) Checkpoint script is a hard gate before new specialist assignments
- Decision:
  - overseer must run `./scripts/overseer-checkpoint.sh -m "<slice batch summary>" --all-accepted` after accepted slice batches, and no new specialist assignment is allowed until it succeeds.
- Why:
  - convert checkpointing from soft process to a mechanical gate with explicit success/failure.
- Impact:
  - accepted batches are pushed with consistent workflow and verification,
  - assignment cadence is blocked until persistence is confirmed.

### K) Overseer startup must refresh KARMA upstream state
- Decision:
  - at each overseer startup, run `git -C ../KARMA-REPO fetch --all --prune` before proposing targets, then summarize new branches/commits and candidate capability deltas.
- Why:
  - capability-intake planning must be based on current upstream reference state, not stale local snapshots.
- Impact:
  - KARMA intake remains timely and explicit,
  - startup output always includes freshness status (or explicit stale-state warning if fetch fails).
