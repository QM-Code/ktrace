# Network Backend Encapsulation

## Project Snapshot
- Current owner: `specialist-network-backend-encapsulation`
- Status: `in progress (P1 medium-high; Slice 1 docs+inventory complete on 2026-02-11; Slice 2 queued)`
- Immediate next task: execute Slice 2 (runtime boundary migration) to remove game-side `CreateEnetServerEventSource`/`enet_event_source.*` references behind backend-neutral contracts.
- Strategic alignment: `shared unblocker` for `m-dev` parity + `KARMA-capability-ready` architecture by advancing engine-owned networking defaults and removing backend leakage from game paths without changing gameplay/protocol semantics.
- Validation gate: docs-only slices run `./docs/scripts/lint-project-docs.sh`; code-touching slices must pass `./bzbuild.py -c` plus `./scripts/test-server-net.sh <build-dir>` in both assigned engine-network-foundation build dirs.

## Mission
Complete network backend encapsulation so game code consumes only `karma::network` contracts and never directly references `ENet` APIs/types/includes/link wiring.

## Why This Is Separate
Runtime transport ownership has mostly moved into engine contracts; this track finishes boundary cleanup (including tests/build wiring) without changing gameplay or protocol semantics.

## Priority Directive (2026-02-11)
- This is medium-high priority (P1) and should run immediately after active P0 network slice continuity is preserved.
- Target state:
  - `src/game/*` has zero direct `ENet` API/type/include usage,
  - `ENet` remains an engine-internal backend implementation selected via transport contracts.

## Owned Paths
- `docs/projects/network-backend-encapsulation.md`
- `m-rewrite/src/game/CMakeLists.txt`
- `m-rewrite/src/game/server/net/*`
- `m-rewrite/src/game/client/net/*`
- `m-rewrite/src/game/tests/*` (network-related tests)
- `m-rewrite/src/engine/network/*` (only when required for interface/test harness support)
- `docs/projects/ASSIGNMENTS.md`

## Interface Boundaries
- Engine owns:
  - transport backend implementation details (`ENet` and future backend adapters),
  - backend registration/selection and backend-facing lifecycle behavior,
  - generic network test fixtures/harness utilities.
- Game owns:
  - protocol schema and payload semantics,
  - gameplay interpretation and rules.
- Coordinate before changing:
  - `docs/projects/engine-network-foundation.md`
  - `docs/projects/server-network.md`
  - `docs/projects/gameplay-netcode.md`

## Non-Goals
- Do not change `src/game/protos/messages.proto`.
- Do not change `src/game/net/protocol.hpp` or `src/game/net/protocol_codec.cpp`.
- Do not change gameplay semantics in `src/game/*`.
- Do not introduce a new runtime transport backend unless explicitly scoped by a follow-on slice.

## Current Candidate Cleanup Targets
1. Remove direct `ENet` usage from game-side integration tests under `src/game/tests/enet_*` by moving backend-specific fixture behavior into engine test support and/or backend-agnostic contract tests.
2. Remove direct `ENet` link wiring from `src/game/CMakeLists.txt` where game targets should only depend on engine contracts.
3. Ensure game server/client runtime paths remain contract-only (`CreateServerTransport`/`CreateClientTransport`) with no backend includes/types.
4. Keep backend selection config (`network.*TransportBackend`) game-consumable while backend implementation remains engine-internal.

## Slice 1 Results (Docs+Inventory Only, 2026-02-11)
- Scope executed: docs/inventory only (no code/build/test wrapper changes outside required docs lint).
- Inventory commands executed from `m-rewrite/`:
  - `rg -n "ENet|enet_|<enet/enet.h>|ENET_" src/game`
  - `rg -n "ENet|enet_|<enet/enet.h>|ENET_" src/game/CMakeLists.txt src/game/tests src/game/client src/game/server`
  - supplemental completeness checks:
    - `rg -l "ENet|enet_|<enet/enet.h>|ENET_" src/game | sort`
    - `rg -n "<enet/enet.h>|#include <enet|include \"enet" src/game`
    - `rg -n "\benet\b|ENet|ENET_" src/game`
- Inventory summary: `9` files with direct ENet references and `272` matching hits (including symbol names, string literals, CMake target/link wiring, and raw ENet C API usage).

