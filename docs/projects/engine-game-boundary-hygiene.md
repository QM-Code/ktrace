# Engine/Game Boundary Hygiene

## Project Snapshot
- Current owner: `codex`
- Status: `in progress (P2 low-medium; CLI-first override active; Slice 2A/2B/2C/3/4 complete; awaiting next prioritized slice)`
- Immediate next task: hold for reprioritization; Candidate 6 remains deferred pending explicit protocol-boundary review criteria.
- Validation gate: docs lint must pass on every slice; code-touching slices must pass assigned `bzbuild.py` + wrapper gates.

## Mission
Reduce game-side boilerplate/redundant infrastructure code by moving game-agnostic behavior into engine-owned contracts, while keeping gameplay rules and protocol payload semantics game-owned.

## Why This Is Separate
This is boundary-hygiene work that can be delivered in narrow slices without changing gameplay semantics. It directly supports rewrite goals by shrinking repetitive game scaffolding and strengthening engine ownership.

## Priority Directive (2026-02-11)
- This project is low-medium priority (P2) and should run behind active P0 tracks (`renderer-parity`, `engine-network-foundation`).
- Apply default-first policy: move generic behavior into engine only when semantics remain contract-safe.
- Preserve strict ownership boundaries:
  - engine owns lifecycle/bootstrap/config scaffolding helpers,
  - game owns BZ3 rules, protocol payload semantics, and content decisions.

## Owned Paths
- `docs/projects/engine-game-boundary-hygiene.md`
- Candidate integration points:
  - `m-rewrite/src/game/client/bootstrap.cpp`
  - `m-rewrite/src/game/server/bootstrap.cpp`
  - `m-rewrite/src/game/client/cli_options.cpp`
  - `m-rewrite/src/game/server/cli_options.cpp`
  - `m-rewrite/src/game/client/net/client_connection.cpp`
  - `m-rewrite/src/game/server/net/transport_event_source.cpp`
  - `m-rewrite/src/game/server/runtime.cpp`
- Future engine-owned landing zones (to be decided in-slice):
  - `m-rewrite/src/engine/*`
  - `m-rewrite/include/karma/*`

## Interface Boundaries
- Allowed engine extraction:
  - bootstrap/config validation scaffolding,
  - shared CLI parsing scaffolding,
  - transport backend selection/config mapping helpers (not protocol behavior),
  - generic reconnect policy config mapping helpers.
- Must remain game-owned:
  - protocol schema/payload semantics,
  - gameplay rules/state semantics,
  - game-specific message handling.
- Coordinate before changing:
  - `docs/archive/engine-network-foundation-completed-2026-02-12.md`
  - `docs/projects/core-engine-infrastructure.md`
  - `docs/projects/testing-ci-docs.md`

## Non-Goals
- Do not change `src/game/protos/messages.proto`.
- Do not change `src/game/net/protocol.hpp` or `src/game/net/protocol_codec.cpp`.
- Do not move gameplay semantics into engine systems.
- Do not widen scope into renderer/physics/audio feature work.

## Operator Focus Override (2026-02-12)
- Human-directed priority override for this project:
  - execute command-line extraction before remaining lower-priority hygiene slices,
  - centralize engine-controlled options (`-h/--help`, backend flags, shared logging/trace/data-dir/config switches) in engine-owned contracts,
  - keep game-specific options additive and easy to register,
  - make backend option exposure build-aware (show/accept only options that make sense for the current build).
- This override changes near-term execution order inside this project only; it does not change global rewrite P0 priorities.

## Leakage Inventory Refresh (2026-02-12)
| Area | Current touchpoints | Leakage / duplication finding |
|---|---|---|
| Shared CLI parsing primitives | `src/game/client/cli_options.cpp`, `src/game/server/cli_options.cpp` | Repeated `RequireValue`, bool/port parse, `--trace` validation, `--flag=value` handling, help/error flow. |
| Engine backend flags declared in game help/parser | `src/game/client/cli_options.cpp`, `src/game/server/cli_options.cpp` | Engine-controlled backend choices are hard-coded in game code. |
| Backend resolution/compiled-backend checks in game runtime | `src/game/client/main.cpp`, `src/game/server/runtime.cpp` | Repeated backend parse/validation/resolution logic should be engine-owned. |
| Platform backend override in game CLI | `src/game/client/bootstrap.cpp`, `src/game/client/cli_options.cpp` | Game still parses `--backend-platform` even though current build policy is SDL3-only. |

