# Network Backend Encapsulation

## Project Snapshot
- Current owner: `specialist-network-backend-encapsulation`
- Status: `completed (P1 medium-high; Slice 1 + Slice 2 + Slice 3 + Slice 4 + Slice 5 + Slice 6 complete through 2026-02-12)`
- Immediate next task: monitor guardrail enforcement in wrapper/CI flows to keep `src/game/*` backend-token clean.
- Strategic alignment: `shared unblocker` for `m-dev` parity + `KARMA-capability-ready` architecture by advancing engine-owned networking defaults and removing backend leakage from game paths without changing gameplay/protocol semantics.
- Validation gate: docs-only slices run `./docs/scripts/lint-project-docs.sh`; code-touching slices must pass `./scripts/check-network-backend-encapsulation.sh`, `./bzbuild.py -c`, and `./scripts/test-server-net.sh <build-dir>` in both assigned engine-network-foundation build dirs.

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
1. Remove direct `ENet` usage from game-side integration tests under `src/game/tests/transport_*` by moving backend-specific fixture behavior into engine test support and/or backend-agnostic contract tests.
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

## Slice 2 Results (Runtime Boundary Migration, 2026-02-11)
- Scope executed: runtime boundary naming/contracts only for game-facing event source entrypoints (no protocol schema/codec changes, no gameplay behavior changes).
- Completed contract migration:
  - renamed game runtime event-source files:
    - `src/game/server/net/enet_event_source.hpp` -> `src/game/server/net/transport_event_source.hpp`
    - `src/game/server/net/enet_event_source.cpp` -> `src/game/server/net/transport_event_source.cpp`
  - renamed game-facing factory contract:
    - `CreateEnetServerEventSource(uint16_t)` -> `CreateServerTransportEventSource(uint16_t)`
  - updated call-sites/includes and game CMake compile references to consume backend-neutral names.
  - neutralized runtime event-source creation traces in `src/game/server/net/event_source.cpp` (`ENet event source` -> `transport event source`).
- Acceptance gate result: both required `bzbuild.py -c` builds and both required `test-server-net.sh <build-dir>` wrappers passed.

## Slice 3 Results (Game Test Fixture Migration, 2026-02-11)
- Scope executed: migrated raw game-test ENet C API usage into engine-owned network test support contracts without protocol or gameplay semantic changes.
- Engine test-support contract additions (`src/engine/network/tests/loopback_enet_fixture.hpp/.cpp`):
  - `InitializeLoopbackEnet(...)` for shared one-time ENet init.
  - `PumpLoopbackEndpointCapturePayloads(...)` for payload-capturing receive pumps.
  - `DisconnectLoopbackEndpoint(...)`, `GetLoopbackEndpointBoundPort(...)`, and `LoopbackEndpointHasPeer(...)` for backend-neutral fixture control/introspection.
- Game-test migrations completed:
  - `src/game/tests/transport_environment_probe_test.cpp`
  - `src/game/tests/transport_loopback_integration_test.cpp`
  - `src/game/tests/transport_multiclient_loopback_test.cpp`
  - `src/game/tests/transport_disconnect_lifecycle_integration_test.cpp`
  - `src/game/tests/client_world_package_safety_integration_test.cpp`
- Build wiring updates for migration-only scope:
  - Added `network_test_support` linkage + engine test-support include path for migrated game tests in `src/game/CMakeLists.txt`.
  - Kept direct `enet_static/enet` game-test link wiring untouched for Slice 4.
- Verification result:
  - Raw ENet C API/type usage was removed from migrated game tests (`#include <enet.h>`, `ENetHost`/`ENetPeer`/`ENetEvent`, direct `enet_*` calls).
  - Both required `./bzbuild.py -c` validations passed in the assigned build dirs.
  - Required wrapper gates passed in both assigned build dirs.

## Slice 5 Results (Closure + Guardrails, 2026-02-11)
- Scope executed: added anti-regression guardrails for ENet leakage under `src/game/*`, integrated them into wrapper execution, and settled backend-id compatibility assertion policy.
- Guardrail script added:
  - `scripts/check-network-backend-encapsulation.sh`
- Guardrail failure checks (no allowlist used):
  - `#include <enet.h>` / `#include <enet/enet.h>` under `src/game/*`
  - direct ENet C API calls under `src/game/*` (`enet_*(`)
  - ENet C API types under `src/game/*` (`ENetHost`, `ENetPeer`, `ENetEvent`, `ENetAddress`, `ENetPacket`)
  - ENet macro tokens under `src/game/*` (`ENET_*`)
  - direct `target_link_libraries(... enet|enet_static)` in `src/game/CMakeLists.txt`
- Wrapper integration:
  - `scripts/test-server-net.sh` now runs `scripts/check-network-backend-encapsulation.sh` before configure/build/test execution.
- Backend-id compatibility decision (updated in Slice 6):
  - explicit `"enet"` backend-id compatibility assertions were removed from `src/game/tests/server_net_contract_test.cpp` to keep `src/game/*` backend-token agnostic.
  - builtin alias compatibility coverage was moved into engine-owned transport contract coverage (`src/engine/network/tests/server_transport_contract_test.cpp`).

