# Engine/Game Boundary Hygiene

## Project Snapshot
- Current owner: `codex`
- Status: `in progress (P2 low-medium; Slice 1 docs-only inventory/extraction plan complete; Slice 2 ready)`
- Immediate next task: execute Slice 2 (bootstrap/config scaffolding extraction) using the accepted Slice 1 decisions, helper landing zones, and acceptance gates.
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
  - `docs/projects/engine-network-foundation.md`
  - `docs/projects/core-engine-infrastructure.md`
  - `docs/projects/testing-ci-docs.md`

## Non-Goals
- Do not change `src/game/protos/messages.proto`.
- Do not change `src/game/net/protocol.hpp` or `src/game/net/protocol_codec.cpp`.
- Do not move gameplay semantics into engine systems.
- Do not widen scope into renderer/physics/audio feature work.

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

### 3) Slice 2/3/4 Acceptance Gates (Explicit Semantic-Drift Checks)
| Slice | Acceptance gates |
|---|---|
| `Slice 2 (bootstrap/config scaffolding extraction)` | 1. Shared engine bootstrap helper is introduced and consumed by both `src/game/client/bootstrap.cpp` and `src/game/server/bootstrap.cpp` for duplicated bootstrap/config scaffolding only. 2. Required-config strict/nonstrict behavior remains identical (`strict=true` keeps terminal failure; `strict=false` remains warn-only). 3. Explicit semantic-drift check: no edits to `src/game/protos/messages.proto`, `src/game/net/protocol.hpp`, or `src/game/net/protocol_codec.cpp`; no gameplay rule changes. 4. Validation gates pass: `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio`, `./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio`, `./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio`, `./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio`. |
| `Slice 3 (network config mapping extraction)` | 1. Client/server transport backend config parsing + mapping moves to shared engine-network helper(s), with call-sites updated in `client_connection.cpp` and `transport_event_source.cpp`. 2. Reconnect policy mapping (`maxAttempts`, `initialBackoffMs`, `maxBackoffMs`, `timeoutMs`) moves to engine helper without changing keys/defaults. 3. Explicit semantic-drift check: transport runtime behavior and protocol payload semantics remain unchanged; no edits to `src/game/protos/messages.proto`, `src/game/net/protocol.hpp`, or `src/game/net/protocol_codec.cpp`. 4. Validation gates pass (same dual-build + dual-wrapper commands as Slice 2). |
| `Slice 4 (CLI scaffolding extraction)` | 1. Shared CLI parse scaffolding is introduced and consumed by both `src/game/client/cli_options.cpp` and `src/game/server/cli_options.cpp`. 2. Existing option surface and parse semantics remain stable (same option names, accepted values, and failure behavior). 3. Explicit semantic-drift check: no gameplay/protocol semantic changes; no edits to `src/game/protos/messages.proto`, `src/game/net/protocol.hpp`, or `src/game/net/protocol_codec.cpp`. 4. Validation gates pass (same dual-build + dual-wrapper commands as Slice 2). |

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

## Slice Queue
1. Slice 1 (docs-only inventory + extraction plan):
   - convert candidate list into keep-in-game vs extract-to-engine decisions,
   - define target engine contracts/helpers and acceptance criteria.
   - Status: `Completed (2026-02-11; docs-only)`.
2. Slice 2 (bootstrap/config scaffolding extraction):
   - extract shared bootstrap/config-validation helper path,
   - keep strict/non-strict behavior and message semantics unchanged.
   - Status: `Queued (ready)`.
3. Slice 3 (network config mapping extraction):
   - extract transport backend and reconnect config mapping helpers,
   - keep protocol and runtime transport semantics unchanged.
   - Status: `Queued`.
4. Slice 4 (CLI scaffolding extraction):
   - extract common CLI option parsing scaffolding,
   - keep client/server option semantics unchanged.
   - Status: `Queued`.

## Open Questions
- Confirm final engine-owned helper placement for startup/CLI scaffolding (`karma::app` vs `karma::common`) before Slice 2 implementation.
- Keep backend config-key ownership in game docs while moving parser/mapping mechanics into engine helpers, unless engine-network ownership requests key contract changes.
- Candidate 6 remains deferred pending explicit protocol-boundary review criteria; no extraction planned in Slice 2/3/4.

## Handoff Checklist
- [ ] Slice scope completed
- [ ] Boundary constraints preserved (no gameplay/protocol semantic drift)
- [ ] Validation run and summarized
- [ ] This file updated
- [ ] `docs/projects/ASSIGNMENTS.md` updated
- [ ] Risks/open questions listed