## CLI Engineization Objectives (Operator-Aligned)
1. Engine subsumes shared boilerplate for command-line parsing initialization.
2. Engine-owned flags and help live in engine code (including engine build-dependent backend flags).
3. Client/server binaries instantiate parsing through `karma` interfaces and inherit engine options automatically.
4. Game code adds game-specific options through a small additive extension interface.
5. Engine parser/help behavior is build-aware:
   - when a backend family has one compiled option, do not advertise override flags for that family,
   - strict mode target for this track: backend override flags with no meaningful choice are not recognized.

## CLI Design Decisions (2026-02-12)
- Place CLI contracts under `karma::app` (not `karma::common`) because option surfaces are app-lifecycle orchestration, not generic utility.
- Add engine presets for client/server option sets, plus additive game-option registration hooks.
- Keep protocol/gameplay semantics game-owned:
  - engine parser returns parsed game option values to game code,
  - engine does not absorb game rule interpretation.
- Use existing engine backend discovery sources (`CompiledBackends()` in renderer/physics/audio) to drive help and acceptance behavior.
- Treat platform backend as compile-policy constrained:
  - current repo policy is SDL3-only, so no user-facing platform override in the engine preset unless multi-platform builds are reintroduced.

## Slice 1 Results (Docs-Only Inventory + Extraction Plan, 2026-02-11)
- Scope executed: docs-only classification and extraction planning.
- Candidate decision summary: `extract-to-engine=5`, `keep-in-game=0`, `defer-with-rationale=1`.

### 1) Decision Table (Every Listed Candidate)
| Candidate | Current touchpoints | Classification | Rationale |
|---|---|---|---|
| `1. Duplicate bootstrap setup paths` | `src/game/client/bootstrap.cpp`, `src/game/server/bootstrap.cpp` | `extract-to-engine` | Data-path spec + config-layer initialization are game-agnostic startup scaffolding and duplicated with near-identical structure. |
| `2. Duplicate CLI scaffolding` | `src/game/client/cli_options.cpp`, `src/game/server/cli_options.cpp` | `extract-to-engine` | Shared option parsing primitives (`RequireValue`, bool/port parse, `--trace` validation, `--flag=value` handling) are repeated and policy-agnostic. |
| `3. Repeated transport-backend config parse/plumbing` | `src/game/client/net/client_connection.cpp`, `src/game/server/net/transport_event_source.cpp` | `extract-to-engine` | Backend-name parsing + config-to-transport-config wiring is transport-contract plumbing and should live with engine transport contracts. |
| `4. Repeated strict required-config validation/error policy` | `src/game/client/bootstrap.cpp`, `src/game/server/runtime.cpp` | `extract-to-engine` | Strict-vs-nonstrict issue emission + terminal-failure decision is reusable config-validation policy scaffolding, not gameplay logic. |
| `5. Client reconnect policy config mapping` | `src/game/client/net/client_connection.cpp` | `extract-to-engine` | Config key reads for reconnect attempts/backoff/timeout map directly to `ClientTransportConnectOptions` and fit engine network defaults. |
| `6. World/package transfer assembler` | `src/game/client/net/client_connection.cpp` | `defer-with-rationale` | This path is tightly coupled to world/package protocol payload semantics and cache behavior; defer until a later, explicit protocol-boundary review proves safe extraction. |

### 2) Proposed Engine Landing Zones + Helper/Contract Names
| Candidate(s) | Proposed engine landing zone | Proposed helper/contract names |
|---|---|---|
| `1` | `include/karma/app/bootstrap_scaffold.hpp` + `src/engine/app/bootstrap_scaffold.cpp` | `karma::app::BootstrapConfigSpec`, `karma::app::InitializeDataAndConfig(...)` |
| `2` | `include/karma/app/cli_parse_scaffold.hpp` + `src/engine/app/cli_parse_scaffold.cpp` | `karma::app::CliParseCommon`, `karma::app::ParseBoolOption(...)`, `karma::app::ParseUInt16Port(...)`, `karma::app::RequireTraceChannels(...)` |
| `3` | `include/karma/network/transport_config_mapping.hpp` + `src/engine/network/transport_config_mapping.cpp` | `karma::network::ResolveClientTransportConfigFromConfig(...)`, `karma::network::ResolveServerTransportConfigFromConfig(...)` |
| `4` | `include/karma/common/config_validation_policy.hpp` + `src/engine/common/config_validation_policy.cpp` | `karma::config::ApplyRequiredConfigValidationPolicy(...)` |
| `5` | `include/karma/network/client_reconnect_policy.hpp` + `src/engine/network/client_reconnect_policy.cpp` | `karma::network::ReadClientReconnectPolicyFromConfig(...)`, `karma::network::ApplyReconnectPolicyToConnectOptions(...)` |