### ENet Inventory Map
| Path | Symbol/Use | Classification | Planned Destination/Action |
|---|---|---|---|
| `src/game/CMakeLists.txt` | Direct runtime linkage (`target_link_libraries(bz3|bz3-server ... enet_static/enet)`) | `remove` | Remove direct ENet linkage from game runtime targets; keep backend linkage engine-internal via `karma_engine_core`/`karma_engine_client` dependencies. |
| `src/game/CMakeLists.txt` | Test linkage to `enet_static/enet` for `server_net_contract_test`, `enet_*` tests, `client_world_package_safety_integration_test` | `remove` | Move backend fixture ownership to engine test support and remove ENet link blocks from game CMake. |
| `src/game/CMakeLists.txt` | ENet-specific test target names (`enet_environment_probe_test`, `enet_loopback_integration_test`, `enet_multiclient_loopback_test`, `enet_disconnect_lifecycle_integration_test`) and direct compile of `server/net/enet_event_source.cpp` | `migrate` | Rename/re-scope tests to contract intent (not backend name) and compile backend-neutral event-source entrypoints only. |
| `src/game/server/net/enet_event_source.hpp` | Backend-specific factory `CreateEnetServerEventSource` | `migrate` | Replace with backend-neutral factory contract (game-facing name must not expose ENet backend identity). |
| `src/game/server/net/enet_event_source.cpp` | Backend-specific class/function naming (`EnetServerEventSource`, local `enet` variable), ENet-labeled traces | `migrate` | Convert to backend-neutral game-facing naming (or relocate to engine/network boundary) while preserving runtime behavior and protocol/gameplay semantics. |
| `src/game/server/net/event_source.cpp` | Includes/calls ENet-specific factory and emits ENet-specific availability traces | `migrate` | Route through backend-neutral factory and neutral trace wording for game path. |
| `src/game/tests/enet_environment_probe_test.cpp` | Raw ENet C API + types (`<enet.h>`, `enet_initialize`, `ENetHost`, `ENET_EVENT_TYPE_*`, etc.) | `migrate` | Move environment probe coverage under engine-owned network tests/fixtures; remove raw ENet API from game test tree. |
| `src/game/tests/enet_loopback_integration_test.cpp` | Raw ENet client host/peer/event pump and packet send path (`<enet.h>`, `enet_host_*`, `enet_peer_*`, `ENet*`) | `migrate` | Replace raw ENet client harness with engine-owned loopback fixture/contract helpers; keep gameplay/protocol assertions intact. |
| `src/game/tests/enet_multiclient_loopback_test.cpp` | Raw ENet multi-client fixture/control flow and packet I/O (`<enet.h>`, `ENet*`, `enet_*`) | `migrate` | Migrate to engine test support helpers and backend-neutral contracts. |
| `src/game/tests/enet_disconnect_lifecycle_integration_test.cpp` | Raw ENet lifecycle/disconnect fixture (`<enet.h>`, `ENet*`, `enet_*`) | `migrate` | Migrate to engine-owned transport fixture API; retain disconnect lifecycle semantics/assertions. |
| `src/game/tests/client_world_package_safety_integration_test.cpp` | Raw ENet server fixture and packet pump (`<enet.h>`, `ENetHost`, `ENetPeer`, `enet_*`) plus ENet-initialization helper | `migrate` | Move raw transport fixture behavior into engine-network test support with backend-neutral hooks for malformed/partial transfer scenarios. |
| `src/game/tests/server_net_contract_test.cpp` | ENet-specific factory include/calls (`server/net/enet_event_source.hpp`, `CreateEnetServerEventSource`) | `migrate` | Switch to backend-neutral event-source entrypoint naming and remove ENet-specific include dependency from game contract test. |
| `src/game/tests/server_net_contract_test.cpp` | Backend-id compatibility assertions for `"enet"` and `ServerTransportBackend::Enet` | `retain-with-rationale` | Keep explicit backend-id compatibility assertions while `network.ServerTransportBackend` accepts explicit backend ids; revisit after engine-network foundation defines deprecation/alias policy. |

## Validation
From `m-rewrite/`:

```bash
./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio
./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio
./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio
./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio
```

Docs lint (from repository root):

```bash
./docs/scripts/lint-project-docs.sh
```