### ENet Inventory Map
| Path | Symbol/Use | Classification | Planned Destination/Action |
|---|---|---|---|
| `src/game/CMakeLists.txt` | Direct runtime linkage (`target_link_libraries(bz3|bz3-server ... enet_static/enet)`) | `remove` | Remove direct ENet linkage from game runtime targets; keep backend linkage engine-internal via `karma_engine_core`/`karma_engine_client` dependencies. |
| `src/game/CMakeLists.txt` | Test linkage to `enet_static/enet` for `server_net_contract_test`, `enet_*` tests, `client_world_package_safety_integration_test` | `remove` | Move backend fixture ownership to engine test support and remove ENet link blocks from game CMake. |
| `src/game/CMakeLists.txt` | ENet-specific test target names (`transport_environment_probe_test`, `transport_loopback_integration_test`, `transport_multiclient_loopback_test`, `transport_disconnect_lifecycle_integration_test`) and direct compile of `server/net/enet_event_source.cpp` | `migrate` | Rename/re-scope tests to contract intent (not backend name) and compile backend-neutral event-source entrypoints only. |
| `src/game/server/net/enet_event_source.hpp` | Backend-specific factory `CreateEnetServerEventSource` | `migrate` | Replace with backend-neutral factory contract (game-facing name must not expose ENet backend identity). |
| `src/game/server/net/enet_event_source.cpp` | Backend-specific class/function naming (`EnetServerEventSource`, local `enet` variable), ENet-labeled traces | `migrate` | Convert to backend-neutral game-facing naming (or relocate to engine/network boundary) while preserving runtime behavior and protocol/gameplay semantics. |
| `src/game/server/net/event_source.cpp` | Includes/calls ENet-specific factory and emits ENet-specific availability traces | `migrate` | Route through backend-neutral factory and neutral trace wording for game path. |
| `src/game/tests/transport_environment_probe_test.cpp` | Raw ENet C API + types (`<enet.h>`, `enet_initialize`, `ENetHost`, `ENET_EVENT_TYPE_*`, etc.) | `migrate` | Move environment probe coverage under engine-owned network tests/fixtures; remove raw ENet API from game test tree. |
| `src/game/tests/transport_loopback_integration_test.cpp` | Raw ENet client host/peer/event pump and packet send path (`<enet.h>`, `enet_host_*`, `enet_peer_*`, `ENet*`) | `migrate` | Replace raw ENet client harness with engine-owned loopback fixture/contract helpers; keep gameplay/protocol assertions intact. |
| `src/game/tests/transport_multiclient_loopback_test.cpp` | Raw ENet multi-client fixture/control flow and packet I/O (`<enet.h>`, `ENet*`, `enet_*`) | `migrate` | Migrate to engine test support helpers and backend-neutral contracts. |
| `src/game/tests/transport_disconnect_lifecycle_integration_test.cpp` | Raw ENet lifecycle/disconnect fixture (`<enet.h>`, `ENet*`, `enet_*`) | `migrate` | Migrate to engine-owned transport fixture API; retain disconnect lifecycle semantics/assertions. |
| `src/game/tests/client_world_package_safety_integration_test.cpp` | Raw ENet server fixture and packet pump (`<enet.h>`, `ENetHost`, `ENetPeer`, `enet_*`) plus ENet-initialization helper | `migrate` | Move raw transport fixture behavior into engine-network test support with backend-neutral hooks for malformed/partial transfer scenarios. |
| `src/game/tests/server_net_contract_test.cpp` | ENet-specific factory include/calls (`server/net/enet_event_source.hpp`, `CreateEnetServerEventSource`) | `migrate` | Switch to backend-neutral event-source entrypoint naming and remove ENet-specific include dependency from game contract test. |
| `src/game/tests/server_net_contract_test.cpp` | Backend-id compatibility assertions for `"enet"` and `ServerTransportBackend::Enet` | `migrated (Slice 6)` | Removed backend-specific assertions from game test tree; moved builtin alias compatibility coverage to engine transport contract tests. |

## Validation
From `m-rewrite/`:

```bash
./scripts/check-network-backend-encapsulation.sh
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
./scripts/check-network-backend-encapsulation.sh
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
   - Status: `Completed (2026-02-11)`.
3. Slice 3 (game test fixture migration):
   - eliminate raw ENet C API usage from game tests (`src/game/tests/transport_*`, raw fixture paths in `client_world_package_safety_integration_test.cpp`),
   - move backend fixture mechanics into engine-owned test support (`src/engine/network/tests/*`) and keep game tests protocol/gameplay assertion focused.
   - Acceptance gates:
     - both `./bzbuild.py -c` configurations above pass,
     - both `./scripts/test-server-net.sh <build-dir>` wrappers pass,
     - game tests compile without `<enet.h>` includes and without direct `enet_*` calls.
   - Status: `Completed (2026-02-11; both required bzbuild + wrapper gates passed)`.
4. Slice 4 (game CMake decoupling + target hygiene):
   - remove direct `enet_static/enet` link wiring from `src/game/CMakeLists.txt`,
   - rename/re-scope backend-named test targets to contract-intent names and keep backend linkage owned by engine/internal test-support targets.
   - Acceptance gates:
     - both `./bzbuild.py -c` configurations above pass,
     - both `./scripts/test-server-net.sh <build-dir>` wrappers pass,
     - `src/game/CMakeLists.txt` has no direct `target_link_libraries(... enet|enet_static)` entries.
   - Status: `Completed (2026-02-11; both required bzbuild + wrapper gates passed; game CMake ENet link grep returned no matches).`
5. Slice 5 (closure + guardrails):
   - add guardrails preventing ENet backend leakage back into `src/game/*` (docs + CI/test hook policy),
   - settle retain-vs-migrate decision for explicit `"enet"` backend-id compatibility assertions.
   - Acceptance gates:
     - docs lint passes,
     - selected guardrail check runs in CI/local wrapper path and is documented.
   - Status: `Completed (2026-02-11; guardrail script added and integrated into test-server-net wrapper).`
6. Slice 6 (game backend-token eradication + compatibility coverage migration):
   - rename ENet-labeled game integration tests/targets to transport-neutral names,
   - remove remaining `enet`/`ENet` tokens from `src/game/*` test/debug paths,
   - move explicit `"enet"` builtin backend-id compatibility assertions from game tests to engine-owned transport contract tests,
   - tighten guardrail checks to fail on any `enet` token or `*enet*` file path under `src/game/*`.
   - Acceptance gates:
     - `rg -n -i "enet" src/game` returns no matches,
     - both `./bzbuild.py -c` configurations above pass,
     - both `./scripts/test-server-net.sh <build-dir>` wrappers pass,
     - docs updates in this file + `docs/projects/ASSIGNMENTS.md`.
   - Status: `Completed (2026-02-12; game tree backend-token clean, compatibility coverage moved engine-side, wrapper/guardrails updated).`

## Current Status
- `2026-02-11`: Project created at medium-high priority to finish ENet encapsulation and remove backend leakage from game-side code/tests/build wiring.
- `2026-02-11`: Slice 1 (docs+inventory) completed; direct ENet references under `src/game/*` fully mapped/classified (`remove`/`migrate`/`retain-with-rationale`) and follow-on gated slice plan defined.
- `2026-02-11`: Slice 2 (runtime boundary migration) completed; game-facing ENet-specific event-source files/factory were migrated to backend-neutral `transport_event_source.*` and `CreateServerTransportEventSource` contracts while preserving runtime behavior.
- `2026-02-11`: Slice 3 (game test fixture migration) completed; raw ENet C API usage was removed from targeted game tests and moved behind engine-owned loopback test-support contracts; both required `bzbuild.py -c` and wrapper gates passed in both assigned build dirs.
- `2026-02-11`: Slice 4 (game CMake decoupling + target hygiene) completed; direct game-side `enet_static/enet` target link wiring was removed from `src/game/CMakeLists.txt` while preserving Slice 3 test behavior and runtime semantics; both required `bzbuild.py -c` and wrapper gates passed, and `rg -n "target_link_libraries\\(.*(enet_static|enet)\\b" src/game/CMakeLists.txt` returned no matches.
- `2026-02-11`: Slice 5 (closure + guardrails) completed; added `scripts/check-network-backend-encapsulation.sh`, integrated it into `scripts/test-server-net.sh`, validated guardrail/build/wrapper/docs gates, and settled backend-id compatibility policy as retain-with-rationale until explicit `"enet"` selection deprecation/removal is approved.
- `2026-02-11`: Strategic track alignment confirmed: this project is a shared unblocker for `m-dev` parity and KARMA-capability-ready architecture by tightening engine-owned networking defaults without protocol/gameplay changes.
- `2026-02-12`: Slice 6 completed; ENet-labeled game test files/targets were renamed to transport-neutral names, remaining backend-specific token mentions were removed from `src/game/*`, explicit `"enet"` builtin backend-id assertions were migrated from `src/game/tests/server_net_contract_test.cpp` to `src/engine/network/tests/server_transport_contract_test.cpp`, and guardrails now fail on any `enet` token/path under `src/game/*`.
- `2026-02-12`: CI now calls `./scripts/check-network-backend-encapsulation.sh` as a standalone pre-test gate in `.github/workflows/core-test-suite.yml`; wrapper integration in `scripts/test-server-net.sh` remains for defense in depth.

## Open Questions
- Which future backend should be first candidate after encapsulation completeness (`SteamNetworkingSockets`, QUIC, or none until product need)?

## Handoff Checklist
- [ ] Slice scope completed
- [ ] No gameplay/protocol semantic drift
- [ ] Required validation run and summarized
- [ ] Project doc updated
- [ ] `docs/projects/ASSIGNMENTS.md` updated
- [ ] Risks/open questions listed