### 3) Slice 2/3/4 Acceptance Gates (Explicit Semantic-Drift Checks; legacy plan baseline)
| Slice | Acceptance gates |
|---|---|
| `Slice 2 (bootstrap/config scaffolding extraction)` | 1. Shared engine bootstrap helper is introduced and consumed by both `src/game/client/bootstrap.cpp` and `src/game/server/bootstrap.cpp` for duplicated bootstrap/config scaffolding only. 2. Required-config strict/nonstrict behavior remains identical (`strict=true` keeps terminal failure; `strict=false` remains warn-only). 3. Explicit semantic-drift check: no edits to `src/game/protos/messages.proto`, `src/game/net/protocol.hpp`, or `src/game/net/protocol_codec.cpp`; no gameplay rule changes. 4. Validation gates pass: `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio`, `./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio`, `./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio`, `./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio`. |
| `Slice 3 (network config mapping extraction)` | 1. Client/server transport backend config parsing + mapping moves to shared engine-network helper(s), with call-sites updated in `client_connection.cpp` and `transport_event_source.cpp`. 2. Reconnect policy mapping (`maxAttempts`, `initialBackoffMs`, `maxBackoffMs`, `timeoutMs`) moves to engine helper without changing keys/defaults. 3. Explicit semantic-drift check: transport runtime behavior and protocol payload semantics remain unchanged; no edits to `src/game/protos/messages.proto`, `src/game/net/protocol.hpp`, or `src/game/net/protocol_codec.cpp`. 4. Validation gates pass (same dual-build + dual-wrapper commands as Slice 2). |
| `Slice 4 (CLI scaffolding extraction)` | 1. Shared CLI parse scaffolding is introduced and consumed by both `src/game/client/cli_options.cpp` and `src/game/server/cli_options.cpp`. 2. Existing option surface and parse semantics remain stable (same option names, accepted values, and failure behavior). 3. Explicit semantic-drift check: no gameplay/protocol semantic changes; no edits to `src/game/protos/messages.proto`, `src/game/net/protocol.hpp`, or `src/game/net/protocol_codec.cpp`. 4. Validation gates pass (same dual-build + dual-wrapper commands as Slice 2). |

## CLI-First Execution Override (Active Plan, 2026-02-12)
1. `Slice 2A (engine CLI foundation extraction)`:
   - add engine-owned CLI scaffold contracts under `include/karma/app/*` + `src/engine/app/*`,
   - move shared parse primitives/help/error flow and engine-owned options into engine scaffold,
   - include build-aware backend-option exposure policy in scaffold/help generation.
   - Acceptance:
     - client/server game CLI entrypoints consume engine scaffold for shared and engine options,
     - no gameplay/protocol semantic movement,
     - dual-build + dual-wrapper validation passes.
2. `Slice 2B (game extension hook adoption)`:
   - add additive game-option registration and parsed-result hooks for client/server binaries,
   - migrate existing game-specific options to hook-based registration without changing semantics.
   - Acceptance:
     - game option semantics remain stable,
     - engine option semantics/help are centralized in engine,
     - dual-build + dual-wrapper validation passes.
3. `Slice 2C (backend-resolution migration to engine helpers)`:
   - migrate duplicated backend parse/compiled-support validation from `src/game/client/main.cpp` and `src/game/server/runtime.cpp` into engine-owned helpers,
   - keep same config keys/default behavior unless explicitly approved in a dedicated follow-up.
   - Acceptance:
     - no behavior drift for backend selection defaults/error paths,
     - no protocol/gameplay semantic changes,
     - dual-build + dual-wrapper validation passes.
4. Resume remaining planned slices after `2A/2B/2C`:
   - bootstrap/config scaffolding extraction,
   - network config mapping extraction,
   - reconnect helper extraction follow-up.