## Trace Channels
- `net.client`
- `net.server`
- `engine.server`

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
2. Confirm boundary goal: zero direct `ENet` usage under `src/game/*`.
3. Execute exactly one slice from the queue below.
4. Run required validation.
5. Update this file and `docs/projects/ASSIGNMENTS.md` in the same handoff.

## Slice Queue
1. Slice 1 (docs+inventory only):
   - enumerate all remaining direct `ENet` references under `src/game/*`,
   - classify each as remove/migrate/retain-with-rationale,
   - define exact slice plan with acceptance gates.
   - Status: `Completed (2026-02-11)`.
2. Slice 2 (runtime boundary migration):
   - replace game-facing ENet-specific event-source naming (`CreateEnetServerEventSource`, `enet_event_source.*`) with backend-neutral contracts in `src/game/server/net/*`,
   - preserve protocol/gameplay semantics and existing runtime behavior.
   - Acceptance gates:
     - `./bzbuild.py -c build-sdl3-bgfx-jolt-rmlui-miniaudio`
     - `./bzbuild.py -c build-sdl3-bgfx-physx-rmlui-miniaudio`
     - `./scripts/test-server-net.sh build-sdl3-bgfx-jolt-rmlui-miniaudio`
     - `./scripts/test-server-net.sh build-sdl3-bgfx-physx-rmlui-miniaudio`
     - docs updates in this file + `docs/projects/ASSIGNMENTS.md`.
   - Status: `Queued (ready)`.
3. Slice 3 (game test fixture migration):
   - eliminate raw ENet C API usage from game tests (`src/game/tests/enet_*`, raw fixture paths in `client_world_package_safety_integration_test.cpp`),
   - move backend fixture mechanics into engine-owned test support (`src/engine/network/tests/*`) and keep game tests protocol/gameplay assertion focused.
   - Acceptance gates:
     - both `./bzbuild.py -c` configurations above pass,
     - both `./scripts/test-server-net.sh <build-dir>` wrappers pass,
     - game tests compile without `<enet.h>` includes and without direct `enet_*` calls.
   - Status: `Queued`.
4. Slice 4 (game CMake decoupling + target hygiene):
   - remove direct `enet_static/enet` link wiring from `src/game/CMakeLists.txt`,
   - rename/re-scope backend-named test targets to contract-intent names and keep backend linkage owned by engine/internal test-support targets.
   - Acceptance gates:
     - both `./bzbuild.py -c` configurations above pass,
     - both `./scripts/test-server-net.sh <build-dir>` wrappers pass,
     - `src/game/CMakeLists.txt` has no direct `target_link_libraries(... enet|enet_static)` entries.
   - Status: `Queued`.
5. Slice 5 (closure + guardrails):
   - add guardrails preventing ENet backend leakage back into `src/game/*` (docs + CI/test hook policy),
   - settle retain-vs-migrate decision for explicit `"enet"` backend-id compatibility assertions.
   - Acceptance gates:
     - docs lint passes,
     - selected guardrail check runs in CI/local wrapper path and is documented.
   - Status: `Queued`.

## Current Status
- `2026-02-11`: Project created at medium-high priority to finish ENet encapsulation and remove backend leakage from game-side code/tests/build wiring.
- `2026-02-11`: Slice 1 (docs+inventory) completed; direct ENet references under `src/game/*` fully mapped/classified (`remove`/`migrate`/`retain-with-rationale`) and follow-on gated slice plan defined.
- `2026-02-11`: Strategic track alignment confirmed: this project is a shared unblocker for `m-dev` parity and KARMA-capability-ready architecture by tightening engine-owned networking defaults without protocol/gameplay changes.

## Open Questions
- Which legacy game-side ENet tests should be retired vs rewritten as backend-agnostic contract/integration tests?
- Should we add a hard CI grep gate for `ENet` symbols under `src/game/*` after migration completion?
- Which future backend should be first candidate after encapsulation completeness (`SteamNetworkingSockets`, QUIC, or none until product need)?

## Handoff Checklist
- [ ] Slice scope completed
- [ ] No gameplay/protocol semantic drift
- [ ] Required validation run and summarized
- [ ] Project doc updated
- [ ] `docs/projects/ASSIGNMENTS.md` updated
- [ ] Risks/open questions listed