## Slice 2A Results (Engine CLI Foundation Extraction, 2026-02-12)
- Delivered engine-owned CLI foundation contracts:
  - `include/karma/app/cli_parse_scaffold.hpp`
  - `src/engine/app/cli_parse_scaffold.cpp`
  - `include/karma/app/cli_client_backend_options.hpp`
  - `src/engine/app/cli_client_backend_options.cpp`
- Build integration:
  - `src/engine/CMakeLists.txt` updated to compile/link the new CLI modules in `karma_engine_core` and `karma_engine_client`.
- Game parser integration:
  - `src/game/client/cli_options.cpp` now consumes engine CLI scaffold for common options and engine backend options.
  - `src/game/server/cli_options.cpp` now consumes engine CLI scaffold for common options and engine backend options.
- Build-aware backend option behavior implemented:
  - backend flags for families with <=1 compiled backend are no longer exposed in help and are not recognized by parser (strict-mode posture).
- Scope preserved:
  - no protocol payload/schema file changes (`messages.proto`, `protocol.hpp`, `protocol_codec.cpp` untouched),
  - game-specific options remain game-owned in this slice.
- Validation completed (from `m-rewrite/`):
  - `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio` (pass)
  - `./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio` (pass)
  - `./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio` (pass, `9/9`)
  - `./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio` (pass, `9/9`)

## Slice 2B Results (Game Option Registration Hooks, 2026-02-12)
- Delivered declarative game-option registration API in engine scaffold:
  - `karma::app::CliRegisteredOption`
  - `karma::app::DefineFlagOption(...)`
  - `karma::app::DefineStringOption(...)`
  - `karma::app::DefineUInt16Option(...)`
  - `karma::app::ConsumeRegisteredCliOption(...)`
  - `karma::app::AppendRegisteredCliHelp(...)`
- Game parser migration:
  - `src/game/client/cli_options.cpp` now declares game options via registration helpers instead of manual `if/else` chains.
  - `src/game/server/cli_options.cpp` now declares game options via registration helpers instead of manual `if/else` chains.
- Scope preserved:
  - no protocol payload/schema file changes (`messages.proto`, `protocol.hpp`, `protocol_codec.cpp` untouched),
  - engine options remain engine-owned; game options remain game-owned and additive through hooks.
- Validation evidence (from `m-rewrite/`):
  - `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio` (pass)
  - `./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio` (pass)
  - `./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio` (pass, `9/9`)
  - `./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio` (pass, `9/9`; timeout-race remains known intermittent under heavy contention)

## Slice 2C Results (Backend-Resolution Helper Migration, 2026-02-12)
- Delivered engine-owned backend-resolution helper contracts:
  - `include/karma/app/backend_resolution.hpp`
  - `src/engine/app/backend_resolution.cpp` (physics/audio resolver path for core/server usage)
  - `src/engine/app/backend_resolution_client.cpp` (render/UI resolver path for client usage)
- Build integration:
  - `src/engine/CMakeLists.txt` updated to compile/link the new core + client backend-resolution modules.
- Game runtime integration:
  - `src/game/client/main.cpp` now consumes engine backend-resolution helpers for render/physics/audio/ui resolution.
  - `src/game/server/runtime.cpp` now consumes engine backend-resolution helpers for physics/audio resolution.
- Scope preserved:
  - config keys/defaults preserved (`render.backend`, `physics.backend`, `audio.backend`; default `auto`),
  - explicit CLI precedence preserved (`--backend-*` remains higher priority than config),
  - no protocol payload/schema file changes (`messages.proto`, `protocol.hpp`, `protocol_codec.cpp` untouched).
- Validation evidence (from `m-rewrite/`):
  - `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio` (pass)
  - `./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio` (pass)
  - `./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio` (pass, `9/9`)
  - `./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio` (pass, `9/9`)

## Slice 3 Results (Bootstrap/Config Scaffolding Extraction, 2026-02-12)
- Delivered engine-owned bootstrap scaffolding contracts:
  - `include/karma/app/bootstrap_scaffold.hpp`
  - `src/engine/app/bootstrap_scaffold.cpp`
- Build integration:
  - `src/engine/CMakeLists.txt` updated to compile/link bootstrap scaffolding in `karma_engine_core`.
- Game bootstrap integration:
  - `src/game/client/bootstrap.cpp` now consumes engine scaffolding for logging and data/config initialization.
  - `src/game/server/bootstrap.cpp` now consumes engine scaffolding for logging and data/config initialization.
- Required-config validation policy integration:
  - `src/game/client/bootstrap.cpp` and `src/game/server/runtime.cpp` now consume `karma::app::ReportRequiredConfigIssues(...)` to preserve strict/non-strict issue handling behavior.
- Scope preserved:
  - strict mode still logs `error` and fails startup (`throw` on client, `return 1` on server),
  - non-strict mode still logs `warn` and continues,
  - no protocol payload/schema file changes (`messages.proto`, `protocol.hpp`, `protocol_codec.cpp` untouched).
- Validation evidence (from `m-rewrite/`):
  - `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio` (pass)
  - `./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio` (pass)
  - `./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio` (pass, `9/9`)
  - `./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio` (pass, `9/9`)

## Slice 4 Results (Network Config Mapping Extraction, 2026-02-12)
- Delivered engine-owned network config mapping contracts:
  - `include/karma/network/transport_config_mapping.hpp`
  - `src/engine/network/transport_config_mapping.cpp`
  - `include/karma/network/client_reconnect_policy.hpp`
  - `src/engine/network/client_reconnect_policy.cpp`
- Build integration:
  - `src/engine/CMakeLists.txt` updated to compile/link the new network mapping modules in `karma_engine_core`.
- Game network integration:
  - `src/game/client/net/client_connection.cpp` now consumes:
    - `karma::network::ResolveClientTransportConfigFromConfig(...)`,
    - `karma::network::ReadClientReconnectPolicyFromConfig(...)`,
    - `karma::network::ApplyReconnectPolicyToConnectOptions(...)`,
    - `karma::network::EffectiveClientTransportBackendName(...)`.
  - `src/game/server/net/transport_event_source.cpp` now consumes:
    - `karma::network::ResolveServerTransportConfigFromConfig(...)`,
    - `karma::network::EffectiveServerTransportBackendName(...)`.
- Scope preserved:
  - backend config keys/defaults unchanged (`network.ClientTransportBackend`, `network.ServerTransportBackend`, default `auto`),
  - reconnect config keys/defaults unchanged (`network.ClientReconnect*` fallback to `network.Reconnect*`; defaults `0/250/2000/1000`),
  - protocol/gameplay boundaries unchanged (`messages.proto`, `protocol.hpp`, `protocol_codec.cpp` untouched).
- Validation evidence (from `m-rewrite/`):
  - `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio` (pass)
  - `./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio` (pass)
  - `./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio` (pass, `9/9`)
  - `./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio` (pass, `9/9`)

## Validation
Docs-only slices (from repository root):

```bash
./docs/scripts/lint-project-docs.sh
```

Code-touching slices (from `m-rewrite/`):

```bash
./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio
./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio
./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio
./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio
```

## Trace Channels
- `engine.app`
- `config`
- `net.client`
- `net.server`

## Build/Run Commands
From `m-rewrite/`:

```bash
./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio
./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio
./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio
./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio
```

## First Session Checklist
1. Read `AGENTS.md`, then `docs/AGENTS.md`, then this file.
2. Confirm boundary constraints (engine scaffolding only, no gameplay/protocol semantic movement).
3. Execute exactly one slice from the queue below.
4. Run required validation.
5. Update this file and `docs/projects/ASSIGNMENTS.md` in the same handoff.

## Current Status
- `2026-02-11`: Project created and queued at low-medium priority.
- `2026-02-11`: Initial candidate inventory captured from current game-side duplication hotspots.
- `2026-02-11`: Slice 1 completed (docs-only inventory + extraction plan):
  - all six listed candidates classified with rationale,
  - engine landing zones/helper names defined for extractable candidates,
  - Slice 2/3/4 acceptance gates codified with explicit "no gameplay/protocol semantic drift" checks.
- `2026-02-11`: Project state advanced to Slice 2 readiness.
- `2026-02-12`: Leakage inventory refreshed with explicit `src/game/client/main.cpp` backend-resolution duplication and SDL3-only platform policy context.
- `2026-02-12`: Human-directed CLI-first override accepted; execution order updated to `Slice 2A -> Slice 2B -> Slice 2C` before remaining hygiene slices.
- `2026-02-12`: Slice 2A completed:
  - engine CLI scaffold/contracts landed in `karma::app`,
  - client/server parsers now consume engine-owned common/backend option handlers,
  - backend-option exposure is now build-aware in strict mode for single-backend families,
  - dual-build + dual-wrapper validation passed in assigned network build dirs.
- `2026-02-12`: Slice 2B implementation landed:
  - declarative game-option registration/parse/help helpers added to engine scaffold,
  - client/server game parser boilerplate reduced to option registration tables + shared engine dispatch.
- `2026-02-12`: Slice 2B validation closeout completed:
  - sequential wrapper reruns passed in both assigned build dirs (`9/9` each),
  - timeout-race intermittency remains a known environmental flake under heavy contention and is treated as non-slice-local risk.
- `2026-02-12`: Slice 2C completed:
  - duplicated backend parse/compiled-support validation in `src/game/client/main.cpp` and `src/game/server/runtime.cpp` moved into engine-owned `karma::app` helper modules,
  - resolver behavior parity preserved (same CLI-vs-config precedence, same config keys/defaults, same unsupported-backend error shape),
  - dual-build + dual-wrapper validation passed in assigned network build dirs.
- `2026-02-12`: Slice 3 completed:
  - duplicated logging/data-config bootstrap scaffolding moved into `karma::app::bootstrap_scaffold`,
  - strict/non-strict required-config issue reporting unified via `karma::app::ReportRequiredConfigIssues(...)` with behavior parity preserved for client throw/server return flows,
  - dual-build + dual-wrapper validation passed in assigned network build dirs.
- `2026-02-12`: Slice 4 completed:
  - duplicated client/server transport backend config parsing and reconnect-policy mapping moved into engine-owned `karma::network` helpers,
  - call-sites in `src/game/client/net/client_connection.cpp` and `src/game/server/net/transport_event_source.cpp` migrated with key/default parity preserved,
  - dual-build + dual-wrapper validation passed in assigned network build dirs.

## Slice Queue
1. Slice 1 (docs-only inventory + extraction plan):
   - convert candidate list into keep-in-game vs extract-to-engine decisions,
   - define target engine contracts/helpers and acceptance criteria.
   - Status: `Completed (2026-02-11; docs-only)`.
2. Slice 2A (engine CLI foundation extraction):
   - extract shared CLI primitives/help/errors and engine-owned options into `karma::app`,
   - enforce build-aware backend option exposure behavior.
   - Status: `Completed (2026-02-12)`.
3. Slice 2B (game extension hook adoption):
   - migrate game-specific options to additive engine CLI extension hooks,
   - keep game option names/values/failure behavior unchanged.
   - Status: `Completed (2026-02-12)`.
4. Slice 2C (backend-resolution helper migration):
   - migrate duplicated backend parse/compiled-support checks into engine-owned helpers,
   - keep config keys/default behavior and runtime semantics unchanged.
   - Status: `Completed (2026-02-12)`.
5. Slice 3 (bootstrap/config scaffolding extraction):
   - extract shared bootstrap/config-validation helper path,
   - keep strict/non-strict behavior and message semantics unchanged.
   - Status: `Completed (2026-02-12)`.
6. Slice 4 (network config mapping extraction):
   - extract transport backend and reconnect config mapping helpers,
   - keep protocol and runtime transport semantics unchanged.
   - Status: `Completed (2026-02-12)`.

## Open Questions
- Confirm strict-mode policy finalization for single-backend builds:
  - preferred target is "no backend override flag recognized when there is no meaningful choice,"
  - compatibility fallback is "flag hidden from help but accepted for scripted invocations."
- Confirm whether `--backend-ui` strict-mode behavior should follow the same single-choice policy as renderer/physics/audio (currently yes via build-aware exposure).
- Keep backend config-key ownership in game docs while moving parser/mapping mechanics into engine helpers, unless engine-network ownership requests key contract changes.
- Candidate 6 remains deferred pending explicit protocol-boundary review criteria; no extraction planned in Slice 2/3/4.

## Handoff Checklist
- [ ] Slice scope completed
- [ ] Boundary constraints preserved (no gameplay/protocol semantic drift)
- [ ] Validation run and summarized
- [ ] This file updated
- [ ] `docs/projects/ASSIGNMENTS.md` updated
- [ ] Risks/open questions listed
